// SPDX-License-Identifier: BSD-3-Clause

#include <internal/mm/mem_list.h>
#include <internal/types.h>
#include <internal/essentials.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

void *malloc(size_t size)
{
	void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (mem == MAP_FAILED)
		return NULL;

	if (mem_list_add(mem, size) == -1) {
		munmap(mem, size);
		return NULL;
	}
	return mem;
}

void *calloc(size_t nmemb, size_t size)
{
	size_t total_size = nmemb * size;

	if (nmemb != 0 && total_size / nmemb != size) {
		return NULL;  // Overflow detected
	}

	void *mem = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (mem == MAP_FAILED)
		return NULL;

	if (mem_list_add(mem, total_size) == -1) {
		munmap(mem, total_size);
		return NULL;
	}
	return mem;
}

void free(void *ptr)
{
	struct mem_list *item = mem_list_find(ptr);
	munmap(ptr, item->len);
	mem_list_del(ptr);
}

void *realloc(void *ptr, size_t size)
{
	if (size == 0) {
		free(ptr);
		return NULL;
	}

	if (!ptr) {
		return malloc(size);
	}

	size_t *old_size = (size_t *)ptr - 1;

	if (*old_size >= size)
		return ptr;

	void *new_ptr = malloc(size);
	if (!new_ptr)
		return NULL;

	memcpy(new_ptr, ptr, *old_size);

	free(ptr);

	return new_ptr;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size)
{
	size_t total_size = nmemb * size;
	return realloc(ptr, total_size);
}
