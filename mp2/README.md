[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/bN8FEXqy)
# UIUC CS 423 MP2

Your Name: Yanxin Jiang

Your NetID: yanxinj2

## Overview

In this mp, we'll implement a Real-Time CPU scheduler based on the Rate-Monotonic Scheduler (RMS) for a Single-Core Processor. For communication between the scheduler and user-space applications, we'll utilize the Proc filesystem, specifically a single Proc filesystem entry for all interactions: /proc/mp2/status, which can be readable and writable by users.

## implementation

### Rate-Monotonic Scheduler (RMS)
Our scheduler will support the following three operations through the Proc filesystem:

**Registration**: This operation allows the application to inform the Kernel module of its intent to use the RMS scheduler. The application provides its registration parameters to the kernel module (PID, Period, Processing Time).
**Yield**: This operation signals the RMS scheduler that the application has completed its period. After yielding, the application will block until the next period.
**De-Registration**: This operation lets the application inform the RMS scheduler that it has finished using the RMS scheduler.

Rate Monotonic don't allow the job of any application to run before its period. Hence, the application must sleep until the beginning of the period without any busy waiting, avoiding the waste of valuable resources. Consequently, our applications will exhibit various states in the kernel:
**READY**: The application has reached the beginning of the period, and a new job is ready to be scheduled.
**RUNNING**: The application is currently executing a job and using the CPU.
**SLEEPING**: The application has completed the job for the current period and is awaiting the next one.

We need to be careful when initializing switching the status of task in registration, yield, dispatching_kthread, timer handler, etc to ensure the correct operation of the scheduler.

### Wake Up and Preemption Mechanisms
As soon as the dispatching thread wakes up, we will need to find a new task in READY state with the highest priority (with the shortest period) from the list of registered tasks.

When the current running task is in RUNNING state, if the new task has a higher priority than current running task, we need to preempt the current running task (if any) and context switch to the chosen task. Set the preempted task to READY since it's not yet completed and set the newly active task to RUNNING. Otherwise, keep the status of the current running task and it can't be preempted.

When the current task is in SLEEPING state, it means that we enter the dispatching thread from yield handler which changes the task from RUNNING to SLEEPING. We preempt the current task. If there's a task in READY state, pick the one with highest priority and context switch to it. Otherwise, set current running task to NULL.

When the current task is NULL, follow the same procedure as it is in SLEEPING state except that no preemption will happen.
