/* SPDX-License-Identifier: BSD-3-Clause */

#include "block_meta.h"
#include "meta.h"
#include <sys/mman.h>

// 2 global headers for the 2 lists
struct block_meta *block_head_mmap;
struct block_meta *block_head_sbrk;

/* FIND_BEST_FIT */
struct block_meta *find_best_fit(size_t size)
{
	size_t perfect_size = MMAP_THRESHOLD;
	struct block_meta *found = NULL;

	struct block_meta *current = block_head_sbrk;

	do {
		if (current->status == STATUS_FREE && current->size >= size && perfect_size > current->size) {
			perfect_size = current->size;
			found = current;
		}
		if (current == current->next)
			break;
		current = current->next;
	} while (current != block_head_sbrk);

	if (found != NULL) {
		// Remember the large size of the block in which we will allocate
		size_t initial_size = found->size;
		// Check how much space remains in the block after allocation
		size_t difference = found->size - size;

		found->size = size;
		found->status = STATUS_ALLOC;

		// Check if we can split the block
		if (SIZE_T_SIZE + sizeof(char) <= difference)
			split_block(found, difference);
		else
			found->size = initial_size;
	}

	return found;
}

/* SPLIT_BLOCK */
void split_block(struct block_meta *block, size_t difference)
{
	// difference = the remaining space

	struct block_meta *new_free_block = (struct block_meta *)((char *)block + SIZE_T_SIZE + block->size);

	new_free_block->prev = block;
	new_free_block->next = block->next;
	block->next = new_free_block;
	new_free_block->next->prev = new_free_block;

	new_free_block->status = STATUS_FREE;
	size_t new_free_block_size = difference - SIZE_T_SIZE;

	new_free_block->size = ALIGN(new_free_block_size);
}

/* ADD_LAST_BLOCK_WITH_SBRK */
struct block_meta *add_sbrk_last(size_t size)
{
	struct block_meta *new_block = sbrk(size + SIZE_T_SIZE);

	block_head_sbrk->prev->next = new_block;
	new_block->prev = block_head_sbrk->prev;
	new_block->next = block_head_sbrk;
	block_head_sbrk->prev = new_block;

	new_block->status = STATUS_ALLOC;
	new_block->size = ALIGN(size);

	return new_block + 1;
}

/* COMPLETE_LAST_BLOCK_WITH_SBRK */
struct block_meta *complete_last_sbrk(size_t size)
{
	// Extend the block using sbrk by the difference
	size_t difference = size - block_head_sbrk->prev->size;

	sbrk(difference);

	block_head_sbrk->prev->size = size;
	block_head_sbrk->prev->status = STATUS_ALLOC;

	return block_head_sbrk->prev + 1;
}

/* COALESCE_RIGHT */
void coalesce_right(struct block_meta *block_to_be_freed)
{
	if (block_to_be_freed->next == block_head_sbrk)
		return;

	block_to_be_freed->status = STATUS_FREE;
	block_to_be_freed->size += block_to_be_freed->next->size + SIZE_T_SIZE;

	block_to_be_freed->next->next->prev = block_to_be_freed;
	block_to_be_freed->next = block_to_be_freed->next->next;
}

/* COALESCE_LEFT */
void coalesce_left(struct block_meta *block_to_be_freed)
{
	block_to_be_freed->status = STATUS_FREE;
	block_to_be_freed->prev->size += SIZE_T_SIZE + block_to_be_freed->size;

	block_to_be_freed->next->prev = block_to_be_freed->prev;
	block_to_be_freed->prev->next = block_to_be_freed->next;
}

/* ADD_LAST_BLOCK_WITH_MMAP */
struct block_meta *block_meta_add_last_mmap(struct block_meta *new_block, size_t size)
{
	if (block_head_mmap == NULL) {
		block_head_mmap = mmap(NULL, ALIGN(size) + SIZE_T_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | 32, -1, 0);
		block_head_mmap->size = ALIGN(size);
		block_head_mmap->next = block_head_mmap;
		block_head_mmap->prev = block_head_mmap;
		block_head_mmap->status = STATUS_MAPPED;
		return block_head_mmap;
	}

	new_block = mmap(NULL, ALIGN(size) + SIZE_T_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | 32, -1, 0);

	new_block->prev = block_head_mmap->prev;
	block_head_mmap->prev->next = new_block;
	block_head_mmap->prev = new_block;
	new_block->next = block_head_mmap;

	new_block->size = size;
	new_block->status = STATUS_MAPPED;

	return new_block;
}
