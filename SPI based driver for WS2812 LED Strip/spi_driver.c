/* 
Assignment 3 Part 1 Creating SPI Driver
Sushant Trivedi (1213366971) and Ravi Bhushan (1214347783)
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/completion.h>

#define DEVICE_NAME "WS2812"
#define CLASS_NAME	"WSRing"

//IOCTL Commands
#define RESET _IOWR(530,1, int)

static struct class *class;
static struct cdev cdev;
static dev_t spi_device_no;
static struct spi_device  *spi;
static unsigned char leds_pixel_data[16][24];    //pixels
static unsigned char single_led_data[24];  
static spinlock_t spi_lock;

//SPI DEVICE ID LIST
struct spi_device_id led_device_id[] = {{"WS2812",0},{}};

//Indicated that SPI Async transfer is complete
struct completion message_transferred;

//DATA STRUCT - LED DATA
struct led_data
{
	int led_number;
	int red_intensity;
	int green_intensity;
	int blue_intensity;
};

//SPI DRIVER - INIT AND EXIT FUNCTIONS
static int __init spidriver_init(void);   
static void __exit spidriver_exit(void);

//SPI FILE OPERATION FUNCTIONS
static int ws_open(struct inode *, struct file *);
static int ws_release(struct inode *, struct file *);
static long ws_ioctl(struct file *, unsigned int ,unsigned long);
static ssize_t ws_write(struct file *f, const char *t, size_t i, loff_t *lo);

//SPI DRIVER - PROBE AND REMOVE FUNCTIONS
static int spi_probe(struct spi_device *);
static int spi_remove(struct spi_device *);

//DATA STRUCT - SPI FILE OPERATIONS
static const struct file_operations spi_operations = 
{
	.owner = THIS_MODULE,
	.open = ws_open,
	.release = ws_release,
	.write	= ws_write,
	.unlocked_ioctl = ws_ioctl,
};

//SPI DRIVER DATASTRUCTURE
static struct spi_driver ws_driver = 
{
    .driver = 
    {
    .name = "WS2812",
    .owner = THIS_MODULE,
    },
	.id_table = led_device_id,
    .probe = spi_probe,
    .remove = spi_remove,
};

//PIN CONFIG
//pin ->linux pin number, direction ->output is 1 : high, 2: low, val ->value 1 : high,0 : low
int pin_config(int pin, int direction, bool val)
{ 
    int ret;
    ret = gpio_request(pin, "sysfs");
    if(ret)
    { pr_info("GPIO Request Failed %d\n", pin); return -1;}
    gpio_export(pin, false);
  	if(pin<64)
  	{
  		gpio_direction_output(pin, direction);
    }
    if(gpio_cansleep(pin))
    {
        gpio_set_value_cansleep(pin, val);
    }
    return 0;
}

//RESET GPIOS
void reset_gpios(void)
{
	gpio_free(44);
	gpio_unexport(44);
	gpio_free(24);
	gpio_unexport(24);
	gpio_free(72);
	gpio_unexport(72);
	pin_config(24,0,0);
	pin_config(44,1,1);
	pin_config(72,0,0);
}

//SPI DRIVER FUNCTIONS
//SPI PROBE FUNCTION
static int spi_probe(struct spi_device *spi_dev)
{
	int ret=1;
	spi = spi_dev;

	if (alloc_chrdev_region(&spi_device_no, 0, 1, DEVICE_NAME) < 0)
	{
		pr_info("SPI_DRIVER: Failed to Allocate Device\n");
		return -1;
	}
	
	if((class=class_create(THIS_MODULE,CLASS_NAME))==NULL)
	{
		pr_info("SPI_DRIVER: Class Creation Failed\n");
		return -1;
	}
	
	cdev_init(&cdev, &spi_operations);
	cdev.owner = THIS_MODULE;
	ret = cdev_add(&cdev, (spi_device_no), 1);
	if (ret) 
	{ 
		printk("SPI_DRIVER: CDEV Creation Failed\n");
		return ret; 
	}	

	//DEVICE CREATION
	if(device_create(class, NULL, MKDEV(MAJOR(spi_device_no), 0), NULL, DEVICE_NAME) == NULL)
    {
		pr_info("SPI_DRIVER: Device Creation Failed\n");
		return -1;
	}

	pr_info("SPI_DRIVER: Probe Function Called\n");
	return 0;
}

//SPI REMOVE FUNCTION
static int spi_remove(struct spi_device *spi)
{
	device_destroy(class, MKDEV(MAJOR(spi_device_no), 0));
	pr_info("SPI_DRIVER: Remove Function Called\n");
	return 0;
}

uint8_t set_bit_1(void)
{
	return 0xF0;    //data = 1
}

uint8_t set_bit_0(void)
{
	return 0xC0;    //data = 0
}

//Set color of single led
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

//Reset leds
void reset_leds(void)
{
	int row_index,column_index;
	set_color(0,0,0);
	for (row_index=0;row_index<16;row_index++)
	{
		for (column_index=0;column_index<24;column_index++)
			leds_pixel_data[row_index][column_index]=single_led_data[column_index];
	}
	spi_write(spi,leds_pixel_data,sizeof(leds_pixel_data));
}


//OPEN FUNCTION 
int ws_open(struct inode *inode, struct file *file)
{
	pr_info("WS_OPEN: Device opened\n");
	return 0;
}

static void spi_msg_transfer_complete(void *arg)
{
	complete(arg);
}

//Async SPI Transfer
static int spi_transfer_message(struct spi_device *spi, struct spi_message *message, int bus_locked)
{
	DECLARE_COMPLETION_ONSTACK(message_transferred);
	int status;
	struct spi_master *master = spi->master;

	message->complete = spi_msg_transfer_complete;
	message->context = &message_transferred;

	if (!bus_locked)
		mutex_lock(&master->bus_lock_mutex);

	status = spi_async_locked(spi, message);

	if (!bus_locked)
		mutex_unlock(&master->bus_lock_mutex);

	if (status == 0) {
		wait_for_completion(&message_transferred);
		status = message->status;
	}
	message->context = NULL;
	return status;
}

//Create SPI transfer message
static inline int spi_write_async_message(struct spi_device *spi, const void *buf, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= buf,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_transfer_message(spi, &m, 0);
}

//WRITE FUNCTION
ssize_t ws_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	unsigned long flags;
	struct led_data *led_info;
	spin_lock_irqsave(&spi_lock, flags );
	led_info = (struct led_data *) kmalloc(sizeof(struct led_data),GFP_KERNEL);
	copy_from_user(led_info,buf,sizeof(struct led_data));
	set_led_rgb(led_info->led_number,led_info->red_intensity,led_info->green_intensity,led_info->blue_intensity);
	spi_write_async_message(spi,leds_pixel_data,sizeof(leds_pixel_data));
	pr_info("WS_WRITE: Write Completed\n");
	spin_unlock_irqrestore(&spi_lock, flags);
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

//INIT FUNCTION
int __init spidriver_init(void)
{
	int ret;
	
	//REGISTER SPI DRIVER
	ret = spi_register_driver(&ws_driver);
	if(ret < 0)
	{
		pr_info("SPI_DRIVER: Register Failed\n");
		return -1;
	}

	//SETUP GPIO FOR THE SPI COMMUNICATIONS
	//IO11 - SPI MOSI - OUTPUT
	pin_config(24,0,0);
	pin_config(44,1,1);
	pin_config(72,0,0);

	spin_lock_init(&spi_lock);
	pr_info("SPI_DRIVER: INIT SUCCESS\n");

	return 0;
}


//EXIT FUNCTION
void __exit spidriver_exit(void)
{
	int pin_no[] = {24,44,72};
	int i;

	pr_info("SPI_DRIVER: EXIT FUNCTION CALLED\n");
	reset_leds();	
	for(i=0; i<3; i++)
	{
		pr_info("GPIO: Freeing %d\n", pin_no[i]);
		gpio_free(pin_no[i]);
		gpio_unexport(pin_no[i]);
	}
	spi_unregister_driver(&ws_driver);
	cdev_del(&cdev);
    device_destroy(class, MKDEV(MAJOR(spi_device_no), 0));
    device_destroy(class, MKDEV(MAJOR(spi_device_no), 1));
    class_destroy(class);
    unregister_chrdev_region((spi_device_no), 2);
    pr_info("SPI_DRIVER: EXIT SUCCESS\n");
}

module_init(spidriver_init);
module_exit(spidriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sushant Trivedi & Ravi Bhushan");
MODULE_DESCRIPTION("SPI Driver module");
