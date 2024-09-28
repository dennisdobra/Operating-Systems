# Parallel Graph

## Overview

This project implements a generic thread pool that is used to traverse a graph and compute the sum of the elements contained in its nodes. It provides a parallel version of graph traversal using a thread pool to efficiently manage tasks.

## Thread Pool Description

A thread pool contains a fixed number of active threads that wait for specific tasks. The threads are created upon the thread pool's initialization and remain active throughout its lifetime. This approach avoids the overhead of frequently creating and destroying threads. The following functions are implemented:

- **task_create**: Creates an `os_task_t` that represents a task in the queue, consisting of a function pointer and an argument.
- **add_task_in_queue**: Adds a given task to the thread pool's task queue.
- **get_task**: Retrieves a task from the thread pool's task queue.
- **threadpool_create**: Allocates and initializes a new thread pool.
- **thread_loop_function**: The main function executed by threads, waiting for tasks and invoking the corresponding function.
- **threadpool_stop**: Stops all threads and waits for a condition indicating that the graph has been completely traversed.

## Graph Traversal

The parallel implementation computes the sum of all the nodes in a graph. The serial implementation is provided in `src/serial.c`. The process involves:

1. Adding the current node's value to the overall sum.
2. Creating tasks for neighboring nodes and adding them to the task queue.

To ensure that threads stop once the graph traversal is complete, a condition is implemented in a function passed to `threadpool_stop`, which waits for this condition before joining all threads.

## Synchronization

For synchronization, various mechanisms such as mutexes, semaphores, spinlocks, or condition variables can be used. However, hacks like `sleep`, `printf` synchronization, or unnecessary computations are not allowed.

## Input Files

Graph representation in input files follows this structure:

- The first line contains two integers, N (number of nodes) and M (number of edges).
- The second line contains N integers representing the values of the nodes.
- The next M lines contain two integers each, representing the source and destination of an edge.

## Data Structures

- **Graph**: Represented as an `os_graph_t` (see `src/os_graph.h`).
- **List**: Implemented as an `os_queue_t` (see `src/os_list.h`), used for the task queue.
- **Thread Pool**: Represented as an `os_threadpool_t` (see `src/os_threadpool.h`), containing information about the task queue and the threads.

You are not allowed to modify these data structures but can create additional structures leveraging them.

## Infrastructure

### Compilation

To compile both the serial and parallel versions, navigate to the `src/` directory and run:

```bash
make
