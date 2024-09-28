// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>

char *strcpy(char *destination, const char *source)
{
	int len = strlen(source);
	for (int i = 0; i < len; i++) {
		destination[i] = source[i];
	}
	destination[len] = '\0';
	return destination;
}

char *strncpy(char *destination, const char *source, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		destination[i] = source[i];
	}
	return destination;
}

char *strcat(char *destination, const char *source)
{
	int len = strlen(destination);
	int i = 0;
	while (source[i]) {
		destination[len + i] = source[i];
		i++;
	}
	destination[len + i] = '\0';
	return destination;
}

char *strncat(char *destination, const char *source, size_t len)
{
	size_t length = strlen(destination);
	size_t cnt = 0;
	while (cnt != len) {
		destination[length + cnt] = source[cnt];
		cnt++;
	}
	destination[length + len] = '\0';
	return destination;
}

int strcmp(const char *str1, const char *str2)
{
	if (strlen(str1) != strlen(str2)) {
		if (strlen(str1) < strlen(str2))
			return -1;
		else if (strlen(str1) > strlen(str2))
			return 1;
	} else {
		for (size_t i = 0; i < strlen(str1); i++) {
			if (str1[i] != str2[i] && str1[i] < str2[i])
				return -1;
			else if (str1[i] != str2[i] && str1[i] > str2[i])
				return 1;
		}
	}
	return 0;
}

int strncmp(const char *str1, const char *str2, size_t len)
{
	if (len == 0)
		return 0;
	for (size_t i = 0; i < len; i++) {
		if (str1[i] != str2[i]) {
			if (str1[i] < str2[i])
				return -1;
			else if (str1[i] > str2[i])
				return 1;
		}
	}
	return 0;
}

size_t strlen(const char *str)
{
	size_t i = 0;

	for (; *str != '\0'; str++, i++)
		;

	return i;
}

char *strchr(const char *str, int c)
{
	for (size_t i = 0; i < strlen(str); i++) {
		if (str[i] == c)
			return (char *)(str + i);
	}
	return NULL;
}

char *strrchr(const char *str, int c)
{
	for (int i = strlen(str); i >= 0; i--) {
		if (str[i] == c)
			return (char *)(str + i);
	}
	return NULL;
}

char *strstr(const char *haystack, const char *needle)
{
	while (*haystack) {
		if (strncmp(haystack, needle, strlen(needle)) == 0) {
			return (char *)haystack;
		}
		haystack++;
	}
	return NULL;
}

char *strrstr(const char *haystack, const char *needle)
{
	int haystack_len = strlen(haystack);
	int needle_len = strlen(needle);

	char *p = (char *)(haystack + haystack_len - needle_len);

	if (needle_len == 0) {
		return (char *)haystack;
	}

	while (p >= haystack) {
		if (strncmp(p, needle, needle_len) == 0)
			return p;
		p--;
	}

	return NULL;
}

void *memcpy(void *destination, const void *source, size_t num)
{
	while (num--) {
		*(char *)destination++ = *(const char *)source++;
	}
	return destination;
}

void *memmove(void *destination, const void *source, size_t num)
{
	char *dst = (char *)destination;
	const char *src =  (const char *)source;

	if (dst > src && dst < src + num) {
		dst += num - 1;
		src += num - 1;

		while (num--) {
			*dst-- = *src--;
		}
	} else {
		while (num--) {
			*dst++ = *src++;
		}
	}
	return destination;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	if (strncmp(ptr1, ptr2, num) < 0)
		return -1;
	else if (strncmp(ptr1, ptr2, num) > 0)
		return 1;
	return 0;
}

void *memset(void *source, int value, size_t num)
{
	while (num--) {
		*(unsigned char *)source++ = (unsigned char)value;
	}
	return source;
}

