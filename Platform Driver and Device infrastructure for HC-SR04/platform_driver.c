//#include <stdlib.h>
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
#include <linux/platform_device.h>

//TODO: Delete the below headers in final code.
#include <linux/random.h>
#include <linux/delay.h>


//PINMUXING FUNCTIONALITY

// GPIO pin multiplexing table.
// Each column represents corresponding IO0 to IO13 shield pins.
// Each row represents specific function select GPIO pins.
// If value is "-1", no gpio setting required for that pin.
//=====================================================================================
//                   IO : 0  1  2  3  4  5  6  7  8  9 10 11 12 13       Functions :
int gpio_ctrl[4][14] = {{32,28,34,16,36,18,20,-1,-1,22,26,24,42,30},  // Level shifter
                        {-1,45,77,76,-1,66,68,-1,-1,70,74,44,-1,46},  // MUX select 1
                        {-1,-1,-1,64,-1,-1,-1,-1,-1,-1,-1,72,-1,-1},  // MUX select 2
                        {11,12,13,14, 6, 0, 1,38,40, 4,10, 5,15, 7}}; // Linux gpio pins



//PLATFORM DRIVER
#define HCSR_NAME0 "HCSR_0"
#define HCSR_NAME1 "HCSR_1"
#define HCSR_NAME2 "HCSR_2"
#define HCSR_NAME3 "HCSR_3"
#define HCSR_NAME4 "HCSR_4"
#define HCSR_NAME5 "HCSR_5"
#define HCSR_NAME6 "HCSR_6"
#define HCSR_NAME7 "HCSR_7"
#define HCSR_NAME8 "HCSR_8"
#define HCSR_NAME9 "HCSR_9"
#define DRIVER_NAME "HCSR_DRIVER"
#define CLASS_NAME "HCSR"

static const struct platform_device_id P_id_table[] = {
	{ HCSR_NAME0, 0 },
    { HCSR_NAME1, 0 },
    { HCSR_NAME2, 0 },
    { HCSR_NAME3, 0 },
    { HCSR_NAME4, 0 },
    { HCSR_NAME5, 0 },
    { HCSR_NAME6, 0 },
    { HCSR_NAME7, 0 },
    { HCSR_NAME8, 0 },
    { HCSR_NAME9, 0 },
};
static struct class *cl;
static int first=1;
static dev_t H_add;
static int device_found;

////////////////////////////////////////////////////////////////////////////
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

#define SUCCESS 0
#define NO_PIN -1
#define NO_VALUE -1

//Pins that cannot be configured for both rising and falling edge R/F/B
#define NON_ECHO_PINS 6
static int non_echo_pins[NON_ECHO_PINS] = {0,1,7,8,10,12};

//Commands
#define CONFIG_PINS _IOWR(530,1, struct ioctl_buffer)
#define SET_PARAMETERS _IOWR(530,2, struct ioctl_buffer)

/* Circular FIFO Buffer */
#define FIFO_ELEMENTS 5 //Total elements in the fifo buffer
#define FIFO_SIZE (FIFO_ELEMENTS) //Helps to distinguish between buffer full and buffer empty
#define BUFFER_EMPTY -1

//IOCTL Struct
struct ioctl_buffer {
	int param_1; //Trigger Pin or Number of samples per measureent
	int param_2; //Echo Pin or Sampling Period in microseconds
};


struct fifo_buffer {
	int distance; //Save distance measured by hcsr sensor
	uint64_t tsc; //Save the timestamp at which distance was measured
};

//IRQ Data
struct hcsr04_irq_data {
	int irq_no; 									//IRQ number returned by gpio_to_irq()
	int irq_echo_pin; 								//The linux GPIO where this irq is active
	int irq_trigger_pin; 							//The linux GPIO which will trigger the irq
	int irq_edge; 									//0: Rising Edge, 1: Falling Edge. Used in Interrupt Handler
	uint64_t rising_edge_time; 						//Time when rising edge interrupt occurs
	uint64_t falling_edge_time;						//Time when falling edge interrupt occurs
	int total_samples; 								//Total Samples Collected till now
	int outlier_lowest; 							//Lowest distance measured
	int outlier_highest; 							//Highest distance measured
	int sample_sum; 								//Sum of distances measured till now. Average = (sample_sum - outlier_lowest - outlier_highest)/(total_samples-2)
};

//Distance Sensor device structure
struct hcsr04_device {
	struct miscdevice misc;
	struct list_head list;
	struct hcsr04_irq_data *irq_data; 				//Stores irq related data
	struct task_struct *tsk; 						//Kthread task struct
	int trigger_pin; 								//Digital IO pin of Galileo acting as Trigger
	int echo_pin; 									//Digital IO pin of Galileo acting as Echo
	int m; 											//Total samples to be collected
	int delta; 										//Time period of sample collection
	int measurement_in_progress; 					//1 if there is an ongoing write operation being carried out for the device.0 otherwise
	struct fifo_buffer *buffer[FIFO_SIZE]; 			//buffer to store measurement samples
	int bufferIndex; 								//Variable to keep track of the buffer position where read/write operation is to be performed
	struct semaphore read_lock; 					//For signalling buffer read operations
	spinlock_t buffer_lock; 						//For syncing buffer write and read operations
	int are_pins_configured; 						//1 if pins have not been condigured. 0 otherwise
	int are_m_delta_configured; 					//1 of Sampling Period and number of samples to be collected are configured
	dev_t Dev_add;

} hcsr04_device_list;

//Pin Configuration
//This struct holds the pin configuration of each digital IO pin present in Galileo board
struct gen2_pin_config {
	int galileo_pin; 				//IO0 to IO13
	int linux_gpio_pin;
	int level_shifter_pin;
	int pinmux_1;
	int pinmux_1_level; 			//1 for High and 0 for Low
	int pinmux_2;
	int pinmux_2_level; 			//1 for High and 0 for Low
	int is_pin_configured; 			//1 if gpio_request has already been called. 0 otherwise or when gpio_free has been called
	int device_minor_number; 		//Minor Number of the device who configured the pin
	struct list_head list;
} gen2_pin_list;

