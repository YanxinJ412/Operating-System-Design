#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>   
#include <uapi/linux/sched/types.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include "mp2_given.h"

// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
// Please put your name and email here
MODULE_AUTHOR("Yanxin Jiang <yanxinj2@illinois.edu>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CS-423 MP2");

#define DEBUG 1
#define LINE_LEN 64
#define MAX_UL 4294967295

// Process Control Block (PCB)
typedef struct mp2_task_struct {
	struct task_struct* linux_task; 
    struct timer_list wakeup_timer; 
	struct list_head list;
	pid_t pid;
	unsigned long period_ms;
	unsigned long runtime_ms;
	unsigned long deadline_jiff;
	enum {READY, RUNNING, SLEEPING} status;
} mp2_task_struct;
// proc directory and entry
static struct proc_dir_entry *proc_dir_mp2, *proc_entry_mp2;
struct task_struct* find_task_by_pid(unsigned int nr);
static void preempt_old_task(mp2_task_struct* old_task);
static void set_new_task(mp2_task_struct* new_task);
// dispatching thread
static struct task_struct *kthread;
// slab allocator
static struct kmem_cache *task_cache;
static struct mp2_task_struct* curr_t = NULL;
// initialize kernel linked list of process and its lock
LIST_HEAD(task_list);
DEFINE_MUTEX(task_list_lock);
static int dispatching_kthreadfn(void *data);
int __init mp2_init(void);
void __exit mp2_exit(void);


/*	timer_callback_mp2
 *	Input:  struct timer_list *timer
 *	This is a Wake Up Timer Handler, which changes the state of the task to READY
 *	and wakes up the dispatching thread. 
 */
static void timer_callback_mp2(struct timer_list *timer){
	// from corresponding mp2_task_struct baed on the timer
	struct mp2_task_struct *task = from_timer(task, timer, wakeup_timer);
	task->status = READY;
	// update the timer, kthread and deadline of the task struct 
	task->deadline_jiff += msecs_to_jiffies(task->period_ms);
	wake_up_process(kthread);
	mod_timer(&task->wakeup_timer, task->deadline_jiff);
}

/*	read_callback_mp2 
 *	Input: struct timer_list *timer, user_buffer, pos
 *	Read callback function aims to read task information from the /proc/status/mp2 file. When a process 
 *  reads the Proc file, the file should return pid, period, and processing time of the current process.
 */
static ssize_t read_callback_mp2(struct file *file, char __user *user_buffer, size_t count, loff_t *pos){
	int read_count = 0;
	struct mp2_task_struct *read_task;
	char line_buffer[LINE_LEN];
	char *kbuffer;
	ssize_t byte;
	// printk("before read, pos: %d\n", (int)*pos);
	// allocate read buffer to store linked list value
	kbuffer = kmalloc(count+*pos, GFP_KERNEL);
	if(!kbuffer){
		printk("Error kmalloc kbuffer memory");
		return -ENOMEM;
	}
	// lock the proess list for safe access
	mutex_lock(&task_list_lock);
	// iterate through link and read the pid, period(ms), and runtime(ms) to kbuffer
	list_for_each_entry(read_task, &task_list, list){
		int offset = snprintf(line_buffer, LINE_LEN, "%d: %lu, %lu\n", read_task->pid, read_task->period_ms, read_task->runtime_ms);
		// if adding this list node will exceed user buffer. Stop the iteration of linked list
		if(offset + read_count > count + *pos){
			break;
		}
		// update kbuffer
		memcpy(kbuffer+read_count, line_buffer, offset);
		read_count += offset;
		
	}
	// calculate the actual read byte
	byte = (read_count - *pos) < count ? (read_count - *pos) : count;
	// Check if we already read this data
	if(*pos > read_count){
		// free the kbuffer and return 0 (i.e. return 0 byte of read data)
		mutex_unlock(&task_list_lock);
		printk("Error already read");
		kfree(kbuffer);
		return 0;
	}
	// copy the read data to user
	if (copy_to_user(user_buffer, kbuffer+*pos, byte)) {
		mutex_unlock(&task_list_lock);
		printk("Error copy to user");
		kfree(kbuffer);
		return -EFAULT;  
	}
	// update the position of file's reading pointer
	*pos += byte;
	// unlock the process list
	mutex_unlock(&task_list_lock);
	kfree(kbuffer);
	return byte;
}

