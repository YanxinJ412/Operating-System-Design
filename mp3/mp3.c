#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>   
#include <uapi/linux/sched/types.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include "mp3_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_ID");
MODULE_DESCRIPTION("CS-423 MP3");

#define DEBUG 1
#define LINE_LEN 64
// Sampling rate is 20 times per second
#define DELAY 1000/20

#define BUFFER_SIZE 128*4096
#define DEV_NAME "device_mp3"
#define MAJOR_NUM 423
#define MINOR_NUM 0

struct task_struct* find_task_by_pid(unsigned int nr);
int get_cpu_use(int pid, unsigned long *min_flt, unsigned long *maj_flt,
               unsigned long *utime, unsigned long *stime);
// Process Control Block (PCB)
typedef struct mp3_task_struct {
	pid_t pid;
	struct list_head list;
} mp3_task_struct;

static void *membuf = NULL;
static int buf_offset = 0;
// proc directory and entry
static struct proc_dir_entry *proc_dir_mp3, *proc_entry_mp3;
// slab allocator
static struct kmem_cache *task_cache;
// create work
static void work_handler_mp3(struct work_struct *work);
DECLARE_DELAYED_WORK(delayed_work_mp3, work_handler_mp3);
static struct workqueue_struct *workqueue_mp3;
// amount of tasks
static int task_count = 0;
// initialize kernel linked list of process and its lock
LIST_HEAD(task_list);
DEFINE_MUTEX(task_list_lock);
// device drivers
static dev_t dev_mp3;    
static struct cdev cdev_mp3;    

int __init mp3_init(void);
void __exit mp3_exit(void);



/*	read_callback_mp3
 *	Input: struct timer_list *timer, user_buffer, pos
 *	Read callback function aims to read task information from the /proc/status/mp2 file. When a process 
 *  reads the Proc file, the file should return pid of the current process.
 */
