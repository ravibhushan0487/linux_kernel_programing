#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/math64.h>
#include <asm/div64.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/math64.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/device.h>

#include "platform_device.h"


static int devices = 1;
module_param(devices, int, 0644);
MODULE_PARM_DESC(devices, "The number of HCSR devices to be created.");

static int pdevice_init(void)
{
	int index;
	char *device_name;
	struct Device_HCSR *HCSR_dev;
	struct platform_device *pl_temp2;
	int err;

	INIT_LIST_HEAD(&HCSR_devicelist.list);


	for (index=0; index<devices; index++)
	{
		HCSR_dev = (struct Device_HCSR *)kmalloc(sizeof(struct Device_HCSR), GFP_KERNEL);
		device_name = (char *)kmalloc(sizeof(char)*10, GFP_KERNEL);
		sprintf(device_name, "HCSR_%d", index);	

		HCSR_dev->name = device_name;
		HCSR_dev->device_no = index;

		pl_temp2 = platform_device_alloc(device_name, -1);
		if(pl_temp2) 
		{
			//err = platform_device_add_resources(pl_temp2, &resources,ARRAY_SIZE(resources));
			//if (err == 0)
				//err = platform_device_add_data(pl_temp2, &platform_data,sizeof(platform_data));
			//if (err == 0)
				err = platform_device_add(pl_temp2);
		} 
		else 
		{
			err = -ENOMEM;
		}
		if (err)
			platform_device_put(pl_temp2);

	HCSR_dev->pl_device = pl_temp2;
	list_add(&(HCSR_dev->list), &(HCSR_devicelist.list));

	pr_info("Platform Device %d Registered\n", index);
	}
	return 0;
}

static void pdevice_exit(void)
{
	struct Device_HCSR *HCSR_dev;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &HCSR_devicelist.list)
	{
		HCSR_dev = list_entry(pos, struct Device_HCSR, list);
		platform_device_del(HCSR_dev->pl_device);
		pr_info("Device Unregistered: %s\n", HCSR_dev->name);
	}
}

module_init(pdevice_init);
module_exit(pdevice_exit);


MODULE_DESCRIPTION("platform_device.c");
MODULE_AUTHOR("Sushant Trivedi");
MODULE_LICENSE("GPL");