#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/kprobes.h>
#include <linux/uaccess.h>
#include <linux/ptrace.h>
#include <linux/thread_info.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/errno.h>
#include <asm/ptrace.h>

#define DEVICE_NAME "Mprobe"
#define SIZE_OF_BUFFER	10

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ravi Bhushan");
MODULE_DESCRIPTION("EOSI_Assignment_1");

/*Time Stamp Counter code*/
#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#elif defined(__x86_64__)


static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#endif

struct mprobe_dev {
	struct cdev cdev;
	int location;	
} *mprobe_devp;

static struct kprobe* kp;

//Mprobe Output 
typedef struct mprobe_user_output {
	void *kprobe_addr;
	uint64_t tsc;
	int pid;
	int value;
} user_output;

//Mprobe Ring Buffer
struct mprobe_ring_buffer {
	user_output *data;
	int   head;
	int   tail;
	unsigned long count;
	unsigned long loss;
} *mprobe_output;

//Input Buffer
struct mprobe_input_buffer {
	unsigned long *func_offset;
};

static dev_t mprobe_dev_number;
struct class *mprobe_dev_class;          
static struct device *mprobe_device;

static int __init mprobe_driver_init(void);
static void __exit mprobe_driver_exit(void);
static int mprobe_device_open(struct inode *, struct file *);
static int mprobe_device_release(struct inode *, struct file *);
static long mprobe_device_unregister(struct file *, unsigned int, unsigned long);
static ssize_t mprobe_device_read(struct file *, char *, size_t, loff_t *);
static ssize_t mprobe_device_write(struct file *, const char *, size_t, loff_t *);


static int mprobe_pre_handler(struct kprobe *p, struct pt_regs *regs) {
	
	uint64_t tsc;
	int value_at_address;
	struct task_struct* task = current;
	user_output probe_data;
	printk(KERN_INFO "Mprobe - KProbe hit.");
	probe_data.kprobe_addr = (void*) p->addr;
	//Pid of process by using the current macro
	probe_data.pid = task->pid;
	//TSC
	tsc =rdtsc();
	probe_data.tsc = tsc;
	value_at_address = *((int*)p->addr);
	probe_data.value = value_at_address;
	printk(KERN_INFO "Mprobe - Data captured by Kprobe\n");
	printk(KERN_INFO "Mprobe - Kprobe address: %p\n", probe_data.kprobe_addr);
	printk(KERN_INFO "Mprobe - Timestamp of probe: %lu\n", probe_data.tsc);
	printk(KERN_INFO "Mprobe - Process pid: %d\n",probe_data.pid);
	printk(KERN_INFO "Mprobe - Local variable value: %d\n",probe_data.value);
	/* Storing data in ring buffer*/				
	 if(mprobe_output->data )
	 {

		 memcpy(&(mprobe_output->data[mprobe_output->tail]), &probe_data, sizeof(mprobe_output));
		 printk(KERN_INFO "Mprobe - Storing in ring buffer\n");
		 mprobe_output->tail = (mprobe_output->tail +1) % SIZE_OF_BUFFER;
		  if(mprobe_output->tail == mprobe_output->head)
		  {
			  mprobe_output->loss++;
		  }
		  else
		  {
			  mprobe_output->count++;
		  }
 	 }
	return 0;
}

static void mprobe_post_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags) {
	printk(KERN_INFO "<%s> Mprobe post_handler: p->addr = 0x%p, flags = 0x%lx\n",
		p->symbol_name, p->addr, regs->flags);
}


static int mprobe_device_open(struct inode * inode, struct file * filp) {
	
	kp = NULL;
	mprobe_output = NULL;
	
	mprobe_output = kmalloc(sizeof(struct mprobe_ring_buffer), GFP_KERNEL);
		
	if (!mprobe_output) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}
	
	if(!(mprobe_output->data = kmalloc(sizeof(user_output)* SIZE_OF_BUFFER, GFP_KERNEL)))
	{		
		printk("Bad Kmalloc\n");
		 return -ENOMEM;
	}
		
	mprobe_output->head =0;
	mprobe_output->tail =0;
	mprobe_output->count=0;
	mprobe_output->loss =0;
	
	return 0;
}

static int mprobe_device_release(struct inode * inode, struct file * filp) {
	//clear mprobe_output if it exists
	if(mprobe_output != NULL) {
		kfree(mprobe_output->data);
		kfree(mprobe_output);
		mprobe_output = NULL;
	}
	if(kp != NULL) {
		unregister_kprobe(kp);
		kfree(kp);
		kp = NULL;
	}
	return 0;
}