//Initialize device variables
//-1 indicates that there is no pin configured
void initialize_device(struct hcsr04_device *device)  
{
	int bufferIndex;
	device->bufferIndex = 0;
	device->trigger_pin = NO_PIN;
	device->echo_pin = NO_PIN;
	device->m = NO_VALUE;
	device->delta = NO_VALUE;
	device->measurement_in_progress = 0;
	device->are_pins_configured = 0;
	device->are_m_delta_configured = 0;
	sema_init(&device->read_lock, 0);
	spin_lock_init(&device->buffer_lock);
	device->tsk = (struct task_struct *)kmalloc(sizeof(struct task_struct), GFP_KERNEL);
	device->irq_data = (struct hcsr04_irq_data *)kmalloc(sizeof(struct hcsr04_irq_data), GFP_KERNEL);
	device->irq_data->irq_no = NO_VALUE;
	device->irq_data->irq_echo_pin = NO_VALUE;
	device->irq_data->irq_trigger_pin = NO_VALUE;
	device->irq_data->irq_edge = 0;
	device->irq_data->rising_edge_time = 0;
	device->irq_data->falling_edge_time = 0;
	device->irq_data->total_samples = 0;
	device->irq_data->outlier_lowest = 0;
	device->irq_data->outlier_highest = 0;
	device->irq_data->sample_sum = 0;
	bufferIndex = 0;
	while(bufferIndex < FIFO_SIZE)
	{
		device->buffer[bufferIndex] = (struct fifo_buffer *)kmalloc(sizeof(struct fifo_buffer), GFP_KERNEL);
		device->buffer[bufferIndex]->distance = NO_VALUE;
		device->buffer[bufferIndex]->tsc = NO_VALUE;
		bufferIndex++;
	}
}

//Prints all the data stored in struct hcsr04_device of a device
void print_device_data(struct hcsr04_device *device)  
{
	int index;
	printk(KERN_ALERT "\n\nDevice:%s Details\n",device->misc.name);
	printk(KERN_ALERT "---------------------\n");
	printk(KERN_ALERT "Minor No:%d\n",device->misc.minor);
	if(device->are_pins_configured)
	{
		printk(KERN_ALERT "Trigger Pin:%d and Echo Pin:%d\n",device->trigger_pin,device->echo_pin);
	}
	else
	{
		printk(KERN_ALERT "No Trigger or Echo Pins were configured\n");
	}
	if(device->irq_data->irq_no != NO_VALUE)
	{
		printk(KERN_ALERT "Interrupt configured on GPIO%d with IRQ Number:%d\n",device->echo_pin,device->irq_data->irq_no);
	}
	if(device->are_pins_configured)
	{
		printk(KERN_ALERT "%d samples collected with sampling period %d,for each measurement\n",device->m,device->delta);
	}
	else
	{
		printk(KERN_ALERT "No Total Samples or Sampling period configured\n");
	}
	if(device->are_pins_configured && device->are_m_delta_configured)
	{
		printk(KERN_ALERT "Buffer Data\n");
		for(index=0;index<FIFO_SIZE;index++)
		{
			if(device->buffer[index]->distance != NO_VALUE && device->buffer[index]->tsc != NO_VALUE )
			{
				printk(KERN_ALERT "Distance:%d and time:%llu\n",device->buffer[index]->distance,device->buffer[index]->tsc);
			}
		}
	}
	else
	{
		printk(KERN_ALERT "Buffer is empty\n");
	}
}

//Store data related to a single pin
//NO_PIN indicates that there is no pin to be configured
void populate_pin_data(int galileo_pin,int linux_gpio_pin, int level_shifter_pin, 
	int pinmux_1,int pinmux_1_level, int pinmux_2, int pinmux_2_level)
{
	struct gen2_pin_config *gen2_pin;
	gen2_pin = (struct gen2_pin_config *)kmalloc(sizeof(struct gen2_pin_config), GFP_KERNEL);
	gen2_pin->galileo_pin = galileo_pin;
	gen2_pin->linux_gpio_pin = linux_gpio_pin;
	gen2_pin->level_shifter_pin = level_shifter_pin;
	gen2_pin->pinmux_1 = pinmux_1;
	gen2_pin->pinmux_1_level = pinmux_1_level;
	gen2_pin->pinmux_2 = pinmux_2;
	gen2_pin->pinmux_2_level = pinmux_2_level;
	gen2_pin->is_pin_configured = 0;

	list_add(&(gen2_pin->list), &(gen2_pin_list.list));

}

//Stores Galileo Gen2 Digital Pins configuration data
//NO_PIN indicates that there is no pin to be configured
//Source Gen2_pins.xlsx
int populate_gen2_pins_data(void) 
{
	//IO0
	populate_pin_data(0,11,32,NO_PIN,NO_VALUE,NO_PIN,NO_VALUE);
	

	//IO1
	populate_pin_data(1,12,28,45,0,NO_PIN,NO_VALUE);

	//IO2
	populate_pin_data(2,61,NO_PIN,77,0,NO_PIN,NO_VALUE);

	//IO3
	populate_pin_data(3,62,NO_PIN,76,0,64,0);

	//IO4
	populate_pin_data(4,6,36,NO_PIN,NO_VALUE,NO_PIN,NO_VALUE);
	
	//IO5
	populate_pin_data(5,0,18,66,0,NO_PIN,NO_VALUE);

	//IO6
	populate_pin_data(6,1,20,68,0,NO_PIN,NO_VALUE);

	//IO7
	populate_pin_data(7,38,NO_PIN,NO_PIN,NO_VALUE,NO_PIN,NO_VALUE);

	//IO8
	populate_pin_data(8,40,NO_PIN,NO_PIN,NO_VALUE,NO_PIN,NO_VALUE);

	//IO9
	populate_pin_data(9,4,22,70,0,NO_PIN,NO_VALUE);

	//IO10
	populate_pin_data(10,10,26,74,0,NO_PIN,NO_VALUE);

	//IO11
	populate_pin_data(11,5,24,44,0,72,0);

	//IO12
	populate_pin_data(12,15,42,NO_PIN,NO_VALUE,NO_PIN,NO_VALUE);

	//IO13
	populate_pin_data(13,7,30,46,0,NO_PIN,NO_VALUE);

	return 1;
}

