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
#include <linux/fs.h>
#include <linux/slab.h>


#include <linux/platform_device.h>

#define DEVICE_NAME "HCSR_D"
#define DRIVER_NAME "HCSR_DRI"

typedef struct Device_HCSR 
{
	char* name;
	int device_no;
	struct list_head list;  			//Used to access all instances of the devices created in device module
	struct platform_device *pl_device;
} *DevH_Temp;

struct Device_HCSR HCSR_devicelist;


MODULE_DESCRIPTION("platform_device.h");
MODULE_AUTHOR("Sushant Trivedi");
MODULE_LICENSE("GPL");