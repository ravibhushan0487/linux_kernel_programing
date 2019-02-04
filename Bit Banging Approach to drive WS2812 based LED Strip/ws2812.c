/* 
Creating  Char Device to implement bit banging using ndelay and iowrite32
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <asm/div64.h>
#include <asm/delay.h>
#include <linux/timekeeping.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/pci.h>

#define DEVICE_NAME "WS2812"
#define CLASS_NAME	"WSRing"
//IOCTL Commands
#define RESET _IOWR(530,1, int)

#define BOARD_IO_PIN 12 //Linux Pin Number of IO1
#define LEVEL_SHIFTER 28
#define PINMUX_1 45
#define LEVEL_SHIFTER_VALUE 0
#define PINMUX_1_VALUE 0

#define PORTA_DATA        0x00  /* Data */
#define GIP_GPIO_BAR    1

unsigned offset=4;
u32 val_data = 0;
static spinlock_t lock;
static void __iomem *reg_base;
static void __iomem *reg_data;

static struct class *class;
static struct cdev cdev;
static dev_t device_no;

static unsigned char leds_pixel_data[16][24];    //pixels
static unsigned char single_led_data[24];  

//DATA STRUCT - LED DATA
struct led_data
{
	int led_number;
	int red_intensity;
	int green_intensity;
	int blue_intensity;
};

/*Single PIN CONFIG
**pin: GPIO Linux pin number
**io_flag: 0 -> Input, 1 -> Output
**Val: value of the pin. 1 -> High, 0 -> Low
**configure: 1: Configure pin using gpio_request 0: Update pin
**show_log: 1: shows pin configured log 0: no logs are shown*/
int pin_config(int pin, bool val, int show_log)
{ 
    int ret;
    char *name;

    //below variables are used for display purpose only
   	char *operation, *direction, *cansleep;
    
    name = (char *)kmalloc(sizeof(char)*10, GFP_KERNEL);
	sprintf(name, "gpio%d", pin);
    
    ret = gpio_request(pin, name);
    if(ret)
    { 
    	pr_info("GPIO Request Failed %d\n", pin); 
    	return 1;
    }
    gpio_export(pin, false);
    operation = "Configured";
    
    gpio_direction_output(pin, val);
    direction = "as ouput";  
    
    if(gpio_cansleep(pin))
    {
    	gpio_set_value_cansleep(pin, val);
    	cansleep = "can sleep";
    }
    else
    {
    	gpio_set_value(pin, val);
    	cansleep = "cannot sleep";
    }
    if(show_log) 
    {
    	pr_info("%s pin %s %s which %s\n",operation,name,direction,cansleep);
    }
    return 0;
}

//Release Single pin
//pin: Linux GPIO pin number
void release_pin(int pin)
{
  gpio_free(pin);
  gpio_unexport(pin);
  pr_info("Freed pin %d\n",pin);
}

//Resetting GPIOs
int reset_gpios(void)
{
	int pin_confg_ret;

	release_pin(BOARD_IO_PIN);
	release_pin(LEVEL_SHIFTER);
	release_pin(PINMUX_1);
	//Configure IO1 Output Pin
	//Configure Linux Pin
	pin_confg_ret = pin_config(BOARD_IO_PIN,0,1);
	if(pin_confg_ret)
    { 
    	pr_info("GPIO Request Failed 12\n"); 
    	return 0;
    }

    //Configure Level Shifter Pin
	pin_confg_ret = pin_config(LEVEL_SHIFTER,LEVEL_SHIFTER_VALUE,1);
	if(pin_confg_ret)
    { 
    	pr_info("GPIO Request Failed 28\n"); 
    	//Free Linux Pin configured before
    	release_pin(BOARD_IO_PIN); 
    	return 0;
    }

	//Configure PINMUX 1 Pin
	pin_confg_ret = pin_config(PINMUX_1,PINMUX_1_VALUE,1);
	if(pin_confg_ret)
    { 
    	pr_info("GPIO Request Failed 45\n");
    	//Free Linux Pin configured before
    	release_pin(BOARD_IO_PIN); 
    	//Free Level Shifter Pin COnfigured before
    	release_pin(LEVEL_SHIFTER); 
    	return 0;
    }
    return 0;
}