//FIFO functions
// Reads from the buffer and return BUFFER_EMPTY as distance if buffer empty
struct fifo_buffer* fifoRead(struct hcsr04_device *device) 
{
   struct fifo_buffer *buffer;
   buffer = (struct fifo_buffer *)kmalloc(sizeof(struct fifo_buffer), GFP_KERNEL);
   if (device->buffer[device->bufferIndex]->tsc == NO_VALUE) {
	   	//Buffer is empty
	   	printk(KERN_ALERT "FIFO buffer of device %s is empty\n",device->misc.name);
	   	buffer->distance = BUFFER_EMPTY;
	   	buffer->tsc = 0;
	   	return buffer;
   } 
   else
   {
	   	buffer->distance = device->buffer[device->bufferIndex]->distance;
	   	buffer->tsc = device->buffer[device->bufferIndex]->tsc; 
	   	//Deleting the values of current location as we have read it
	   	device->buffer[device->bufferIndex]->distance = NO_VALUE;
   		device->buffer[device->bufferIndex]->tsc = NO_VALUE;  	
   }
   device->bufferIndex = device->bufferIndex - 1;
   if(device->bufferIndex < 0) 
   {
   		device->bufferIndex = FIFO_SIZE - 1;
   }
   return buffer;
}
 
// Writes to the buffer and overwrites if buffer is full
int fifoWrite(struct hcsr04_device *device, struct fifo_buffer *buffer) 
{

   	device->bufferIndex = (device->bufferIndex + 1) % FIFO_SIZE;
   	printk(KERN_ALERT "Writing at position %d of %s with distance:%d and time:%llu\n", device->bufferIndex,device->misc.name,buffer->distance,buffer->tsc);
   	device->buffer[device->bufferIndex]->distance = buffer->distance;
   	device->buffer[device->bufferIndex]->tsc = buffer->tsc;
   	return SUCCESS;
}

//Release Single pin
//pin: Linux GPIO pin number
void release_pin(int pin)
{
	gpio_free(pin);
    gpio_unexport(pin);
    printk(KERN_ALERT "Freed pin %d\n",pin);
}

//Free Gpio pins
void release_gpio_pins(struct hcsr04_device *device)
{
	struct gen2_pin_config *gen2_pin;
	struct list_head *gpio_pos;
	
	list_for_each(gpio_pos, &gen2_pin_list.list)
	{
		gen2_pin = list_entry(gpio_pos, struct gen2_pin_config, list);
		
		//Device is null when this function is called in exit module
		//Else, release only those pins which have been set by the device whose release function has been called
		if(gen2_pin->is_pin_configured && (device == NULL || device->misc.minor == gen2_pin->device_minor_number))
		{
			printk(KERN_ALERT "Freeing Pins related to IO%d\n",gen2_pin->galileo_pin);
			//Free Linux GPIO pin
			release_pin(gen2_pin->linux_gpio_pin);
            
            //Free Level Shifter Pin
            if(gen2_pin->level_shifter_pin != NO_PIN) 
            {
            	release_pin(gen2_pin->level_shifter_pin);
            }

             //Free PINMUX 1 Pin
            if(gen2_pin->pinmux_1 != NO_PIN) 
            {
            	release_pin(gen2_pin->pinmux_1);
            }

             //Free PINMUX 2 Pin
            if(gen2_pin->pinmux_2 != NO_PIN) 
            {
            	release_pin(gen2_pin->pinmux_2);
            }
            gen2_pin->is_pin_configured = 0;
            gen2_pin->device_minor_number = NO_VALUE;
            if(device == NULL)
            {
            	kfree(gen2_pin);
            }
		} 
	}
	    
}

/*Single PIN CONFIG
**pin: GPIO Linux pin number
**io_flag: 0 -> Input, 1 -> Output
**Val: value of the pin. 1 -> High, 0 -> Low
**configure: 1: Configure pin using gpio_request 0: Update pin
**show_log: 1: shows pin configured log 0: no logs are shown*/
int pin_config(int pin, int io_flag, bool val, int configure, int show_log)
{ 
    int ret;
    char *name;

    //below variables are used for display purpose only
   	char *operation, *direction, *cansleep;
    
    name = (char *)kmalloc(sizeof(char)*10, GFP_KERNEL);
	sprintf(name, "gpio%d", pin);
    
    if(configure)
    {
        ret = gpio_request(pin, name);
        if(ret)
        { 
        	printk(KERN_ALERT "GPIO Request Failed %d\n", pin); 
        	return 1;
        }
        gpio_export(pin, false);
        operation = "Configured";
    }   
    else
    {
    	operation = "Updated";
    }
    if (io_flag)
    {
    	direction="";
    	if(pin<64)
    	{
    		gpio_direction_output(pin, val);
	    	if(val) 
	    	{
	    		direction = "as output high";
	    	}
	    	else
	    	{
	    		direction = "as output low";
	    	}
	    }
    }
    else
    {
    	//SUSHANT
    	direction="";
    	if(pin<64)
    	{
    		gpio_direction_input(pin); 
     		direction = "as input";  
    	}
     	
    }
    if(gpio_cansleep(pin))
    {
    	gpio_set_value_cansleep(pin, 0);
    	cansleep = "can sleep";
    }
    else
    {
    	gpio_set_value(pin, 0);
    	cansleep = "cannot sleep";
    }
    if(show_log) 
    {
    	printk(KERN_ALERT "%s pin %s %s which %s\n",operation,name,direction,cansleep);
    }
    return 0;
}