/* 
 *  admission_control
 *	Input: unsigned long new_period_ms, unsigned long new_runtime_ms
 *	Output: int 0 if sucess. Else, sucessfil
 *	The admission control determines if the new application can be scheduled alongside 
 *  other already admitted periodic applications without any deadline misses for the 
 *  registered tasks. 
 */
static int admission_control(unsigned long new_period_ms, unsigned long new_runtime_ms){
	struct mp2_task_struct *iter_task;
	unsigned long utilization = new_runtime_ms * 1000 / new_period_ms;
	// iteraote through the task link to compute the utilization more cearly
	mutex_lock(&task_list_lock);
	list_for_each_entry(iter_task, &task_list, list) {
		utilization += iter_task->runtime_ms * 1000 / iter_task->period_ms;
	}
	mutex_unlock(&task_list_lock);
	// if the utilization is smaller than 693,
	if(utilization <= 693){
		return 0;
	}
	return -1;
	
}

// Registers a new task with specified parameters (pid, period, runtime).
/*	
 *	registration
 *	Input: pid_t xf, unsigned long period_ms, unsigned long runtime_ms
 *	registration function registers a new task with provided pid, period, and runtime.
 *  Initialize the new task;
 */
static int registration(pid_t pid, unsigned long period_ms, unsigned long runtime_ms){
	struct mp2_task_struct *new_task;
	// chek admissioal control to know if the new takedh
	if(admission_control(period_ms, runtime_ms) == -1){
		printk("Error Not allow new task in the system");
		return -EINVAL;
	}
	// lock process lock 
	mutex_lock(&task_list_lock);
	// iterate through linked list to check if PID already exists
	list_for_each_entry(new_task, &task_list, list) {
		if(new_task->pid == pid) {
			mutex_unlock(&task_list_lock);
			printk("Error PID already exists");
			return -EINVAL;
		}
	}
	// update the new process node, allocate its memory firstly
	new_task = kmem_cache_alloc(task_cache, GFP_KERNEL);
	if(!new_task) {
		mutex_unlock(&task_list_lock);
		printk("Error Fail to get new_task from cache");
		return -ENOMEM;
	}
	//  intialize the name, artst, and persitvd
    new_task->pid = pid;
	new_task->period_ms = period_ms;
	new_task->runtime_ms = runtime_ms;
	new_task->status = SLEEPING;
	new_task->deadline_jiff = jiffies + msecs_to_jiffies(new_task->period_ms);
	timer_setup(&new_task->wakeup_timer, timer_callback_mp2, 0);
	mod_timer(&new_task->wakeup_timer, new_task->deadline_jiff);
	new_task->linux_task = find_task_by_pid(pid);
	if (!new_task->linux_task) {
		kmem_cache_free(task_cache, new_task);
		mutex_unlock(&task_list_lock);
		printk("Error Fail to find task by pid");
		return -EINVAL;
	}	
	
	// add the new process node into the linked list
    list_add_tail(&new_task->list, &task_list);
    mutex_unlock(&task_list_lock);
	return 0;
}


/*
 *	yield_handler_mp2
 *	Input: pidt_t pid
 *  Output: int 0, if sucess. Else, unsuccessful operation
 *  yidld hanlder put the associated application to sleep and setup the wakeup timer. It change
 *  the task state to SLEEPING. When the wakeup timer expires, the timer interrupt handler should
 *  change the state of the task to READY and should wake up the dispatching thread. 
 */
