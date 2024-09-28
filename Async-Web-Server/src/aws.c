// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/eventfd.h>
#include <libaio.h>
#include <errno.h>

#include "aws.h"
#include "utils/util.h"
#include "utils/debug.h"
#include "utils/sock_util.h"
#include "utils/w_epoll.h"

/* server socket file descriptor */
static int listenfd;

/* epoll file descriptor for multiplexing */
static int epollfd;

static io_context_t ctx;

static int aws_on_path_cb(http_parser *p, const char *buf, size_t len)
{
	/*
	 * Callback function for parsing HTTP requests.
	 * Records the requested path in the connection structure.
	 */
	struct connection *conn = (struct connection *)p->data;

	memcpy(conn->request_path, buf, len);
	conn->request_path[len] = '\0';
	conn->have_path = 1;

	return 0;
}

static void connection_prepare_send_404(struct connection *conn)
{
	/* Prepare the connection buffer to send the 404 header. */
	snprintf(conn->send_buffer, sizeof(conn->send_buffer),
		"HTTP/1.1 404 Not Found\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n");

	conn->send_len = strlen(conn->send_buffer);
}

static enum resource_type connection_get_resource_type(struct connection *conn)
{
	const char *static_keyword = "static";
	const char *dynamic_keyword = "dynamic";

	if (strstr(conn->filename, static_keyword) != NULL)
		return RESOURCE_TYPE_STATIC;
	else if (strstr(conn->filename, dynamic_keyword) != NULL)
		return RESOURCE_TYPE_DYNAMIC;
	else
		return RESOURCE_TYPE_DYNAMIC;
}

struct connection *connection_create(int sockfd)
{
	/* Initialize connection structure on given socket. */
	struct connection *conn = malloc(sizeof(*conn));

	conn->sockfd = sockfd;
	memset(conn->recv_buffer, 0, BUFSIZ);
	memset(conn->send_buffer, 0, BUFSIZ);
	conn->state = STATE_INITIAL;
	conn->ctx = 0;

	io_setup(1, &conn->ctx);

	return conn;
}

void connection_start_async_io(struct connection *conn)
{
	/* Open the file for reading */
	 conn->fd = open(conn->filename, O_RDONLY);

	if (conn->fd == -1) {
		perror("Error opening file");
		connection_prepare_send_404(conn);
		return;
	}

	/* Prepare iocb for the asynchronous read operation */
	memset(conn->recv_buffer, 0, BUFSIZ);
	io_prep_pread(&conn->iocb, conn->fd, conn->recv_buffer, BUFSIZ, conn->file_pos);
	conn->piocb[0] = &conn->iocb;

	/* Submit the asynchronous read operation */
	io_submit(conn->ctx, 1, conn->piocb);

	/* Set the connection state to indicate ongoing asynchronous operation */
	conn->state = STATE_ASYNC_ONGOING;
}

void connection_remove(struct connection *conn)
{
	/* Remove connection handler. */
	if (conn->state == STATE_INITIAL) {
		close(conn->sockfd);
		conn->state = STATE_CONNECTION_CLOSED;
		free(conn);
	}
}

