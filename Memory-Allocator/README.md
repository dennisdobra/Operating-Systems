# Memory Allocator

## Overview

This project is a minimalistic memory allocator that manually manages virtual memory in a Linux environment. The goal is to provide reliable memory allocation, reallocation, and deallocation with a small footprint, similar to the standard `malloc()`, `calloc()`, `realloc()`, and `free()` functions.

The allocator uses Linux system calls like `brk()`, `mmap()`, and `munmap()` to manage memory. It also implements advanced techniques like block splitting, block coalescing, memory alignment, and heap preallocation to improve performance and reduce memory fragmentation.

## Features

### Custom memory management functions:
- `os_malloc(size_t size)` – Allocates memory dynamically.
- `os_calloc(size_t nmemb, size_t size)` – Allocates memory and initializes it to zero.
- `os_realloc(void *ptr, size_t size)` – Resizes previously allocated memory.
- `os_free(void *ptr)` – Frees allocated memory.

### Memory management strategies:
- **Block splitting:** Efficient use of memory by dividing larger blocks into smaller ones.
- **Block coalescing:** Merges adjacent free blocks to reduce external fragmentation.
- **Memory alignment:** Ensures all allocated memory is aligned to 8 bytes for performance optimization.
- **Heap preallocation:** Reduces the number of syscalls by allocating larger chunks of memory when needed.

### Efficient use of `brk()` and `mmap()`:
- Small allocations use `brk()` while larger chunks rely on `mmap()` for efficient memory management.

## Directory Structure

Memory-Allocator/
│
├── src/
│   ├── osmem.c           # Memory allocation implementations
│   ├── helpers.c         # Helper functions
│   └── Makefile          # Build instructions for libosmem.so
│
├── tests/
│   ├── snippets/         # Test cases for allocator functions
│   ├── ref/              # Reference outputs for the test suite
│   ├── run-tests.py      # Automated testing script
│   └── Makefile          # Builds and runs tests
│
└── utils/
    ├── osmem.h           # Header file defining the API
    ├── block_meta.h      # Metadata for memory blocks
    └── printf.c          # Custom printf implementation without heap usage

## Notes

- Memory allocations smaller than the `MMAP_THRESHOLD` use `brk()`, while larger ones use `mmap()`.
- `os_realloc()` tries to expand blocks in place, coalescing adjacent free blocks if needed.
- Make sure to check syscall error codes using the provided `DIE()` macro to ensure robustness.