static int yield_handler_mp2(pid_t pid){
	struct mp2_task_struct *yield_task;
	mutex_lock(&task_list_lock);
	// printk("jiffies is %lu, deadline_jiff is %lu\n", jiffies, yield_task->deadline_jiff);
	// iterate through the task list and find the task with the corresponding pid for yield.
	list_for_each_entry(yield_task, &task_list, list) {
		if(yield_task->pid == pid){
			// set curr_t to NULL if current task is the yield task
			if (curr_t && curr_t == yield_task) {
					curr_t = NULL;
			}
			// set the task to sleeping status
			yield_task->status = SLEEPING;
			// change status to TASK_UNINTERRUPTIBLE to prevent preeption. 
			set_current_state(TASK_UNINTERRUPTIBLE);
			mutex_unlock(&task_list_lock);
			// get the dispatching thread to sleep
			wake_up_process(kthread);
			schedule();
			return 0;
		}
	}
	mutex_unlock(&task_list_lock);
	printk("Error Fail to find sleep task");
	return -EINVAL;

}

/*
 *	static struct mp2_task_struct* find_highest_priority_task(void){
 *  Output: struct mp2_task_struct* , the highest priority task
 *  find_highest_priority_task iterate through the task list and find the highest priority task 
 *  under Ready state. higher priority measn shorter period time in ms 
 */
static struct mp2_task_struct* find_highest_priority_task(void){
	struct mp2_task_struct* highest_task = NULL;
	struct mp2_task_struct* iter_task;
	unsigned long high_prio_period_ms = MAX_UL;
	// iterate through the task list to find the highest priority task by comparing their period
	mutex_lock(&task_list_lock);
	list_for_each_entry(iter_task, &task_list, list) {
		if(iter_task->status == READY) {
			// period is smaller, it has a higher priority
			if(high_prio_period_ms > iter_task->period_ms){
				highest_task = iter_task;
				high_prio_period_ms = iter_task->period_ms;
			}
		}
	}
	mutex_unlock(&task_list_lock);
	return highest_task;
}

/*
 *	static void preempt_old_task
 *  input: struct mp2_task_struct* , old_task : old task to preempt
 *  the dispatching thread should preempt the old task for context switch
 *  It preempts an old task by setting its scheduling policy to normal and
 *  changing its status to READY.
 */
static void preempt_old_task(mp2_task_struct* old_task){
	struct sched_attr old_attr;
	if(old_task){
		old_attr.sched_policy = SCHED_NORMAL;
		old_attr.sched_priority = 0;
		sched_setattr_nocheck(old_task->linux_task, &old_attr);
		old_task->status = READY;
 	}
}

/*
 *	static void set_new_task
 *  input: struct mp2_task_struct* , new_task : new task 
 *  Sets a new task by waking it up and setting its scheduling policy to 
 *  FIFO. Its status will be set up RUNNING
 */
static void set_new_task(mp2_task_struct* new_task){
 	struct sched_attr new_attr;
 	if(new_task){
		wake_up_process(new_task->linux_task);
		new_attr.sched_policy = SCHED_FIFO;
		new_attr.sched_priority = 99;
		sched_setattr_nocheck(new_task->linux_task, &new_attr);
		new_task->status = RUNNING;
	}
}

/*
 *	static int dispatching_kthreadfn
 *  input: void* data;
 *  Implementing the Dispatching Thread based on different status of the current
 *  and next tasks and do the context switch(preempt the old task and set the new
 *  task)
 */
