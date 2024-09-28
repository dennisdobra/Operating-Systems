# mini-libc

# Overview

 - Mini-libc is a minimalistic implementation of the C standard library designed to work on Linux systems.
 - This library is a simplified replacement for the system's libc (like glibc), built directly on the syscall interface provided by Linux.
 - The project is targeted at x86_64 architecture and provides core functionalities such as string manipulation, memory management, and POSIX file I/O.
 - The goal of this project is to implement a small subset of libc from scratch, gaining a deeper understanding of its structure, functionality, and low-level system interfaces.

# Features

String Handling (<string.h>):
 - strcpy(), strcat(), strlen(), strncpy(), strncat(), strcmp(), strncmp(), strstr(), strrstr(), memcpy(), memset(), memmove(), memcmp().

Input/Output (<stdio.h>, <unistd.h>, <sys/fcntl.h>, <sys/stat.h>):
 - Basic file handling functions: open(), close(), lseek(), stat(), fstat(), truncate(), ftruncate().
 - Output function: puts().
 - Time management: nanosleep(), sleep().

Memory Management (<stdlib.h>, <sys/mman.h>):
 - malloc(), free(), calloc(), realloc(), realloc_array(), mmap(), mremap(), munmap().

Error Handling (<errno.h>):
 - Global errno variable to capture and handle system errors.

# System Requirements

 - Linux system with x86_64 architecture (for development and testing).
 - Alternatively, use an x86_64 virtual machine on ARM64 or Aarch64 systems (such as macOS).

# Project Structure

 - src/: The main source code for mini-libc. This includes the core functions and system call wrappers.
 - samples/: Sample applications and use cases that demonstrate the functionality of mini-libc.
 - tests/: A comprehensive set of tests to validate the correctness of mini-libc, including both functional and performance testing.