// write 1 (high voltage) to gpio 
void iowrite32_1(void)
{
  iowrite32(val_data | BIT(offset % 32), reg_data);
}

// write 0 (low voltage) to gpio 
void iowrite32_0(void)
{
  iowrite32(val_data & ~BIT(offset % 32), reg_data);
}

//Send 1 using iowrite32()
void send_bit_1_ndelay(void)
{   
  iowrite32_1();
  ndelay(400);
  iowrite32_0();
  ndelay(800);
}

//Send 0 using iowrite32()
void send_bit_0_ndelay(void)
{
  iowrite32_1();
  iowrite32_1();
  iowrite32_0();
  ndelay(500);
}

//Directly write to io using bit banging
void write_to_registers(void)
{
	int led_index,bit_index;
	unsigned long flags = 0;

	//Synchronize bit transfer
	spin_lock_irqsave(&lock, flags);
	for(led_index=0;led_index<16;led_index++)
  	{
    	for(bit_index=0;bit_index<24;bit_index++)
    	{
    		if(leds_pixel_data[led_index][bit_index] == 1)
      			send_bit_1_ndelay();
      		else
      			send_bit_0_ndelay();
   	 	}
  	}
  	spin_unlock_irqrestore(&lock, flags);
}

uint8_t set_bit_1(void)
{
	return 1;    //data = 1
}

uint8_t set_bit_0(void)
{
	return 0;    //data = 0
}

void set_color(int green, int red, int blue)
{
	int index;
	int value=8;
	for (index=0;index<value;index++)
	{
		if((green>>index)& 0x01) 
		{
			single_led_data[index] = set_bit_1();
		} else 
		{
			single_led_data[index] = set_bit_0();
		}
	}
	
	for (index=value;index<2*value;index++)
	{
		if((red>>(index-value))& 0x01)
		{
			single_led_data[index] = set_bit_1();
		} else
		{
			single_led_data[index] = set_bit_0();
		}
	}

	for (index=2*value;index<3*value;index++)	
	{
		if((blue>>(index-2*value))& 0x01)
		{
			single_led_data[index] = set_bit_1();
		} else
		{
			single_led_data[index] = set_bit_0();
		}
	}
}

void set_led_rgb(int number, int red, int green, int blue)
{	
	int index;
	set_color(green,red,blue);
	for(index=0;index<24;index++)
		leds_pixel_data[number][index]=single_led_data[index];
}


void reset_leds(void)
{
	int row_index,column_index;
	set_color(0,0,0);
	for (row_index=0;row_index<16;row_index++)
	{
		for (column_index=0;column_index<24;column_index++)
			leds_pixel_data[row_index][column_index]=single_led_data[column_index];
	}

	write_to_registers();
}

//OPEN FUNCTION 
int ws_open(struct inode *inode, struct file *file)
{
	pr_info("WS_OPEN: Device opened\n");
	return 0;
}

//WRITE FUNCTION
ssize_t ws_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct led_data *led_info;
	led_info = (struct led_data *) kmalloc(sizeof(struct led_data),GFP_KERNEL);
	copy_from_user(led_info,buf,sizeof(struct led_data));
	set_led_rgb(led_info->led_number,led_info->red_intensity,led_info->green_intensity,led_info->blue_intensity);
	write_to_registers();
	pr_info("WS_WRITE: Write Completed\n");
	return 0;
}

//IOCTL FUNCTION
static long ws_ioctl(struct file * file, unsigned int ioctlnum, unsigned long arg)
{
	switch(ioctlnum) 
    {
        case RESET:
        	pr_info("WS_IOCTL: RESET Command Received\n");
        	reset_leds();
        	reset_gpios();
        	break;
        default:
        	return EINVAL;
    }
	return 0;
}