static ssize_t read_callback_mp3(struct file *file, char __user *user_buffer, size_t count, loff_t *pos){
	int read_count = 0;
	struct mp3_task_struct *read_task;
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
	// iterate through link and read the pid to kbuffer
	list_for_each_entry(read_task, &task_list, list){ 
		int offset = snprintf(line_buffer, LINE_LEN, "PID: %d\n", read_task->pid);
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

// Registers a new task with specified parameters (pid).
/*	
 *	Registration
 *	Input: pid_t pid
 *	Registration function registers a new task with provided pid
 *  Initialize the new task;
 */
static int Registration(pid_t pid){
   struct mp3_task_struct *new_task;
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
   	new_task->pid = pid;
	// If this is the first task, add new work into the workqueue
   	if(task_count == 0){
		queue_delayed_work(workqueue_mp3, &delayed_work_mp3, msecs_to_jiffies(DELAY));
   	}
	task_count++;
	// add the new process node into the linked list
	list_add_tail(&new_task->list, &task_list);
	mutex_unlock(&task_list_lock);
	printk("Successful Registration");
	return 0;
}


/*
 * static int Unregistration(pid_t pid)
 *	Input: pid of the task we want to deregister
 *  Output: int 0 if sucess. Else, unsuccessful operation
 *	It remove the task from the list and free all data structures allocated during registration.
 */
static int Unregistration(pid_t pid){
	struct mp3_task_struct *del_task, *temp;
	// deregister the task with locks to prevent interruption and race condition
	mutex_lock(&task_list_lock);
	list_for_each_entry_safe(del_task, temp, &task_list, list) {
		if(del_task->pid == pid){
			// free all data structure allocated 
			list_del(&del_task->list);
			kmem_cache_free(task_cache, del_task);
			mutex_unlock(&task_list_lock);
			printk("Successful Deregistration");
			// flush workqueue if no task exists
			task_count--;
			// if the unregistered task is the last task in the task_list, flush and cancel the work and workqueue
			if(task_count == 0){
				cancel_delayed_work_sync(&delayed_work_mp3);
				flush_workqueue(workqueue_mp3);
			}
			return 0;
		}	
	}
	mutex_unlock(&task_list_lock);
	printk("Unsuccessful Deregistration");
	return -EINVAL;
}

/*
 * static ssize_t write_callback_mp3(struct file *file, const char __user *user_buffer, size_t count, loff_t *pos)
 *	Input: struct file *file, const char __user *user_buffer, size_t count, loff_t *pos
 * Output: ssize_t byte of writing
 *	Write callback function handle task registration, yielding, and deregistration commands. Based on the command, 
 * it will shift to different handler and do the operation based on the value from the user space.
 */
static ssize_t write_callback_mp3(struct file *file, const char __user *user_buffer, size_t count, loff_t *pos){
	pid_t pid;
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
			// R PID
			if(sscanf(kbuffer, "R %d", &pid) == 1){
				if(Registration(pid) < 0){
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
		// unregister
		case 'U':
			// U PID
			if(sscanf(kbuffer, "U %d", &pid) == 1){
				if(Unregistration(pid) < 0){
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

/*
 * static void work_handler_mp3(struct work_struct *work)
 * Input: struct work_struct *work
 * Output: None
 * use a delayed work queue that periodically measures the major and minor page fault counts, and 
 * CPU utilization of all registered user processes and saves the measured information to the memory buffer. 
 */
static void work_handler_mp3(struct work_struct *work){
	struct mp3_task_struct *work_proc, *temp;
	unsigned long total_min_flt = 0, total_maj_flt = 0, total_cpu_util = 0;
	unsigned long *membuf_start = (unsigned long *)membuf;
	// iterate through the linked list and update the cpu time usage
	mutex_lock(&task_list_lock);
	list_for_each_entry_safe(work_proc, temp, &task_list, list) {
		unsigned long min_flt = 0, maj_flt = 0, utime = 0, stime = 0;
		if (get_cpu_use(work_proc->pid, &min_flt, &maj_flt, &utime, &stime) == 0){
        	// accumulate minor and major page fault and cpu utilization
        	total_min_flt += min_flt;
			total_maj_flt += maj_flt;
			total_cpu_util += (utime + stime);
    	} 
		else {
			// Error occurs or the process is dead, delete the process
			list_del(&work_proc->list);
			kfree(work_proc);
		}
	}
	mutex_unlock(&task_list_lock);
	// store the page fault count and cpu utilization into the buffer at the current time (jiffies)
	if(buf_offset + 4*sizeof(unsigned long) < BUFFER_SIZE){
		membuf_start[buf_offset++] = jiffies;           
		membuf_start[buf_offset++] = total_min_flt; 
		membuf_start[buf_offset++] = total_maj_flt;
		membuf_start[buf_offset++] = total_cpu_util;  
	}
	// set up the delay so that the sampling rate becomes 20 times per second
	queue_delayed_work(workqueue_mp3, &delayed_work_mp3, msecs_to_jiffies(DELAY));

}

/*
 * static int open_callback_mp3(struct inode *, struct file *)
 * simply return 0 for character device driver set up
 */
static int open_callback_mp3(struct inode *, struct file *){
   	return 0;
}

/*
 * static int release_callback_mp3(struct inode *, struct file *)
 * simply return 0 for character device driver set up
 */
static int release_callback_mp3(struct inode *, struct file *){
   	return 0;
}

/*
 * static int mmap_callback_mp3(struct file *filp, struct vm_area_struct *vma)
 * Input: struct file *filp, struct vm_area_struct *vma
 * Output: return 0 if succeed
 * Map buffer memory into the virtual address space of a user process upon request. We need to map
 * the physical pages of the buffer to the virtual address space of a requested user process.
 */
static int mmap_callback_mp3(struct file *filp, struct vm_area_struct *vma){
	void *membuf_start;
	unsigned long pfn, size, vma_start_;
	// pointer for memory buffer to map physical pages to virtual address space
	membuf_start = membuf;
	// counter for buffer size
	size = vma->vm_end - vma->vm_start;
	// pointer for virtual page address of virtual address space of a user process upon request
	vma_start_ = vma->vm_start;
	// iterate through the entire memory buffer
	while(size > 0){
		// find physical address of each page in memory buffer
		pfn = vmalloc_to_pfn(membuf_start);
		if (pfn == -EINVAL) {
			printk("Fail to find corresponding physical address\n");
			return  -EINVAL;
		}
		// map the page of physical address to the virtual address requested
		if(remap_pfn_range(vma, vma_start_, pfn, PAGE_SIZE/4, PAGE_SHARED) < 0){
			printk("Fail to map the address area\n");
    		return -EINVAL;
		}
		// change the pointer position of memory buffer and virtual address space request
		membuf_start += PAGE_SIZE;
		vma_start_ += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
   	return 0;
}

// struct file_operations : Defline character device driver
static struct file_operations fops_mp3 = {
   .open = open_callback_mp3,
   .release = release_callback_mp3,
   .mmap = mmap_callback_mp3,
   .owner = THIS_MODULE,
};

// struct proc_ops : Defline the operation for the proc entry
static const struct proc_ops ops_mp3 = {
	.proc_read  = read_callback_mp3,
	.proc_write  = write_callback_mp3,
};

// mp3_init - Called when module is loaded
int __init mp3_init(void)
{
	// dev_t dev_mp3;
	#ifdef DEBUG
	printk(KERN_ALERT "MP3 MODULE LOADING\n");
	#endif
	// Insert your code here ...
	// create a Proc Filesystem folder /proc/mp3/
	proc_dir_mp3 = proc_mkdir("mp3",NULL);
	if(!proc_dir_mp3){
		printk("Error Fail to create mp3 Proc folder");
		return -ENOMEM; 
    }
	printk(KERN_ALERT "done proc_dir_mp3\n");
	// create the file /proc/mp3/status.
	proc_entry_mp3 = proc_create("status", 0666, proc_dir_mp3, &ops_mp3);
	if(!proc_entry_mp3){
		remove_proc_entry("mp3", NULL);  
		printk("Error Fail to create mp3 Proc entry");
		return -ENOMEM;
    }
	printk(KERN_ALERT "done proc_entry_mp3\n");
	// initialize memory buffer
	membuf = vmalloc(BUFFER_SIZE);
	if (!membuf) {
		remove_proc_entry("status", proc_dir_mp3);
		remove_proc_entry("mp3", NULL);  
		printk(KERN_ALERT "MP3: Failed to allocate memory for membuf\n");
		return -ENOMEM;
	}
    memset(membuf, -1, BUFFER_SIZE);
	// initialize character device driver
	dev_mp3 = MKDEV(MAJOR_NUM, MINOR_NUM);
	if(register_chrdev_region(dev_mp3, 1, DEV_NAME) < 0){
		remove_proc_entry("status", proc_dir_mp3);
		remove_proc_entry("mp3", NULL);  
		vfree(membuf);
		printk("Error Fail to register chrdev region");
		return -ENOMEM;
	}
	cdev_init(&cdev_mp3, &fops_mp3);
	if(cdev_add(&cdev_mp3, dev_mp3, 1) < 0){
		remove_proc_entry("status", proc_dir_mp3);
		remove_proc_entry("mp3", NULL);  
		vfree(membuf);
		unregister_chrdev_region(dev_mp3, 1);
		printk("Error Fail to add cdev_mp3");
		return -ENOMEM;
	}
	// create kmem cache for mp3_task
	task_cache = kmem_cache_create("mp3_task", sizeof(struct mp3_task_struct), 0, SLAB_HWCACHE_ALIGN, NULL);
    if (!task_cache) {
		remove_proc_entry("status", proc_dir_mp3);
		remove_proc_entry("mp3", NULL);  
		vfree(membuf);
		unregister_chrdev_region(dev_mp3, 1);
		cdev_del(&cdev_mp3);
        printk("Error Fail to create mp3 task cache");
        return -ENOMEM;
    }
	// create workqueue
	workqueue_mp3 = create_workqueue("mp3 workqueue");
	if (!workqueue_mp3) {
		remove_proc_entry("status", proc_dir_mp3);
		remove_proc_entry("mp3", NULL);  
		vfree(membuf);
		unregister_chrdev_region(dev_mp3, 1);
		cdev_del(&cdev_mp3);
		kmem_cache_destroy(task_cache);
    	printk("Error Failed to create workqueue\n");
		return -ENOMEM;
	}
	
   
   printk(KERN_ALERT "MP3 MODULE LOADED\n");
   return 0;   
}

// mp3_exit - Called when module is unloaded
void __exit mp3_exit(void)
{
   struct mp3_task_struct* del_task;
   struct mp3_task_struct* temp;
   #ifdef DEBUG
   printk(KERN_ALERT "MP3 MODULE UNLOADING\n");
   #endif
   // Insert your code here ...
	// remove proc entry and the entire proc folder
	if (proc_entry_mp3) remove_proc_entry("status", proc_dir_mp3);
	printk(KERN_ALERT "proc_entry_mp3\n");
	if (proc_dir_mp3) remove_proc_entry("mp3", NULL);  
	printk(KERN_ALERT "proc_dir_mp3\n");
	if (membuf) vfree(membuf);
	printk(KERN_ALERT "membuf\n");
	// free the kernel linked list with locks to prevent interruption and race condition
	mutex_lock(&task_list_lock);
	list_for_each_entry_safe(del_task, temp, &task_list, list) {
		list_del(&del_task->list);
		kmem_cache_free(task_cache, del_task);
	}
	mutex_unlock(&task_list_lock);
	kmem_cache_destroy(task_cache);
	// unregister and delete character device driver
	cdev_del(&cdev_mp3);
	unregister_chrdev_region(dev_mp3, 1);
	// stop running workqueue and work
   	cancel_delayed_work_sync(&delayed_work_mp3);
	if(workqueue_mp3){
		destroy_workqueue(workqueue_mp3);
	}
   	printk(KERN_ALERT "MP3 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp3_init);
module_exit(mp3_exit);