//PIN CONFIG -> Sushant
//io_flag: 0 -> Input, 1 -> Output, 2 -> Set_value_cansleep, 4-> None
int pin_config2(int pin, int io_flag, bool val, int first)
{ 
    int ret;
    if(first == 1)
    {
        ret = gpio_request(pin, "sysfs");
        if(ret)
        { pr_info("GPIO Request Failed %d\n", pin); return -1;}
        gpio_export(pin, false);
    }   
    if (io_flag == 0)
    {
        gpio_direction_input(pin);
    }
    else if(io_flag == 1)
    {
        gpio_direction_output(pin, val);
    }
    else if(io_flag == 2)
    {
        gpio_set_value_cansleep(pin, val);
    }
    return 0;
}

//Interrupt Handler
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct hcsr04_device *device;
	uint64_t echo_pulse_width;
	int distance;
	device = (struct hcsr04_device *)dev_id;
    
    //Need to distinguish between HCSR Echo Pulse Rising and Falling Edge Interrupts
    if(device->irq_data->irq_edge == 0)
    {
        //Rising Edge Interrupt
        //printk("Interrupt %d occured on rising edge\n",irq);
        device->irq_data->rising_edge_time = rdtsc();
        device->irq_data->irq_edge = 1;//Toggle it as next interrupt will be falling edge
    }
    else if(device->irq_data->irq_edge == 1)
    {
        //Falling Edge Interrupt
        //printk("Interrupt %d occured on falling edge\n",irq);
		device->irq_data->falling_edge_time = rdtsc();
        device->irq_data->irq_edge = 0;
        echo_pulse_width = (device->irq_data->rising_edge_time - device->irq_data->falling_edge_time);
        distance = do_div(echo_pulse_width, 23200);
        if(distance > device->irq_data->outlier_highest)
        {
        	device->irq_data->outlier_highest = distance;
        }
        else if(distance < device->irq_data->outlier_lowest)
        {
        	device->irq_data->outlier_lowest = distance;	
        }
        printk(KERN_ALERT "Distance Measured for %s:%d\n",device->misc.name,distance);
        device->irq_data->total_samples = device->irq_data->total_samples + 1;
        device->irq_data->sample_sum = device->irq_data->sample_sum + distance;
    }
    
    
    return IRQ_HANDLED;
}


//Configure Interrupts on Echo Pin 
int configure_interrupt(struct hcsr04_device *device, int echo_pin_linux) 
{
	int irq, error;
	char *irq_name;
	irq = gpio_to_irq(echo_pin_linux);
    if(irq < 0)
    {
        printk(KERN_ALERT "GPIO%d cannot be used as echo for %s as interrupt cannot be configured\n",device->echo_pin,device->misc.name);
        return 0;
    }
    irq_name = (char *)kmalloc(sizeof(char)*15, GFP_KERNEL);
	sprintf(irq_name, "%s_%d", device->misc.name,irq);
    error = request_irq(irq, irq_handler, IRQ_TYPE_EDGE_BOTH, irq_name, (void*)device);
    if(error)
    {
        printk(KERN_ALERT "Unable to claim irq %d; error %d\n ", irq, error);
        return 0;
    }
    device->irq_data->irq_no = irq;
    device->irq_data->irq_echo_pin = echo_pin_linux;
    printk(KERN_ALERT "Interrupt %d configured for device %s on GPIO%d\n",irq,device->misc.name,device->echo_pin);
	return 1;
}

