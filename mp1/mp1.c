// SPDX-License-Identifier: GPL-2.0-only
/*
 * This module emits "Hello, world" on printk when loaded.
 *
 * It is designed to be used for basic evaluation of the module loading
 * subsystem (for example when validating module signing/verification). It
 * lacks any extra dependencies, and will not normally be loaded by the
 * system unless explicitly requested by name.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include "mp1_given.h"

// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
// Please put your name and email here
MODULE_AUTHOR("Yanxin Jiang <yanxinj2@illinois.edu>");
MODULE_LICENSE("GPL");

#define LINE_LEN 64
#define TIMEOUT 5000
#define MAX_BUFFER_SIZE 4096

int get_cpu_use(int pid, unsigned long *cpu_use);
// proc directory and entry
static struct proc_dir_entry *proc_dir_mp1, *proc_entry_mp1;
// process struct 
typedef struct process_mp1{
    pid_t pid;
    unsigned long cpu_use;
    struct list_head list;
} process_mp1;
// timer, work and workqueue
static struct timer_list timer_mp1;
static struct work_struct work_mp1;
static struct workqueue_struct *workqueue_mp1;
// initialize kernel linked list of process and its lock
LIST_HEAD(process_list);
DEFINE_MUTEX(process_lock);

// It is used for creating the file /proc/mp1/status when module Initialization
// so that the users can read the file
// I/O: 
// 		Parameter: file - corresponding proc entry
//		           user_buffer - the kernel linked list will be copied to it
//                 count - number of byte to read
//                 pos - offset value for the cursor of file read
//      Output:    byte : number of byte read and copy if read sucessfully
//                 Else, return error
// When a process reads the Proc file, the file should return a list of registered
// processes together with their corresponding User Space CPU Time (known also as 
// user time) shown as <PID1>: <CPU Time of PID1>
static ssize_t read_callback_mp1(struct file *file, char __user *user_buffer, size_t count, loff_t *pos){
	int read_count = 0;
	struct process_mp1 *read_proc;
	char line_buffer[LINE_LEN];
	char *kbuffer;
	ssize_t byte;
	// allocate read buffer to store linked list value
	kbuffer = kmalloc(count+*pos, GFP_KERNEL);
	if(!kbuffer){
		printk(KERN_INFO "Error kmalloc kbuffer memory");
		return -ENOMEM;
	}
	// lock the proess list for safe access
	mutex_lock(&process_lock);
	// iterate through link and read the pid and its cpu usage to kbuffer
	list_for_each_entry(read_proc, &process_list, list){
		int offset = snprintf(line_buffer, LINE_LEN, "%d: %lu\n", read_proc->pid, read_proc->cpu_use);
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
		mutex_unlock(&process_lock);
		printk(KERN_INFO "Error already read");
		kfree(kbuffer);
		return 0;
	}
	// copy the read data to user
	if (copy_to_user(user_buffer, kbuffer+*pos, byte)) {
		mutex_unlock(&process_lock);
		printk(KERN_INFO "Error copy to user");
		kfree(kbuffer);
		return -EFAULT;  
	}
	// update the position of file's reading pointer
	*pos += byte;
	// unlock the process list
	mutex_unlock(&process_lock);
	
	return byte;


}



// It is used for creating the file /proc/mp1/status when module Initialization
// so that the users can write the file
// I/O: 
// 		Parameter: file - corresponding proc entry
//		           user_buffer - the kernel linked list will be copied to it
//                 count - number of byte to read
//                 pos - offset value for the cursor of file read
//      Output:    count : number of byte read and copy if read sucessfully
//                 Else, return error
// When a process writes a PID (Process Identifier) into the Proc file, the process
// corresponding to the PID should be registered in the module for User Space CPU 
// Time measurement. Multiple PIDs may be registered. The written message be a decimal
// string, which contains the PID.
static ssize_t write_callback_mp1(struct file *file, const char __user *user_buffer, size_t count, loff_t *pos){
	pid_t pid;
    struct process_mp1 *write_proc;
	char *kbuffer;
	// allocate kbuffer
	kbuffer = kmalloc(count+1, GFP_KERNEL);
	// return error if allocation fails
	if(!kbuffer){
		printk(KERN_INFO "Error kmalloc kbuffer memory");
		return -ENOMEM;
	}
	// copy the writing pid value from user space
    if (copy_from_user(kbuffer, user_buffer, count)){
		kfree(kbuffer);
		printk(KERN_INFO "Error copy from user");
		return -EFAULT;
	}
	// Convert string value of pid info to integer
    kbuffer[count] = '\0';
    if (kstrtoint(kbuffer, 10, &pid)){
		kfree(kbuffer);
		return -EINVAL;
	}
	// lock process lock 
	mutex_lock(&process_lock);
	// iterate through linked list to check if PID already exists
	list_for_each_entry(write_proc, &process_list, list) {
		if(write_proc->pid == pid) {
			mutex_unlock(&process_lock);
			kfree(kbuffer);
			printk(KERN_INFO "Error PID already exists");
			return -EINVAL;
		}
	}
	// mutex_unlock(&process_lock);
	// update the new process node, allocate its memory firstly
	write_proc = kmalloc(sizeof(struct process_mp1), GFP_KERNEL);
	if(!write_proc) {
		kfree(kbuffer);
		printk(KERN_INFO "Error kmalloc write_proc");
		return -ENOMEM;
	}
    write_proc->pid = pid;
    write_proc->cpu_use = 0;
	// add the new process node into the linked list
    // mutex_lock(&process_lock);
    list_add_tail(&write_proc->list, &process_list);
    mutex_unlock(&process_lock);
	// free kbuffer
	kfree(kbuffer);
    return count;
}

// timer_callback_mp1
// timer callback function to implement the periodic update functionality
// I/O:
//		Parameter: timer - struct timer
// When your timer is fired, this function should use Kernel Workqueue to 
// schedule an user time update work 
static void timer_callback_mp1(struct timer_list *timer){
	// use workqueue to schedule an user time update work (Top-Half)
	queue_work(workqueue_mp1, &work_mp1);
	// re-enable this function to make the timer periodically
	mod_timer(timer, jiffies + msecs_to_jiffies(TIMEOUT));
}

// work_handler_mp1
// work handler function to updates to the CPU Times for the processes
// in the linked list
// I/O:
//		Parameter: work - struct work
// The user time update work should, when exiting the Workqueue, iterate 
// the registered process list and update the user time (Bottom-Half).
static void work_handler_mp1(struct work_struct *work){
	struct process_mp1 *work_proc, *temp;
	unsigned long cpu_use;
	// iterate through the linked list and update the cpu time usage
	mutex_lock(&process_lock);
	list_for_each_entry_safe(work_proc, temp, &process_list, list) {
		if (get_cpu_use(work_proc->pid, &cpu_use) == 0) {
        	// Process is still alive, return 0 and provide cpu time usage 
        	work_proc->cpu_use = cpu_use;
    	} 
		else {
			// Error occurs or the process is dead, delete the process
			list_del(&work_proc->list);
			kfree(work_proc);
		}
	}
	mutex_unlock(&process_lock);
}


// struct proc_ops : Defline the operation for the proc entry
static const struct proc_ops ops_mp1 = {
	.proc_read  = read_callback_mp1,
	.proc_write  = write_callback_mp1,
};

// Test Module Initialization
// I/O : return 0 if the module initialzes. Else, return error
// implement the Proc Filesystem entries (i.e /proc/mp1/status). 
// 		1. Create a Proc Filesystem folder /proc/mp1 
// 		2. Create the file /proc/mp1/status. 
// initialize timer, work, and workqueue 
static int __init test_module_init(void)
{	
	pr_warn("Hello, world\n");
	// create a Proc Filesystem folder /proc/mp1/
	proc_dir_mp1 = proc_mkdir("mp1",NULL);
	if(!proc_dir_mp1){
		printk(KERN_INFO "Error creating mp1 Proc folder");
		return -ENOMEM; 
    }
	// create the file /proc/mp1/status.
	proc_entry_mp1 = proc_create("status", 0666, proc_dir_mp1, &ops_mp1);
	if(!proc_entry_mp1){
		printk(KERN_INFO "Error creating mp1 Proc entry");
		return -ENOMEM;
    }
	// setup your timer to call my_timer_callback
	timer_setup(&timer_mp1, timer_callback_mp1, 0);
	// setup timer interval to based on TIMEOUT such that the timer wake 
	// up every 5sec
	mod_timer(&timer_mp1, jiffies + msecs_to_jiffies(TIMEOUT));
	// create workqueue
	workqueue_mp1 = create_workqueue("mp1 workqueue");
	// create work
	INIT_WORK(&work_mp1, work_handler_mp1);
	// Module Proc initialize sucessfully
	// printk(KERN_INFO "Proc initialized");
	return 0;
}

module_init(test_module_init);

// Test Module Exit
// I/O : return 0 if the module initialzes; else return error
// remove the entire proc folder and proc entry
// free the kernel linked list
// stop running timer, work, and workqueue 
static void __exit test_module_exit(void)
{
	struct process_mp1 *del_proc, *temp;
	// remove proc entry
	if(proc_entry_mp1){
		remove_proc_entry("status",proc_dir_mp1);
	}
	// remove the entire proc folder
	if(proc_dir_mp1){
		proc_remove(proc_dir_mp1);
	}
	// free the kernel linked list with locks to prevent interruption and race condition
	mutex_lock(&process_lock);
	list_for_each_entry_safe(del_proc, temp, &process_list, list) {
		list_del(&del_proc->list);
		kfree(del_proc);
	}
	mutex_unlock(&process_lock);
	// stop running timer and work
	del_timer_sync(&timer_mp1);
	cancel_work_sync(&work_mp1);
	// flush_work(&work_mp1);

	// stop running workqueue
	if(workqueue_mp1){
		destroy_workqueue(workqueue_mp1);
	}
	// flush_workqueue(workqueue_mp1);
	pr_warn("Goodbye\n");
	
}

module_exit(test_module_exit);