static int dispatching_kthreadfn(void *data) {
	struct mp2_task_struct *higest_task;
	while (!kthread_should_stop()) { 
		higest_task = NULL;
		higest_task = find_highest_priority_task();
		// printk("highest_task: 0x%llx\n", (u64)higest_task);
		mutex_lock(&task_list_lock);
		// if the current task is null,  pick the one in the task_list with highest
		//  priority and context switch to it if possible
		if(!curr_t){
			if(higest_task) set_new_task(higest_task);
			curr_t = higest_task;	
		}
		// if the current task is SLEEPING,  do the preemption of the current task and 
		// pick the one in the task_list with highest priority and context switch to it 
		// if possible
		else if(curr_t->status == SLEEPING){
			if(higest_task) set_new_task(higest_task);
			preempt_old_task(curr_t);
			curr_t = higest_task;	
		}
		// if the current task is RUNNING, pick the one in the task_list with highest priority.
		// If its priority is greater than the current task, take it as the new task and context switch
		// to it by setting the newtask and preempting the old task. Else, do nothing: the current task
		// keeps running
		else if(curr_t->status == RUNNING){
			if(higest_task->period_ms < curr_t->period_ms){
				set_new_task(higest_task);
				preempt_old_task(curr_t);
				curr_t = higest_task;	
			}
		}
		// update dispatching thread and state of tasks
		mutex_unlock(&task_list_lock);
		set_current_state(TASK_INTERRUPTIBLE); 
		schedule();
	}

    return 0;
}

/*
 * 	static int deregistration(pid_t pid)
 *	input: pid of the task we want to deregister
 *  Output: int 0 if sucess. Else, unsuccessful operation
 *	It remove the task from the list and free all data structures allocated during registration.
 */
static int deregistration(pid_t pid){
	struct mp2_task_struct *del_task, *temp;
	// deregister the task with locks to prevent interruption and race condition
	mutex_lock(&task_list_lock);
	list_for_each_entry_safe(del_task, temp, &task_list, list) {
		if(del_task->pid == pid){
			if (del_task == curr_t) {
				curr_t = NULL;
			}
			// free all data structure allocated 
			list_del(&del_task->list);
			del_timer_sync(&del_task->wakeup_timer);
			kmem_cache_free(task_cache, del_task);
			mutex_unlock(&task_list_lock);
			printk("Successful Deregistration");
			return 0;
		}	
	}
	mutex_unlock(&task_list_lock);
	printk("Unsuccessful Deregistration");
	return -EINVAL;
}

/*
 * 	static ssize_t write_callback_mp2(struct file *file, const char __user *user_buffer, size_t count, loff_t *pos)
 *	input: struct file *file, const char __user *user_buffer, size_t count, loff_t *pos
 *  Output: ssize_t byte of writing
 *	Write callback function handle task registration, yielding, and deregistration commands. Based on the command, 
 *  it will shift to different handler and do the operation based on the value from the user space.
 */
static ssize_t write_callback_mp2(struct file *file, const char __user *user_buffer, size_t count, loff_t *pos){
	pid_t pid;
	unsigned long period_ms, runtime_ms;
	char *kbuffer;
	// allocate kbuffer
	kbuffer = kmalloc(count+1, GFP_KERNEL);
	// return error if allocation fails
	if(!kbuffer){
		printk("Error kmalloc kbuffer memory");
		return -ENOMEM;
	}
	// copy the writing pid value from user space
    if (copy_from_user(kbuffer, user_buffer, count)){
		kfree(kbuffer);
		printk("Error copy from user");
		return -EFAULT;
	}
	// Convert string value of pid info to integer
    kbuffer[count] = '\0';
	// switch and handle register, yield, and deregister these two commands
	switch(kbuffer[0]){
		// register
		case 'R':
			// R,PID,period_ms,runtime_ms
			if(sscanf(kbuffer, "R,%d,%lu,%lu", &pid, &period_ms, &runtime_ms) == 3){
				if(registration(pid, period_ms, runtime_ms) < 0){
					kfree(kbuffer);
					printk("Error Fail to registration");
					return -EINVAL;
				}
			}
			else{
				kfree(kbuffer);
				printk("Error Registration has no enough info");
				return -EINVAL;
			}
			break;
		// yield
		case 'Y':
			// Y,PID
			if(sscanf(kbuffer, "Y,%d", &pid) == 1){
				yield_handler_mp2(pid);
			}
			else{
				kfree(kbuffer);
				printk("Error Yield has no enough info");
				return -EINVAL;
			}
			break;
		// deregister
		case 'D':
			if(sscanf(kbuffer, "D,%d", &pid) == 1){
				if(deregistration(pid) < 0){
					kfree(kbuffer);
					printk("Error Fail to deregistration");
					return -EINVAL;
				}
			}
			else{
				kfree(kbuffer);
				printk("Error De-registration has no enough info");
				return -EINVAL;
			}
			break;
		default: 
			kfree(kbuffer);
			printk("Error No desired message");
			return -EINVAL;
	}
	// free kbuffer
	kfree(kbuffer);
    return count;
}