//Configure pins as trigger or echo
//Trigger pin has been set to low here. Need to set it to high during call to interrupts
int configure_pins(struct hcsr04_device *device) 
{
	struct gen2_pin_config *gen2_pin;
	struct list_head *gpio_pos;
	int pin_confg_ret, io_flag;
	
	list_for_each(gpio_pos, &gen2_pin_list.list)
	{
		gen2_pin = list_entry(gpio_pos, struct gen2_pin_config, list);
		if(device->trigger_pin == gen2_pin->galileo_pin || device->echo_pin == gen2_pin->galileo_pin)
		{
			if(device->echo_pin == gen2_pin->galileo_pin)
			{
				io_flag = 0;
			}
			else
			{
				io_flag = 1;
			}
			//Check if Pin has already been configured. If yes then do not call gpio_request and gpio_export again
			if(gen2_pin->is_pin_configured)
			{
				//Check if Pin has been configued by the same device. If no then return error
				if(device->misc.minor != gen2_pin->device_minor_number) 
				{
					printk(KERN_ALERT "Reconfiguring Pin %d for device %s\n",device->trigger_pin,device->misc.name);
				}
				//Pin has been configured by the same device. Update Pin's function to be a trigger pin
				printk(KERN_ALERT "Updating Pins related to IO%d\n",gen2_pin->galileo_pin);
				
				//Configure Linux Pin
				pin_confg_ret = pin_config(gen2_pin->linux_gpio_pin,io_flag,0,0,1);
				if(pin_confg_ret)
		        { 
		        	printk(KERN_ALERT "GPIO Request Failed for %d\n", gen2_pin->linux_gpio_pin); 
		        	return 0;
		        }

		        //Configure Level Shifter Pin
				if(gen2_pin->level_shifter_pin != NO_PIN) 
				{
					pin_confg_ret = pin_config(gen2_pin->level_shifter_pin,io_flag,0,0,1);
					if(pin_confg_ret)
			        { 
			        	printk(KERN_ALERT "GPIO Request Failed %d\n", gen2_pin->level_shifter_pin);
			        	//Free Linux Pin configured before
			        	gpio_free(gen2_pin->linux_gpio_pin); 
			        	return 0;
			        }
				}
			}
			else
			{
				//Pin has not been configured. Configure Pin and make it a trigger pin
				printk(KERN_ALERT "Configuring Pins related to IO%d\n",gen2_pin->galileo_pin);
				
				//Configure Linux Pin
				pin_confg_ret = pin_config(gen2_pin->linux_gpio_pin,io_flag,0,1,1);
				if(pin_confg_ret)
		        { 
		        	printk(KERN_ALERT "GPIO Request Failed %d\n", gen2_pin->linux_gpio_pin); 
		        	return 0;
		        }

		        //Configure Level Shifter Pin
				if(gen2_pin->level_shifter_pin != NO_PIN) 
				{
					pin_confg_ret = pin_config(gen2_pin->level_shifter_pin,io_flag,0,1,1);
					if(pin_confg_ret)
			        { 
			        	printk(KERN_ALERT "GPIO Request Failed %d\n", gen2_pin->level_shifter_pin); 
			        	//Free Linux Pin configured before
			        	gpio_free(gen2_pin->linux_gpio_pin); 
			        	return 0;
			        }
				}

				//Configure PINMUX_1 Pin
				if(gen2_pin->pinmux_1 != NO_PIN) 
				{
					pin_confg_ret = pin_config(gen2_pin->pinmux_1,1,gen2_pin->pinmux_1_level,1,1);
					if(pin_confg_ret)
			        { 
			        	printk(KERN_ALERT "GPIO Request Failed %d\n", gen2_pin->pinmux_1); 
			        	
			        	//Free Linux Pin configured before
			        	gpio_free(gen2_pin->linux_gpio_pin); 
			        	//Free Level Shifter Pin COnfigured before
			        	if(gen2_pin->level_shifter_pin != NO_PIN) 
			        	{
			        		gpio_free(gen2_pin->level_shifter_pin); 
			        	}
			        	return 0;
			        }
				}

				//Configure PINMUX_2 Pin
				if(gen2_pin->pinmux_2 != NO_PIN) 
				{
					pin_confg_ret = pin_config(gen2_pin->pinmux_2,1,gen2_pin->pinmux_2_level,1,1);
					if(pin_confg_ret)
			        { 
			        	printk(KERN_ALERT "GPIO Request Failed %d\n", gen2_pin->pinmux_2); 

			        	//Free Linux Pin configured before
			        	gpio_free(gen2_pin->linux_gpio_pin); 
			        	//Free Level Shifter Pin COnfigured before
			        	if(gen2_pin->level_shifter_pin != NO_PIN) 
			        	{
			        		gpio_free(gen2_pin->level_shifter_pin); 
			        	}
			        	//Free PINMUX_1 pin configured before
			        	if(gen2_pin->pinmux_1 != NO_PIN) 
						{
							gpio_free(gen2_pin->pinmux_1); 	
						}
			        	return 0;
			        }
				}
				gen2_pin->is_pin_configured = 1;
            	gen2_pin->device_minor_number = device->misc.minor;
			}
			//Configure Interrupt for echo pin
			if(!io_flag) 
			{
				if(!configure_interrupt(device,gen2_pin->linux_gpio_pin))
				{
					return 0;
				}
			}else
			{
				device->irq_data->irq_trigger_pin = gen2_pin->linux_gpio_pin;
			}

		}
	}
	return 1;
}

//Configure Pins on galileo board based on user input
int configure_galileo_pins(struct hcsr04_device *device)
{
	if(configure_pins(device))
	{
		return 1;
	} 
	else
	{
		return 0;
	}
	
}

/*Compare pin input by user with pins stored in non_echo_pins array
 *non_echo_pin array contains pins which cannot be used as echo pin 
 *as they cannot trigger interrupt on both edges at the same time*/
int is_edge_interruptible(int echo_pin)
{
	int is_edge_interruptible = 1;
	int index;
	for (index=0; index < NON_ECHO_PINS; index++) 
	{
        if (non_echo_pins[index] == echo_pin)
         {
         	is_edge_interruptible = 0;
         }   
    }
	return is_edge_interruptible;
}


//Measurement Related Functions
//Triggers Interrupt
static int hcsr04_thread_write(void *data)
{
	uint64_t tsc;
	struct fifo_buffer *buffer;
	struct hcsr04_device *device;
	int samples_collected, total_samples;
	unsigned long flags;                       //SUSHANT_May_need_to_be_global

	device = (struct hcsr04_device *)data;


	spin_lock_irqsave(&device->buffer_lock, flags);

	buffer = (struct fifo_buffer *)kmalloc(sizeof(struct fifo_buffer), GFP_KERNEL);
	samples_collected = 0;
	total_samples = device->m;
	printk(KERN_ALERT "\nSample Collection for %s write operation\n",device->misc.name);
	while(samples_collected <= (total_samples + 2)) 
	{
		/*if(kthread_should_stop()) {
			do_exit(0);
		}*/
		//Set trigger pin to 0
		//pin_config(device->irq_data->irq_trigger_pin, 1, 0, 0, 0);
	    //Set Trigger pin to 1
	    pin_config(device->irq_data->irq_trigger_pin, 1, 1, 0,0); 
		mdelay(20);    
		pin_config(device->irq_data->irq_trigger_pin, 1, 0, 0,0);
		samples_collected++;
		device->irq_data->total_samples = samples_collected;
		mdelay(device->delta);
	}
	
	//wait_for_completion (&device->hcsr_measurement_complete); /* Wait till measurement is complete */
	tsc = rdtsc();
	buffer->tsc = tsc;
	buffer->distance = device->irq_data->sample_sum/device->irq_data->total_samples;
	fifoWrite(device,buffer);
	device->measurement_in_progress = 0;

	spin_unlock_irqrestore(&device->buffer_lock, flags);
	up(&device->read_lock);
	do_exit(0);
}