//RELEASE FUNCTION
int ws_release(struct inode *inode, struct file *file)
{
	reset_leds();
	pr_info("WS_RELEASE: Driver Released Successfully\n");
	return 0;
}


//DATA STRUCT - WS2812 Driver FILE OPERATIONS
static const struct file_operations ws2812_operations = 
{
	.owner = THIS_MODULE,
	.open = ws_open,
	.release = ws_release,
	.write	= ws_write,
	.unlocked_ioctl = ws_ioctl
};

int __init ws2812_driver_init(void)
{
	int ret;
	struct pci_dev *pdev;
  	resource_size_t start = 0, len = 0;
  	spin_lock_init(&lock);

	if (alloc_chrdev_region(&device_no, 0, 1, DEVICE_NAME) < 0)
	{
		pr_info("WS2812_DRIVER: Failed to Allocate Device\n");
		return -1;
	}
	
	if((class=class_create(THIS_MODULE,CLASS_NAME))==NULL)
	{
		pr_info("WS2812_DRIVER: Class Creation Failed\n");
		return -1;
	}
	
	cdev_init(&cdev, &ws2812_operations);
	cdev.owner = THIS_MODULE;
	ret = cdev_add(&cdev, (device_no), 1);
	if (ret) 
	{ 
		printk("WS2812_DRIVER: CDEV Creation Failed\n");
		return ret; 
	}	

	//CHAR DEVICE CREATION
	if(device_create(class, NULL, MKDEV(MAJOR(device_no), 0), NULL, DEVICE_NAME) == NULL)
    {
		pr_info("WS2812_DRIVER: Device Creation Failed\n");
		return -1;
	}

	//Code to access GPIO registers directly 
    pdev = pci_get_device(0x8086,0x0934, NULL);
    start = pci_resource_start(pdev, GIP_GPIO_BAR);
    len = pci_resource_len(pdev, GIP_GPIO_BAR);
    if (!start || len == 0) {
    printk( "bar%d not set\n", GIP_GPIO_BAR);
    }
    // getting the base address
    reg_base = ioremap_nocache(start, len);
    if (NULL == reg_base) {
      printk( "I/O memory remapping failed\n");
    }

    reg_data= reg_base + PORTA_DATA;
    val_data = ioread32(reg_data);

	//Configure IO1 Output Pin
	//Configure Linux Pin
	ret = pin_config(BOARD_IO_PIN,0,1);
	if(ret)
    { 
    	pr_info("GPIO Request Failed 12\n"); 
    	return 0;
    }

    //Configure Level Shifter Pin
	ret = pin_config(LEVEL_SHIFTER,LEVEL_SHIFTER_VALUE,1);
	if(ret)
    { 
    	pr_info("GPIO Request Failed 28\n"); 
    	//Free Linux Pin configured before
    	release_pin(BOARD_IO_PIN); 
    	return 0;
    }

	//Configure PINMUX 1 Pin
	ret = pin_config(PINMUX_1,PINMUX_1_VALUE,1);
	if(ret)
    { 
    	pr_info("GPIO Request Failed 45\n");
    	//Free Linux Pin configured before
    	release_pin(BOARD_IO_PIN); 
    	//Free Level Shifter Pin COnfigured before
    	release_pin(LEVEL_SHIFTER); 
    	return 0;
    }
    reset_leds();
    return 0;

}

void __exit ws2812_driver_exit(void)
{
	cdev_del(&cdev);
    device_destroy(class, MKDEV(MAJOR(device_no), 0));
    device_destroy(class, MKDEV(MAJOR(device_no), 1));
    class_destroy(class);
    unregister_chrdev_region((device_no), 2);

    release_pin(BOARD_IO_PIN);
	release_pin(LEVEL_SHIFTER);
	release_pin(PINMUX_1);

    pr_info("WS2812_DRIVER: EXIT SUCCESS\n");
}

module_init(ws2812_driver_init);
module_exit(ws2812_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sushant Trivedi & Ravi Bhushan");
MODULE_DESCRIPTION("WS2812 Driver module");
