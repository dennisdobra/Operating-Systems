// SPDX-License-Identifier: BSD-3-Clause
#include "osmem.h"
#include "meta.h"
#include "block_meta.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "printf.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h> 

#define MMAP_THRESHOLD		(128 * 1024) // Threshold for switching to mmap
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT -1)) // Align size to multiple of 8
#define SIZE_T_SIZE (ALIGN(sizeof(struct block_meta))) // Aligned size of metadata structure

void *os_malloc(size_t size)
{
	// size = payload size requested
	if (size == 0)
		return NULL;

	// Align size (add padding if necessary)
	size = ALIGN(size);

	/* HEAP PREALLOCATION */
	// If the head of the sbrk list is null and the size is below the mmap threshold
	if (block_head_sbrk == NULL && SIZE_T_SIZE + size < MMAP_THRESHOLD) {
		size_t initial_heap_size = MMAP_THRESHOLD;

		// Allocate initial heap size with sbrk
		block_head_sbrk = sbrk(initial_heap_size);

		block_head_sbrk->size = size;
		block_head_sbrk->status = STATUS_ALLOC;
		block_head_sbrk->prev = block_head_sbrk;
		block_head_sbrk->next = block_head_sbrk;

		/* SPLIT_BLOCK */
		// If there is enough space to create a new free block
		if (initial_heap_size - (SIZE_T_SIZE + block_head_sbrk->size) >= SIZE_T_SIZE + sizeof(char)) {
			struct block_meta *new_free_block = (struct block_meta *)((char *)block_head_sbrk + SIZE_T_SIZE + block_head_sbrk->size);

			new_free_block->prev = block_head_sbrk;
			new_free_block->next = block_head_sbrk;
			block_head_sbrk->next = new_free_block;
			block_head_sbrk->prev = new_free_block;

			new_free_block->status = STATUS_FREE;
			new_free_block->size = initial_heap_size - (SIZE_T_SIZE + block_head_sbrk->size) - SIZE_T_SIZE;
		} else
			block_head_sbrk->size = MMAP_THRESHOLD - SIZE_T_SIZE;

		// Return pointer to the allocated block's data
		return (block_head_sbrk + 1);

	// If the size exceeds the mmap threshold and no mmap blocks exist
	} else if (block_head_mmap == NULL && SIZE_T_SIZE + size >= MMAP_THRESHOLD) {
		block_head_mmap = mmap(NULL, size + SIZE_T_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | 32, -1, 0);

		block_head_mmap->size = size;
		block_head_mmap->status = STATUS_MAPPED;
		block_head_mmap->prev = block_head_mmap;
		block_head_mmap->next = block_head_mmap;

		return (block_head_mmap + 1);
	}
	// Otherwise (blocks already exist)
	if (size + SIZE_T_SIZE < MMAP_THRESHOLD) {
		// Traverse the sbrk list to find the best fitting block
		struct block_meta *tmp = find_best_fit(size);

		// If no suitable block is found, add or complete a block at the end
		if (tmp == NULL) {
			if (block_head_sbrk->prev->status != STATUS_FREE)
				return add_sbrk_last(size);
			return complete_last_sbrk(size);
		} else
			return tmp + 1;

	// If size exceeds the mmap threshold, use mmap to allocate
	} else if (size + SIZE_T_SIZE >= MMAP_THRESHOLD)
		return block_meta_add_last_mmap(NULL, size) + 1;
	return NULL;
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;

	// Get the block metadata associated with the pointer
	struct block_meta *block = (struct block_meta *)((char *)ptr - sizeof(struct block_meta));

	if (block->status == STATUS_MAPPED) {
		// Unmap the block if it's mapped
		if (block->next == block) {
			munmap(block, ALIGN(block->size) + SIZE_T_SIZE);
			block_head_mmap = NULL;
		} else {
			if (block == block_head_mmap)
				block_head_mmap = block->next;
			block->prev->next = block->next;
			block->next->prev = block->prev;

			munmap(block, ALIGN(block->size) + SIZE_T_SIZE);
		}
	} else if (block->status == STATUS_ALLOC) {
		// Try to coalesce with adjacent blocks if possible
		// Coalesce with the next block if it's free
		if (block->next != block_head_sbrk && block->next->status == STATUS_FREE)
			coalesce_right(block);
		// Coalesce with the previous block if it's free
		if (block->prev != block_head_sbrk->prev && block->prev->status == STATUS_FREE)
			coalesce_left(block);
		else
			block->status = STATUS_FREE;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (size == 0 || nmemb == 0)
		return NULL;

	// Calculate the total payload size and align it
	size_t payload_size = ALIGN(nmemb * size);

	// If there are no mmap blocks and size exceeds the page size, use mmap
	if (block_head_mmap == NULL && SIZE_T_SIZE + payload_size >= (size_t)getpagesize()) {
		block_head_mmap = mmap(NULL, SIZE_T_SIZE + payload_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | 32, -1, 0);

		block_head_mmap->size = payload_size;
		block_head_mmap->status = STATUS_MAPPED;
		block_head_mmap->prev = block_head_mmap;
		block_head_mmap->next = block_head_mmap;

		// Set the allocated memory to 0
		memset((char *)block_head_mmap + SIZE_T_SIZE, 0, payload_size);

		return (block_head_mmap + 1);

	// Otherwise, use sbrk if the size is smaller than the page size
	} else if (block_head_sbrk == NULL && SIZE_T_SIZE + payload_size < (size_t)getpagesize()) {
		// Allocate an initial heap size with sbrk
		size_t initial_heap_size = MMAP_THRESHOLD;

		block_head_sbrk = sbrk(initial_heap_size);

		block_head_sbrk->size = payload_size;
		block_head_sbrk->status = STATUS_ALLOC;
		block_head_sbrk->prev = block_head_sbrk;
		block_head_sbrk->next = block_head_sbrk;

		// Set the allocated memory to 0
		memset((char *)block_head_sbrk + SIZE_T_SIZE, 0, payload_size);

		/* SPLIT_BLOCK */
		// If there is enough space to create a new free block
		if (initial_heap_size - (SIZE_T_SIZE + block_head_sbrk->size) >= SIZE_T_SIZE + sizeof(char)) {
			struct block_meta *new_free_block = (struct block_meta *)((char *)block_head_sbrk + SIZE_T_SIZE + block_head_sbrk->size);

			new_free_block->prev = block_head_sbrk;
			new_free_block->next = block_head_sbrk;
			block_head_sbrk->next = new_free_block;
			block_head_sbrk->prev = new_free_block;

			new_free_block->status = STATUS_FREE;
			new_free_block->size = initial_heap_size - (SIZE_T_SIZE + block_head_sbrk->size) - SIZE_T_SIZE;
		} else
			block_head_sbrk->size = MMAP_THRESHOLD - SIZE_T_SIZE;

		return (block_head_sbrk + 1);

	} else {
		// If at least one block head exists
		if (payload_size + SIZE_T_SIZE < (size_t)getpagesize()) {

			struct block_meta *tmp = find_best_fit(payload_size);

			if (tmp == NULL) {
				// If no suitable block is found
				if (block_head_sbrk->prev->status != STATUS_FREE) {
					struct block_meta *block = add_sbrk_last(payload_size);

					memset((char *)block, 0, payload_size);
					return block;
				}
				// Otherwise, complete the last block
				struct block_meta *block = complete_last_sbrk(payload_size);

				memset((char *)block, 0, payload_size);
				return block_head_sbrk->prev + 1;
			}
			// Otherwise, set the memory to 0 and return the pointer
			memset((char *)tmp + SIZE_T_SIZE, 0, payload_size);
			return tmp + 1;

		} else if (payload_size + SIZE_T_SIZE >= (size_t)getpagesize()) {
			// Allocate with mmap and set the memory to 0
			struct block_meta *block = block_meta_add_last_mmap(NULL, payload_size);

			memset((char *)block + SIZE_T_SIZE, 0, payload_size);
			return block + 1;
		}
	}
	return NULL;
}

void *os_realloc(void *ptr, size_t size)
{
	// If the pointer is null, simply allocate a new block
	if (ptr == NULL)
		return os_malloc(size);

	// Align the size
	size = ALIGN(size);

	// Get the block metadata associated with the pointer
	struct block_meta *block = (struct block_meta *)((char *)ptr - SIZE_T_SIZE);

	// If the size exceeds the current block size
	if (size > block->size) {
		// Allocate a new block
		void *new_ptr = os_malloc(size);

		if (new_ptr != NULL) {
			// Copy the contents of the old block to the new block
			memcpy(new_ptr, ptr, block->size);

			// Free the old block
			os_free(ptr);
		}
		return new_ptr;
	}

	// If the size is within the current block size
	if (size <= block->size) {
		// Check if we can split the block
		if (block->size - size > SIZE_T_SIZE + sizeof(char)) {
			struct block_meta *split = (struct block_meta *)((char *)block + size + SIZE_T_SIZE);

			split->status = STATUS_FREE;
			split->size = block->size - size - SIZE_T_SIZE;
			split->prev = block;
			split->next = block->next;

			block->size = size;
			block->next = split;
		}
		return ptr;
	}

	return NULL;
}