static ssize_t mprobe_device_read(struct file * filp, char *buf, size_t count, loff_t *ppos) {

	int read_bytes,result;	
	if(mprobe_output!= NULL && mprobe_output->count >= 1) {
		int counter= mprobe_output->head;
		if (copy_to_user((struct mprobe_ring_buffer *)buf, &(mprobe_output->data[counter]) , sizeof(user_output))) {	
			printk(KERN_INFO "Mprobe - Unable to read data from ring buffer\n");
			result = -1;
			copy_to_user((int*)buf, &result, sizeof(int));
			return -EINVAL;
		}
		read_bytes = sizeof(user_output);
		mprobe_output->head = (mprobe_output->head + 1)% SIZE_OF_BUFFER;
		return read_bytes;
	} else {
		printk(KERN_INFO "Mprobe - Unable to read data from ring buffer\n");
		result = -1;
		copy_to_user((int*)buf, &result, sizeof(int));
		return -EINVAL;
	  
	}
}

static ssize_t mprobe_device_write(struct file *filp, const char *buf, size_t count, loff_t *ppos) {
	struct mprobe_input_buffer mprobe_input;
	int res;
	//unregister kprobe and delete mprobe_output if it exists
	if(mprobe_output != NULL) {
		kfree(mprobe_output->data);
		kfree(mprobe_output);
		mprobe_output = NULL;
	}
	if(kp != NULL) {
		unregister_kprobe(kp);
		kfree(kp);
		kp = NULL;
	}

	//get input from buf
	copy_from_user(&mprobe_input, (struct mprobe_input_buffer*)buf, sizeof(struct mprobe_input_buffer));

	//construct new kprobe from user input
	kp = kmalloc(sizeof(struct kprobe), GFP_KERNEL);
	memset(kp, 0, sizeof(struct kprobe));

	kp->addr = kallsyms_lookup_name("ht530_drv_driver_write") + (kprobe_opcode_t *)mprobe_input.func_offset;


	kp->pre_handler = mprobe_pre_handler;
	kp->post_handler = mprobe_post_handler;

	//register kprobe
	res = register_kprobe(kp);
	if(res<0) {
		printk(KERN_INFO "Mprobe - Unable to register kprobe, error=%d",res);
	} else {
		printk(KERN_INFO "Mprobe - Kprobe registered.\n");
	}

	return 0;
}

static long mprobe_device_unregister(struct file *filp,unsigned int cmd, unsigned long arg) {
	//clear mprobe_output if it exists
	if(mprobe_output != NULL) {
		kfree(mprobe_output->data);
		kfree(mprobe_output);
		mprobe_output = NULL;
	}
	if(kp != NULL) {
		unregister_kprobe(kp);
		kfree(kp);
		kp = NULL;
	}
	return 0;
}

//File Operations
static struct file_operations mprobe_fops = {
    .owner		    = THIS_MODULE,   /* Owner */    
    .open		    = mprobe_device_open,  /* Open Device */   
    .release	    = mprobe_device_release,  /* Close opened Device file */
    .write		    = mprobe_device_write, /* Register new Mprobe */    
    .read		    = mprobe_device_read, /* Retrive trace data from ring buffer */
    .unlocked_ioctl	= mprobe_device_unregister, /* ioctl removed since version 2.6.Unregister existing kprobe */
};

//Driver Initialization
static int __init mprobe_driver_init(void) {
	
	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&mprobe_dev_number, 0, 1, DEVICE_NAME) < 0) {	//Use cat /proc/devices/ to look up the driver
			printk(KERN_DEBUG "Mprobe - Can't register device\n"); 
			return -1;
	}
	printk(KERN_INFO "Mprobe - Major: %d, Minor: %d", MAJOR(mprobe_dev_number), MINOR(mprobe_dev_number));

	//Memory Allocation
	mprobe_devp = kmalloc(sizeof(struct mprobe_dev), GFP_KERNEL);
	if (!mprobe_devp) {
		printk("Mprobe - Kmalloc incomplete\n"); 
        return -ENOMEM;
	}

	printk(KERN_INFO "Mprobe - Memory Allocated");
	mprobe_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
	
	//CDEV - Connect
	cdev_init(&mprobe_devp->cdev, &mprobe_fops);
	mprobe_devp->cdev.owner = THIS_MODULE;
	if (cdev_add(&mprobe_devp->cdev, (mprobe_dev_number), 1)) {
		printk(KERN_INFO "Mprobe- CDEV_ADD failed");
        return 1;
	}
	
	mprobe_device = device_create(mprobe_dev_class, NULL, MKDEV(MAJOR(mprobe_dev_number), 0), NULL, DEVICE_NAME);
	printk("Mprobe - Device Created");
	printk(KERN_INFO "Mprobe - Driver Initiaized.\n");
	return 0;
}


//Driver Exit
static void __exit mprobe_driver_exit(void) {
	
	unregister_chrdev_region((mprobe_dev_number), 1);
	cdev_del(&mprobe_devp->cdev);
	
	device_destroy(mprobe_dev_class, MKDEV(MAJOR(mprobe_dev_number), 0));
	kfree(mprobe_devp);
	class_destroy(mprobe_dev_class);
	
	printk(KERN_INFO "Mprobe - Driver removed.\n");
}

module_init(mprobe_driver_init);
module_exit(mprobe_driver_exit);