void handle_new_connection(void)
{
	static int sockfd;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	struct connection *conn;
	int rc;

	/* Accept new connection. */
	sockfd = accept(listenfd, (SSA *)&addr, &addrlen);

	dlog(LOG_ERR, "Accepted connection from: %s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

	/* Set socket to be non-blocking. */
	int flags = fcntl(sockfd, F_GETFL, 0);

	flags = flags | O_NONBLOCK;
	fcntl(sockfd, F_SETFL, flags);

	/* Instantiate new connection handler. */
	conn = connection_create(sockfd);

	/* Add socket to epoll. */
	rc = w_epoll_add_ptr_in(epollfd, sockfd, conn);

	/* Initialize HTTP_REQUEST parser. */
	http_parser_init(&conn->request_parser, HTTP_REQUEST);
	conn->request_parser.data = conn;
}

int parse_header(struct connection *conn)
{
	/* Parse the HTTP header and extract the file path. */
	/* Use mostly null settings except for on_path callback. */
	http_parser_settings settings_on_path = {
		.on_message_begin = 0,
		.on_header_field = 0,
		.on_header_value = 0,
		.on_path = aws_on_path_cb,
		.on_url = 0,
		.on_fragment = 0,
		.on_query_string = 0,
		.on_body = 0,
		.on_headers_complete = 0,
		.on_message_complete = 0
	};
	return 0;
}

enum connection_state connection_send_static(struct connection *conn)
{
	/* Send static data using sendfile(2). */
	conn->fd = open(conn->filename, O_RDONLY);
	off_t offset = 0;
	ssize_t sent_bytes = 0;

	while ((sent_bytes = sendfile(conn->sockfd, conn->fd, &offset, BUFSIZ)) > 0) {
		if (sent_bytes == -1) {
			dlog(LOG_INFO, "Error in sendfile: %s\n", strerror(errno));
			break;
		}
	}

	if (sent_bytes == -1)
		dlog(LOG_INFO, "Error in sendfile: %s\n", strerror(errno));
	close(conn->fd);
	return STATE_NO_STATE;
}

int connection_send_data(struct connection *conn)
{
	/* May be used as a helper function. */
	/* Send as much data as possible from the connection send buffer.
	 * Returns the number of bytes sent or -1 if an error occurred
	 */
	return -1;
}

int connection_send_dynamic(struct connection *conn)
{
	ssize_t sent_bytes = 0;
	off_t total_file_size = conn->file_size;

	conn->file_pos = 0;
	dlog(LOG_INFO, "file_size %ld", total_file_size);
	do {
		off_t offset = 0;

		sent_bytes = 1;
		connection_start_async_io(conn);
		struct io_event events[1];

		io_getevents(conn->ctx, 1, 1, events, NULL);
		while (sent_bytes > 0 && offset < BUFSIZ) {
			if (conn->state != STATE_ASYNC_ONGOING)
				return -1;
			sent_bytes = send(conn->sockfd, conn->recv_buffer + offset, BUFSIZ, 0);
			offset += sent_bytes;
		}
		total_file_size -= BUFSIZ;

		conn->state = STATE_DATA_SENT;
		conn->file_pos += BUFSIZ;
		close(conn->fd);
	} while (total_file_size > 0);

	return 0;
}

void extract_and_set_filename(struct connection *conn)
{
	const char *request_start = strstr(conn->recv_buffer, "GET /");

	request_start += 5;
	const char *request_end = strstr(request_start, "HTTP");

	size_t filename_length = request_end - request_start;

	snprintf(conn->filename, sizeof(conn->filename), "./%.*s", (int)filename_length, request_start);
	dlog(LOG_INFO, "Length of the extracted filename: %ld\n", strlen(conn->filename));
}

void handle_input(struct connection *conn)
{
	ssize_t bytes_received;
	char buffer[BUFSIZ];
	size_t current_position = 0;

	while (1) {
		bytes_received = recv(conn->sockfd, buffer, BUFSIZ, 0);
		dlog(LOG_INFO, "Number of bytes received: %d\n", (int)bytes_received);

		if (bytes_received <= 0) {
			if (bytes_received == 0)
				dlog(LOG_INFO, "Connection closed by client\n");
			else
				perror("recv");

			handle_output(conn);
			close(conn->sockfd);
			return;
		}
		if (current_position + bytes_received < sizeof(conn->recv_buffer)) {
			memcpy(conn->recv_buffer + current_position, buffer, bytes_received);
			current_position += bytes_received;
			conn->recv_len = current_position;
			conn->recv_buffer[current_position] = '\0';

			dlog(LOG_INFO, "Received string: %s\n", conn->recv_buffer);
			const char *http_end = strstr(conn->recv_buffer, "\r\n\r\n");

			if (http_end != NULL)
				parse_header(conn);
		} else {
			dlog(LOG_INFO, "Received data exceeds buffer size\n");
			handle_output(conn);
			return;
		}
	}
}

void handle_output(struct connection *conn)
{
	ssize_t bytes_sent;

	dlog(LOG_INFO, "buffer: %s\n", conn->recv_buffer);
	const char *http_position = strstr(conn->recv_buffer, "HTTP");

	if (http_position != NULL) {
		extract_and_set_filename(conn);

		conn->filename[strlen(conn->filename) - 1] = 0;
		dlog(LOG_INFO, "filename: %s\n", conn->filename);
		int file_fd = open(conn->filename, O_RDONLY);

		if (file_fd == -1) {
			connection_prepare_send_404(conn);
			int total_sent = 0;

			while (1) {
				bytes_sent = send(conn->sockfd, conn->send_buffer + total_sent, conn->send_len - total_sent, 0);
				if (bytes_sent == 0)
					break;

				total_sent += bytes_sent;
			}
			dlog(LOG_INFO, "Failed to open file %s\n", conn->filename);
			return;
		}

		off_t file_size = lseek(file_fd, 0, SEEK_END);

		conn->file_size = file_size;

		if (file_size == -1) {
			dlog(LOG_INFO, "Error in lseek: %s\n", strerror(errno));
			close(file_fd);
			return;
		}

		lseek(file_fd, 0, SEEK_SET);

		snprintf(conn->send_buffer, sizeof(conn->send_buffer),
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: %ld\r\n"
			"Connection: close\r\n"
			"\r\n",
			file_size);
		conn->send_len = strlen(conn->send_buffer);
		dlog(LOG_INFO, "Number of characters sent in header: %ld\n", conn->send_len);

		int total_sent = 0;

		while (1) {
			bytes_sent = send(conn->sockfd, conn->send_buffer + total_sent, conn->send_len - total_sent, 0);
			if (bytes_sent == 0)
				break;
			total_sent += bytes_sent;
		}

		dlog(LOG_INFO, "Sent header: %s\n", conn->send_buffer);
		conn->fd = file_fd;
		close(file_fd);
		if (connection_get_resource_type(conn) == RESOURCE_TYPE_STATIC)
			connection_send_static(conn);
		else
			connection_send_dynamic(conn);
	}

	if (bytes_sent <= 0) {
		dlog(LOG_INFO, "Client closed the connection\n");
		close(conn->sockfd);
	}
}

void handle_client(uint32_t event, struct connection *conn)
{
	/* Handle new client. There can be input and output connections.
	 * Take care of what happened at the end of a connection.
	 */
	if (event & EPOLLIN)
		handle_input(conn);

	if (event & EPOLLOUT) {
		conn->state = STATE_SENDING_DATA;
		handle_output(conn);

		if (conn->state == STATE_SENDING_DATA)
			connection_send_data(conn);
	}
}

int main(void)
{
	int rc;

	/* Initialize asynchronous operations. */
	io_setup(1, &ctx);

	/* Initialize multiplexing. */
	epollfd = w_epoll_create();

	/* Create server socket. */
	listenfd = tcp_create_listener(AWS_LISTEN_PORT, DEFAULT_LISTEN_BACKLOG);

	rc = w_epoll_add_fd_in(epollfd, listenfd);

	/* Uncomment the following line for debugging. */
	dlog(LOG_INFO, "Server waiting for connections on port %d\n", AWS_LISTEN_PORT);

	/* server main loop */
	while (1) {
		struct epoll_event rev;

		/* Wait for events. */
		rc = w_epoll_wait_infinite(epollfd, &rev);
		DIE(rc < 0, "w_epoll_wait_infinite");

		/* Switch event types; consider
		 *   - new connection requests (on server socket)
		 *   - socket communication (on connection sockets)
		 */
		if (rev.data.fd == listenfd) {
			if (rev.events & EPOLLIN)
				handle_new_connection();
		} else {
			handle_client(rev.events, rev.data.ptr);
		}
	}

	return 0;
}