//Creates a kernel thread to initiate measurement
int initiate_measurement(struct hcsr04_device *device)
{
	unsigned long flags;
	spin_lock_irqsave(&device->buffer_lock, flags);

	printk(KERN_ALERT "Initiating new measurement for device %s\n",device->misc.name);
	//TODO:call a kernel thread to do this and implement hrtimer there
	device->tsk = kthread_run(&hcsr04_thread_write, (void *)device, "%s_write", device->misc.name);  
	if (IS_ERR(device->tsk)) 
	{
		printk(KERN_ALERT "Unable to perform write operation for %s device. Unable to create kernel thread.\n",device->misc.name);
		return 1;
	}

	spin_unlock_irqrestore(&device->buffer_lock, flags);
	return 0;
}

//Device file operation functions
static int hcsr04_open(struct inode *inode, struct file *file)
{
	int device_minor = iminor(inode);
	int device_found;
	struct hcsr04_device *device;
	struct list_head *pos;
	device_found = 0;
	list_for_each(pos, &hcsr04_device_list.list)
	{
		device = list_entry(pos, struct hcsr04_device, list);
		if(device->misc.minor == device_minor)
		{
			device_found = 1;
			printk(KERN_ALERT "device match found\n");
	    	printk(KERN_ALERT "Opened %s with minor no:%d\n",device->misc.name,device->misc.minor);
			initialize_device(device);
			file->private_data = device;
			break;
		}
	}
	if(!device_found) 
	{
		printk(KERN_ALERT "No device found\n");
	}
    return SUCCESS;
}

static int hcsr04_write(struct file *file, const char __user *buf, 
			size_t len, loff_t *ppos)
{
	int buffer_operation;
    struct hcsr04_device *device = file->private_data;

    if(!device->are_pins_configured || !device->are_m_delta_configured)
    {
    	printk(KERN_ALERT "Please configure the device %s before writing to it\n",device->misc.name);
    	return EINVAL;
    }
    copy_from_user(&buffer_operation, (int*)buf, sizeof(int));
    if(buffer_operation != 0)
    {
    	printk(KERN_ALERT "Clearing FIFO buffer of %s\n",device->misc.name);
    	device->bufferIndex = 0;
    }
    if(device->measurement_in_progress)
    {
    	printk(KERN_ALERT "Measurement in progress from %s\n",device->misc.name);
    	return EINVAL;
    } 
    else
    {
		//Initiating new measurement
		device->measurement_in_progress = 1;
		if(!initiate_measurement(device))
		{
			return EINVAL;
		}
		
    }
    return SUCCESS;
}

static int hcsr04_read(struct file *file, char * buf, size_t count, loff_t *ppos)
{
	struct hcsr04_device *device;
	struct fifo_buffer *buffer;
	unsigned long flags;

	device = file->private_data;

	spin_lock_irqsave(&device->buffer_lock, flags);

	buffer = fifoRead(device);

	spin_unlock_irqrestore(&device->buffer_lock, flags);

	if(buffer->distance == BUFFER_EMPTY)
	{
		printk(KERN_ALERT "FIFO buffer of device %s is empty\n",device->misc.name);
		if(!device->are_pins_configured || !device->are_m_delta_configured)
	    {
	    	printk(KERN_ALERT "Please configure the device %s before reading from it\n",device->misc.name);
	    	return EINVAL;
	    }
	    //TODO: Not working
		if(device->measurement_in_progress)
	    {
	    	printk(KERN_ALERT "Measurement in progress\n");
	    } 
	    else
	    {
	    	//Initiating new measurement
			if(!initiate_measurement(device))
			{
				return EINVAL;
			}
	    }
	    	/* Wait till measurement is complete */
	    	spin_lock_irqsave(&device->buffer_lock, flags);

	    	copy_to_user((struct fifo_buffer *)buf, fifoRead(device), sizeof(struct fifo_buffer));

			spin_unlock_irqrestore(&device->buffer_lock, flags);
	} 
	
	copy_to_user((struct fifo_buffer *)buf, buffer, sizeof(struct fifo_buffer));	
	
	return SUCCESS;
}

//Reset data related to a device
static int hcsr04_release(struct inode *inode, struct file *file)
{
	int thread_return;
	struct hcsr04_device *device = file->private_data;
	print_device_data(device);
	printk(KERN_ALERT "Erasing data of %s and reinitializing it\n",device->misc.name);
	if(device->measurement_in_progress)
	{
		thread_return = kthread_stop(device->tsk);
	    if(!thread_return) 
	    {
	    	printk(KERN_ALERT "There was a problem while stopping write thread of %s\n",device->misc.name);
	    }
	}
	if(device->irq_data->irq_no != NO_VALUE)
	{
		printk(KERN_ALERT "Freeing IRQ %d of %s on IO%d\n",device->irq_data->irq_no,device->misc.name,device->echo_pin);
		free_irq(device->irq_data->irq_no, (void*)device);
	}
	release_gpio_pins(device);
	initialize_device(device);
	
	return SUCCESS;
}

