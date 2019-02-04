#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/export.h>
#include <asm/errno.h>

/*For each probe you need to allocate a kprobe structure*/

static inline int is_kernel_text(unsigned long addr)
{
	if ((addr >= (unsigned long)_stext && addr <= (unsigned long)_etext) ||
	    arch_is_kernel_text(addr))
		return 1;
	return in_gate_area_no_mm(addr);
}

int handler_pre(struct kprobe *p, struct pt_regs *regs)
{

	printk("kprobe pre_handler: dump_stack at p->addr=0x%p, ip=%lx, flags=0x%lx, id=%d\n",
	p->addr, regs->ip, regs->flags, p->id);
	printk("HMODE: %d %d %d\n", p->dumpmode, p->owner_id, p->access_id);
	if (p->dumpmode == 0) 
	{
		if (p->access_id == current->pid)
		{
			dump_stack();
		}
		else
		{
			printk("DUMPMODE: 0, ACCESS DENIED\n");
		}
	}
	else if (p->dumpmode == 1) 
	{
		if (p->access_id == current->tgid)
		{
			dump_stack();
		}
		else {printk("DUMPMODE: 1, ACCESS DENIED\n");}	
	}
	else 
	{
		dump_stack();
	}
	//dump_stack();
	return 0;
}

void handler_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	printk("kprobe post_handler: p->addr=0x%p, flags=0x%lx\n",
	p->addr, regs->flags);
}


int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr)
{
	printk("kprobe fault_handler: p->addr=0x%p, trap #%dn",
		p->addr, trapnr);
	/* Return 0 because we don't handle the fault. */
	return 0;
}


//SYSCALL_DEFINE1(insdump, unsigned int, fd)
SYSCALL_DEFINE2(insdump, const char __user *, symbolname, int, mode)
{
	int ret;
	unsigned long addr;
	struct kprobe *kp;
	struct kprobe_list *kp_list;

	kp = (struct kprobe *)kmalloc(sizeof(struct kprobe), GFP_KERNEL);
	kp_list = (struct kprobe_list *)kmalloc(sizeof(struct kprobe_list), GFP_KERNEL);

	pr_info("SYSCALL: INSDUMP %d %s\n", mode, symbolname);
	
	kp->pre_handler = handler_pre;
	kp->post_handler = handler_post;
	kp->fault_handler = handler_fault;
	kp->addr = (kprobe_opcode_t*) kallsyms_lookup_name(symbolname);//sys_gettid
	addr = (unsigned long) kallsyms_lookup_name(symbolname);
	ret = is_kernel_text(addr);
	if(ret != 1){
		printk("Kernel function is not in text section\n");
		return -EINVAL;
	}
	kp->id = current->pid;
	kp_list->id = current->pid;
	pr_info("OPCODE: %lu\n", (unsigned long) kp->addr);
	
	/* register the kprobe now */
	if (!kp->addr) 
	{
		printk("Couldn't find %s to plant kprobe\n", symbolname);
		return -1;
	}

	//DUMPMODE
	kp->dumpmode = mode;
	if(mode == 0)
	{
		kp->owner_id = current->pid;
		kp->access_id = current->pid;
	}
	else if (mode==1)
	{
		kp->owner_id = current->pid;
		kp->access_id = current->tgid;
	}
	else
	{
		printk("MODE: Any Process\n");
		kp->owner_id = current->pid;
		kp->access_id = -1;
	}

	kp_list->kp = kp;

	if ((ret = register_kprobe_list(kp_list) < 0)) 
	{
		printk("register_kprobe failed, returned %d\n", ret);
		return -1;
	}
	printk("kprobe registered\n");

	return kp->id;
}

//SYSCALL_DEFINE1(rmdump, unsigned int, fd)
SYSCALL_DEFINE1(rmdump, unsigned int, dumpid)
{
	pr_info("SYSCALL: DUMP\n");

	pr_info("DUMP ID: %d\n", dumpid);
	//Note that this funciton does not return any value. So, we cannot return EINVAL if it fails
	unregister_kprobe_by_id(dumpid);
	printk("kprobe unregistered\n");
	
	return 0;
}

//Removes active kprobes related to a process
SYSCALL_DEFINE0(remove_active_kprobes)
{
	pr_info("Removing Active Kprobes of pid:%d\n",current->pid);
	remove_active_kprobes();
	return 0;
}

SYSCALL_DEFINE0(active_kprobes)
{
	int active_kprobes;
	active_kprobes = get_active_kprobes();
	return active_kprobes;
}