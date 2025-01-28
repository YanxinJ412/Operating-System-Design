[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/1a6ZJ1_M)
# UIUC CS 423 MP1

Your Name: Yanxin Jiang

Your NetID: yanxinj2

## Overview

In this MP we built a kernel module that measures the User Space CPU Time of processes. The kernel module should allow multiple processes to register themselves as targets of measurement and later query their CPU Time usage. We created a user-space program to test your kernel module implementation.

## implementation

### Proc File System Interaction
The user interacts with the kernel module through `/proc/mp1/status` file. When a process writes a PID into the Proc file, the single process or multiple process corresponding to the PID should be registered in the module for User Space CPU Time measurement. When a process reads the Proc file, the file should return a list of registered processes together with their corresponding User Space CPU Time shown as `<PID>: <CPU Time of PID>`

In read callback function, there is a variable `loff_t *pos`, which is the file pointer of read data. We should start from this position, read `size_t count` byte, and store them in user_buffer by `copy_to_user`. On the other hand, the write callback function allows adding a new process node to the linked list, copying the data from user space using 'copy_from_user`.

### Periodic CPU Time Updates
The kernel module should keep the list of registered process and their user time in memory and update them every 5 seconds. We use the Two-Halves Approach to handle the timer interrupt, and implement it using Kernel Workqueue. When the timer is fired, the timer callback function should use Kernel Workqueue to schedule an user time update work, which is Top-Half. The user time update work should, when exiting the Workqueue, iterate the registered process list and update the user time, which is Bottom-Half. 

### Mutex Utilization
The Linux kernel is a preemptible kernel, which measn all the contexts run concurrently and can be interrupted from its execution at any time. To protect your data structures, we need to set up the lock of process list when adding, delete nodes or iterating through the list.

### Kernel Slab Allocator
To allocate and free memory, we need to use Kernel Slab Allocator. We create the buffer, linked list node to read/write data and store the data using `kmalloc` and eventually free them to prevent memory leakage.