static long hcsr04_ioctl(struct file * file, unsigned int ioctlnum, unsigned long arg)
{   
	struct hcsr04_device *device = file->private_data;
	struct ioctl_buffer *buffer;
	int edge_interruptible; //Checks if the echo pin input by the user is edge interrupt enabled
	switch(ioctlnum) 
    {
        case CONFIG_PINS:
        	buffer = (struct ioctl_buffer*) arg;
        	edge_interruptible = is_edge_interruptible(buffer->param_2);
        	if(buffer->param_1 < 0 || buffer->param_1 > 12 || buffer->param_2 < 0 || buffer->param_2 > 12 || !edge_interruptible)
        	{
        		printk(KERN_ALERT "Wrong Pin numbers\n");

        		return EINVAL;
        	}
        	//Release the previous GPIOs held by device
        	if(device->echo_pin != NO_VALUE && device->irq_data->irq_no != NO_VALUE)
        	{
        		//Free IRQ
				printk(KERN_ALERT "Freeing IRQ %d of %s on IO%d\n",device->irq_data->irq_no,device->misc.name,device->echo_pin);
        		free_irq(device->irq_data->irq_no, (void*)device);
        		device->echo_pin = NO_VALUE;
        		device->irq_data->irq_no = NO_VALUE;
        	}
        	//release_gpio_pins(device);
        	device->trigger_pin = buffer->param_1;
        	device->echo_pin = buffer->param_2;
        	//Configure device with new pins
        	if(!configure_galileo_pins(device))
        	{
        		printk(KERN_ALERT "Unable to Configure pin %d as Trigger and pin %d as Edge pins in Galileo board\n",
        			device->trigger_pin,device->echo_pin);
        		return EINVAL;
        	}
        	device->are_pins_configured = 1;
        	break;
        case SET_PARAMETERS:

        	buffer = (struct ioctl_buffer*) arg;
			if(buffer->param_1 <= 0 || buffer->param_2 <= 0)
        	{
        		return EINVAL;
        	}
        	device->m = buffer->param_1;
        	device->delta = buffer->param_2;
        	device->are_m_delta_configured = 1;
        	break;
        default:
        	return EINVAL;
    }
	return SUCCESS;
}

static const struct file_operations hcsr04_fops = {
    .owner			= THIS_MODULE,
    .write			= hcsr04_write,
    .read 			= hcsr04_read,
    .open			= hcsr04_open,
    .release		= hcsr04_release,
    .unlocked_ioctl = hcsr04_ioctl
};


//SYSFS FUNCTIONS
//SYSFS - TRIGGER
static ssize_t trigger_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr04_device *device = dev_get_drvdata(dev);
    return snprintf(buf, PAGE_SIZE, "%d\n", device->trigger_pin);
}


static ssize_t trigger_pin_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	// pr_info("Trigger Pin Store %s\n", buf);
 //    return count;
	int io_flag, pin_no,i;
	struct hcsr04_device *device = dev_get_drvdata(dev);
	sscanf(buf, "%d", &device->trigger_pin);
	pr_info("TRIGGER PIN STOORE CALLED\n");

	//OUTPUT - TRIGGER
	for(i=0; i<4; i++)
	{
	    io_flag = 1;
	    pin_no = gpio_ctrl[i][device->trigger_pin];
	    if(pin_no != -1)
	    {
	        if(pin_no >= 64) {io_flag=4;}
	        //pr_info("Output Configuring: %d, %d, %d, %d\n",pin_no, io_flag, 0, 1);
	        pin_config2(pin_no, io_flag, 0, 1);
	    }
	}
	return count;
}


//SYSFS - ECHO
static ssize_t echo_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr04_device *device = dev_get_drvdata(dev);
    return snprintf(buf, PAGE_SIZE, "%d\n", device->echo_pin);
}


static ssize_t echo_pin_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int io_flag, pin_no,i;
	struct hcsr04_device *device = dev_get_drvdata(dev);
	sscanf(buf, "%d", &device->echo_pin);


	//INPUT - ECHO_PIN
	for(i=0; i<4; i++)
	{
	    io_flag = 0;
	    pin_no = gpio_ctrl[i][device->echo_pin];
	    if(pin_no != -1)
	    {
	        if(pin_no >= 64) {io_flag=4;}
	        //pr_info("Output Configuring: %d, %d, %d, %d\n",pin_no, io_flag, 0, 1);
	        pin_config2(pin_no, io_flag, 0, 1);
	    }
	}
	return count;
}

//SYSFS - NO_OF_Samples
static ssize_t no_of_samples_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr04_device *device = dev_get_drvdata(dev);
    return snprintf(buf, PAGE_SIZE, "%d\n", device->m);
}

static ssize_t no_of_samples_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	struct hcsr04_device *device = dev_get_drvdata(dev);
	sscanf(buf, "%d", &device->m);
	return count;
}

//SYSFS - DELTA
static ssize_t delta_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr04_device *device = dev_get_drvdata(dev);
    return snprintf(buf, PAGE_SIZE, "%d\n", device->delta);
}

static ssize_t delta_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	struct hcsr04_device *device = dev_get_drvdata(dev);
	sscanf(buf, "%d", &device->delta);
	return count;
}


//SYSFS - DISTANCE
static ssize_t distance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr04_device *device = dev_get_drvdata(dev);

	struct fifo_buffer* buffer;
	buffer = fifoRead(device);
	return snprintf(buf, PAGE_SIZE, "%d\n", buffer->distance);	 
}

static ssize_t distance_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int d;
	sscanf(buf, "%d", &d);
	return count;
}


//SYSFS - ENABLE
static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr04_device *device = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", device->measurement_in_progress);
}



static ssize_t enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int enable=0;
	struct hcsr04_device *device = dev_get_drvdata(dev);
	
	sscanf(buf, "%d", &enable);

	if(enable == 1)
	{
		if(device->measurement_in_progress)
	    {
	    	printk(KERN_ALERT "Measurement in progress from %s\n",device->misc.name);
	    	return EINVAL;
	    } 
	    else
	    {
			//Initiating new measurement
			device->measurement_in_progress = 1;
			if(!initiate_measurement(device))
			{
				return EINVAL;
			}
			
	    }
	}
	return count;
}




static DEVICE_ATTR(trigger_pin,S_IRWXU, trigger_pin_show, trigger_pin_store);
static DEVICE_ATTR(echo_pin,S_IRWXU, echo_pin_show, echo_pin_store);
static DEVICE_ATTR(no_of_samples, S_IRWXU, no_of_samples_show, no_of_samples_store);
static DEVICE_ATTR(delta, S_IRWXU, delta_show, delta_store);
static DEVICE_ATTR(distance, S_IRWXU, distance_show, distance_store);
static DEVICE_ATTR(enable, S_IRWXU, enable_show, enable_store);

