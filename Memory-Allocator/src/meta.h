/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include "printf.h"
#include "block_meta.h"
#include <unistd.h>

#define MMAP_THRESHOLD		(128 * 1024)

extern struct block_meta *block_head_mmap;
extern struct block_meta *block_head_sbrk;

/* FUNCTIONS SIGNATURES*/
struct block_meta* find_best_fit(size_t size);
void split_block(struct block_meta* block, size_t difference);
struct block_meta* add_sbrk_last(size_t size);
struct block_meta* complete_last_sbrk(size_t size);
void coalesce_right(struct block_meta* block_to_be_freed);
void coalesce_left(struct block_meta *block_to_be_freed);
struct block_meta* block_meta_add_last_mmap(struct block_meta* new_block, size_t size);

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT -1))
#define SIZE_T_SIZE (ALIGN(sizeof(struct block_meta)))