// struct proc_ops : Defline the operation for the proc entry
static const struct proc_ops ops_mp2 = {
	.proc_read  = read_callback_mp2,
	.proc_write  = write_callback_mp2,
};

// mp2_init - Called when module is loaded
// I/O : return 0 if the module initialzes. Else, return error
// implement the Proc Filesystem entries (i.e /proc/mp1/status). 
// 		1. Create a Proc Filesystem folder /proc/mp1 
// 		2. Create the file /proc/mp1/status. 
// 		3. Initialize kthread, kmem_cache, etc
// initialize timer, work, and workqueue 
int __init mp2_init(void)
{
#ifdef DEBUG
	printk(KERN_ALERT "MP2 MODULE LOADING\n");
#endif
	// Insert your code here ...
	pr_warn("Hello, world\n");
	// create a Proc Filesystem folder /proc/mp2/
	proc_dir_mp2 = proc_mkdir("mp2",NULL);
	if(!proc_dir_mp2){
		printk("Error Fail to create mp2 Proc folder");
		return -ENOMEM; 
    }
	// create the file /proc/mp2/status.
	proc_entry_mp2 = proc_create("status", 0666, proc_dir_mp2, &ops_mp2);
	if(!proc_entry_mp2){
		printk("Error Fail to create mp2 Proc entry");
		return -ENOMEM;
    }
	// initialize kthread responsible for triggering the context switches as needed
	kthread = kthread_run(dispatching_kthreadfn, NULL, "kthread");
	if (!kthread) {
		printk("Error Fail to create kthread");
		return -ENOMEM;
	}
	// create kmem cache for mp2_task
	task_cache = kmem_cache_create("mp2_task", sizeof(struct mp2_task_struct), 0, SLAB_HWCACHE_ALIGN, NULL);
    if (!task_cache) {
        printk("Error Fail to create mp2 task cache");
        return -ENOMEM;
    }
	printk(KERN_ALERT "MP2 MODULE LOADED\n");
	return 0;
}

// mp2_exit - Called when module is unloaded
// I/O : return 0 if the module initialzes; else return error
// remove the entire proc folder and proc entry
// free the kernel linked list and the corresponding data structure in each 
// task, such as delete timer, free kmem cache.
// destory kmem_cache, kthread, etc
void __exit mp2_exit(void)
{
	struct mp2_task_struct* del_task;
    struct mp2_task_struct* temp;
#ifdef DEBUG
	printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
#endif
	
	// Insert your code here ...
	kthread_stop(kthread);
	// remove proc entry
	// remove the entire proc folder
	if (proc_entry_mp2) remove_proc_entry("status", proc_dir_mp2);
	if (proc_dir_mp2) proc_remove(proc_dir_mp2);  

	// free the kernel linked list with locks to prevent interruption and race condition
	mutex_lock(&task_list_lock);
	list_for_each_entry_safe(del_task, temp, &task_list, list) {
		list_del(&del_task->list);
		kmem_cache_free(task_cache, del_task);
		del_timer_sync(&del_task->wakeup_timer);
	}
	mutex_unlock(&task_list_lock);
	kmem_cache_destroy(task_cache);
	printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);


