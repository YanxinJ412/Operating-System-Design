[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/3ti8WBjt)
# MP3-PageFaultProfiler

Your Name: Yanxin Jiang

Your NetID: yanxinj2

## Overview

In this MP, we will implement a profiler of page fault rate and CPU utilization for the Linux system. We will implement this profiler in a Linux Kernel module and use the Proc filesystem, a character device driver, and a shared memory area to communicate with user space applications. We will use a single Proc file-system entry (/proc/mp3/status) for registering and unregistering monitored user-level processes. The Proc file is accessible by any user. Our profiler should implement three operations available through the Proc file-system.

## implementation

### /proc Filesystem proc_ops

Write callback function coould handle the message including **REGISTRATION** and  **UNREGISTRATION**. **REGISTRATION** registers a new process into the task_list. If this is the first process, add a task in the new queue. **UNREGISTRATION** remove the task from the list and free all data structures allocated during registration. When the last process is deleted, we need to flush and cancel the delayed workqueue and delayed work.

**Registration**: R <PID>
**Unregistration**: U <PID>

### Virtaully-Contiguous Memory
A memory buffer **membuf** is allocated in the kernel memory. This buffer needs to be virtually contiguous but not necessarily physical contiguous. So we need to use vmalloc for allocate this buffer. As we initialize the module, initialize the buffer with -1 in order for the proper implementation of monitor program.

### Delayed Work and Delayed Workqueue

Our kernel module use a delayed work queue that periodically measures the major and minor page fault counts, and CPU utilization of all registered user processes and saves the measured information to the memory buffer. **queue_delayed_work()** can be used to plan a work for execution with a given delay. 

The sampling rate of the profiler is 20 times per second, which means work handler must be executed 20 times per second by the work queue. Therefore, the delayed time should be msecs_to_jiffies(1000/20). Each sample consists of four unsigned long type data: (a)jiffies value (the number of timer ticks executed since the kernel boot-up), (b) minor fault count, (c) major fault count, and (d) CPU utilization (s_time + u_time). The work handler only writes one sample each time. In each sample, (b), (c), and (d) are the sum of that of all the registered processes within a sampling period (1/20 seconds).

### Character Device Driver
The character device driver allows user-level process to map the shared memory buffer to its address space. We need three callback function (open, release, and mmap) while open and release function simply return 0.

Use register_chrdev_region() to register a range of device numbers. We use 423 as the major number of character device and 0 as the minor number of character device. To access this character device from user-level process, a file needs to be created by the following shell commands:
```
$ insmod mp3.ko
$ cat /proc/devices
<check the created device’s major number>
$ mknod node c 423 0
```

### mmap() callback

The buffer memory is mapped into the virtual address space of a user process upon request by mmap() callback. We need to map the physical pages of the buffer to the virtual address space of a requested user process. In order to map the entire physical pages to the requested virtual address space, for each page, use vmalloc_to_pfn(Physical Page Virtual Addr) to get its corresponding physical address. Then, use remap_pfn_range() to map each virtual page to the physical page obtained from vmalloc_to_pfn(). This is requested by a user-level process when the process executes the mmap() function on the character device of the kernel module. This implies this mapping shall be done for the range of virtual address space that is passed as parameters of the request of the user process.