//HCSR04_INIT FUNCTION
static int hcsr04_init(const char *device_name)
{
	int ret;
	int error;
	struct hcsr04_device *device;
	struct device *H_dev;
	//device_name = (char *)kmalloc(sizeof(char)*10, GFP_KERNEL);
	
	//MISC DEVICE DATASTRUCTURE
	device = (struct hcsr04_device *)kmalloc(sizeof(struct hcsr04_device), GFP_KERNEL);
	device->misc.minor = MISC_DYNAMIC_MINOR;
	device->misc.name = device_name;
	device->misc.fops = &hcsr04_fops;
	
	//INITIALIZE DEVICE
	initialize_device(device);

	//DEVICE CREATION
	device->Dev_add = H_add;
	H_dev = device_create(cl, NULL, MKDEV(MAJOR(H_add), MINOR(H_add)), device, (device_name));
	if(!H_dev)
	{
		pr_info("DEVICE CREATE: Device Creation Failed\n");
		device_destroy(cl, H_add);
	}
	H_add++;



	//DEVICE ATTRIBUTES ADDITION
	ret = device_create_file(H_dev, &dev_attr_trigger_pin);
	if (ret<0) 
	{
	 	printk("DEVICE ATTRIBUTE CREATE FAILED: %s\n", dev_attr_trigger_pin.attr.name);
	}

	ret = device_create_file(H_dev, &dev_attr_echo_pin);
	if (ret<0) 
	{
	 	printk("DEVICE ATTRIBUTE CREATE FAILED: %s\n", dev_attr_echo_pin.attr.name);
	}

	ret = device_create_file(H_dev, &dev_attr_no_of_samples);
	if (ret<0) 
	{
		printk("DEVICE ATTRIBUTE CREATE FAILED: %s\n", dev_attr_no_of_samples.attr.name);
	}

	ret = device_create_file(H_dev, &dev_attr_delta);
	if (ret<0) 
	{
		printk("DEVICE ATTRIBUTE CREATE FAILED: %s\n", dev_attr_delta.attr.name);
	}


	ret = device_create_file(H_dev, &dev_attr_enable);
	if (ret<0) 
	{
		printk("DEVICE ATTRIBUTE CREATE FAILED: %s\n", dev_attr_enable.attr.name);
	}


	ret = device_create_file(H_dev, &dev_attr_distance);
	if (ret<0) 
	{
		printk("DEVICE ATTRIBUTE CREATE FAILED: %s\n", dev_attr_distance.attr.name);
	}






	//MISC DEVICE CREATED
	error = misc_register(&(device->misc));
	if (error) 
	{
		printk(KERN_ALERT "Unable to register %s device\n",device_name);
	    return error;
	} 
	else
	{
	  	pr_info("DEVICE CREATED: %s Minor No: %d\n", device_name, device->misc.minor);
	}
	list_add(&(device->list), &(hcsr04_device_list.list));
    return SUCCESS;
}

/////////////PROBE FUNCTIONS////////////////////
static int Pdriver_probe(struct platform_device *dev)
{
	pr_info("Found Devices with Name: %s %d\n", dev->name, device_found);
	device_found++;


	//SYSFS CLASS CREATE
	if(first==1)
	{
		cl = class_create(THIS_MODULE, CLASS_NAME);
		if(IS_ERR(cl))
		{
			pr_info("CLASS CREATE: Can't Create Class\n");
			class_unregister(cl);
			class_destroy(cl);
			return 1;
		}
		first = 0;
		//Initialize Global List
		INIT_LIST_HEAD(&hcsr04_device_list.list);
		INIT_LIST_HEAD(&gen2_pin_list.list);
		populate_gen2_pins_data();
	}

	//HCSR04_INIT FUNCTION CALLED
	if(hcsr04_init(dev->name))
	{
		pr_info("HCSR04_INIT Function Failed\n");
	}
	return 0;
}


void hcsr04_exit(const char *device_name)
{
	int error, bufferIndex;
	struct hcsr04_device *device;
	struct list_head *pos, *q;
	list_for_each_safe(pos, q, &hcsr04_device_list.list)
	{
		device = list_entry(pos, struct hcsr04_device, list);
		//pr_info("%s\n", device->misc.name);
//ERROR
		if(device->misc.name == device_name)
		{
			pr_info("DEVICE DELETED: %s Minor No: %d\n", device->misc.name, device->misc.minor);	
			//DELETE DEVICES
			device_destroy(cl, device->Dev_add);

			//MISC DEVICE DEREGISTERED
	 		error = misc_deregister(&device->misc);
		    if (error) 
		    {
		        printk(KERN_ALERT "Unable to deregister HCSR device\n");
		    }

			list_del(pos);
			bufferIndex = 0;
			while(bufferIndex < FIFO_SIZE)
			{
				kfree(device->buffer[bufferIndex]);
				bufferIndex++;
			}
			kfree(device->irq_data);
			kfree(device->tsk);
			kfree(device);
		}
	}	
	release_gpio_pins(NULL);
    
}





static int Pdriver_remove(struct platform_device *dev)
{
	hcsr04_exit(dev->name);
	device_found--;
	if (device_found==0)
	{
		class_unregister(cl);
		class_destroy(cl);
		pr_info("CLASS DESTROYED\n");
	}	
	pr_info("PROBE: Remove Function Completed %s \n", dev->name);
	return 0;

}


////////////////////////////////////////////////


static struct platform_driver Pdriver_id = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= Pdriver_probe,
	.remove		= Pdriver_remove,
	.id_table	= P_id_table,
};

module_platform_driver(Pdriver_id);


MODULE_DESCRIPTION("platform_driver.c");
MODULE_AUTHOR("Sushant Trivedi");
MODULE_LICENSE("GPL");