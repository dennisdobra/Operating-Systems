/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __OS_THREADPOOL_H__
#define __OS_THREADPOOL_H__	1

#include <pthread.h>
#include "os_list.h"

typedef struct {
	void *argument;
	void (*action)(void *arg);
	void (*destroy_arg)(void *arg);
	os_list_node_t list;
} os_task_t;

typedef struct os_threadpool {
	unsigned int num_threads;
	pthread_t *threads;

	/*
	 * Head of queue used to store tasks.
	 * The first item is head.next if head.next != head (i.e., if the queue
	 * is not empty).
	 * The last item is head.prev if head.prev != head (i.e., if the queue
	 * is not empty).
	 */
	os_list_node_t head;

	/* TODO: Define thread pool / queue synchronization data. */
	pthread_mutex_t mutex;   		// Mutex for synchronizing the thread pool
	int active_tasks;				// Number of tasks currently being executed
	pthread_cond_t task_available; 	// Variable for signaling the availability of tasks
	sem_t sem;						// Semaphore for starting the program
} os_threadpool_t;

os_task_t *create_task(void (*f)(void *), void *arg, void (*destroy_arg)(void *));
void destroy_task(os_task_t *t);

os_threadpool_t *create_threadpool(unsigned int num_threads);
void destroy_threadpool(os_threadpool_t *tp);

void enqueue_task(os_threadpool_t *q, os_task_t *t);
os_task_t *dequeue_task(os_threadpool_t *tp);
void wait_for_completion(os_threadpool_t *tp);

#endif
