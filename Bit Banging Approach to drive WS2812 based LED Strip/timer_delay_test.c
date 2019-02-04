/* 
Test program to measure overheads and understand feasibility of bit banging
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

#elif defined(__powerpc__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int result=0;
  unsigned long int upper, lower,tmp;
  __asm__ volatile(
          "0:                  \n"
          "\tmftbu   %0           \n"
          "\tmftb    %1           \n"
          "\tmftbu   %2           \n"
          "\tcmpw    %2,%0        \n"
          "\tbne     0b         \n"
          : "=r"(upper),"=r"(lower),"=r"(tmp)
          );
  result = upper;
  result = result<<32;
  result = result|lower;

  return(result);
}

#else

#error "No tick counter is available!"

#endif


/* Test Configurations*/
//cat /proc/cpuinfo
#define TICKS_PER_NS 399.076
#define BOARD_IO_PIN 12 //Linux Pin Number of IO1
#define LEVEL_SHIFTER 28
#define PINMUX_1 45
#define LEVEL_SHIFTER_VALUE 0
#define PINMUX_1_VALUE 0
#define SAMPLE_SIZE 1000
#define TOTAL_LED_BITS 384 //16(LEDS)*24(BITS)
#define HIGH_VOLTAGE_TIME_1 700 //ns
#define LOW_VOLTAGE_TIME_1 600 //ns
#define HIGH_VOLTAGE_TIME_0 350 //ns
#define LOW_VOLTAGE_TIME_0 800 //ns

#define PORTA_DATA        0x00  /* Data */
#define GIP_GPIO_BAR    1


unsigned offset=4;
static struct hrtimer hr_timer;
int hr_sample_counter;
int hrtimer_index, send_bit, bit_cycle;

ktime_t ktime_start , ktime_stop;
ktime_t hrtime_expires;
uint64_t rdtsc_start, rdtsc_stop;
uint64_t rdtsc_interval, ktime_interval;
uint64_t rdtsc_avg,ktime_avg;

u32 val_data = 0;
static void __iomem *reg_base;
static void __iomem *reg_data;

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

void ndelay_overhead_rdtsc(void)
{
  int index;
  rdtsc_avg = 0;
  for(index=0;index<SAMPLE_SIZE;index++)
  {
    rdtsc_start = rdtsc();
    ndelay(350);
    rdtsc_stop = rdtsc();
    rdtsc_interval = rdtsc_stop - rdtsc_start;
    rdtsc_interval*=1000;
    do_div(rdtsc_interval,TICKS_PER_NS);
    rdtsc_avg+=rdtsc_interval;
  }
  do_div(rdtsc_avg,SAMPLE_SIZE);
  pr_info("For ndelay of 350ns rdtsc() recorded average delay of %lluns with average overhead of %lluns\n",rdtsc_avg,rdtsc_avg-350);
}

void ndelay_overhead_ktime(void)
{
  int index;
  ktime_avg = 0;
  for(index=0;index<SAMPLE_SIZE;index++)
  {
    ktime_start = ktime_get();
    ndelay(350);
    ktime_stop = ktime_get();
    ktime_interval = ktime_to_ns(ktime_sub(ktime_stop,ktime_start));
    ktime_avg+=ktime_interval;
  }
  do_div(ktime_avg,SAMPLE_SIZE);
  pr_info("For ndelay of 350ns ktime_get() recorded average delay of %lluns with average overhead of %lluns\n",ktime_avg,ktime_avg-350);
}

//hrtimer callback function after timer expires
enum hrtimer_restart hr_timer_expired_rdtsc(struct hrtimer *timer)  
{
  if(hr_sample_counter < SAMPLE_SIZE)
  {
    rdtsc_stop = rdtsc();
    rdtsc_interval = rdtsc_stop - rdtsc_start;
    rdtsc_interval*=1000;
    do_div(rdtsc_interval,TICKS_PER_NS);
    rdtsc_avg+=rdtsc_interval;
    hr_sample_counter+=1;
    hrtimer_add_expires(timer, hrtime_expires);
    rdtsc_start = rdtsc();
    return HRTIMER_RESTART;
  } else
  {
    do_div(rdtsc_avg,SAMPLE_SIZE);
    pr_info("For Hrtimer of 350ns rdtsc() recorded average delay of %lluns with average overhead of %lluns\n",rdtsc_avg,rdtsc_avg-350);
  }
  return 0;
}

void hrtimer_overhead_rdtsc(void)
{
  rdtsc_avg = 0;
  hr_sample_counter = 0;
  hrtime_expires = ktime_set(0,350);
  hrtimer_init(&hr_timer,CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hr_timer.function = hr_timer_expired_rdtsc;
  rdtsc_start = rdtsc();
  hrtimer_start( &hr_timer, hrtime_expires, HRTIMER_MODE_REL);
  ssleep(1);
  hrtimer_cancel(&hr_timer);
}

//hrtimer callback function after timer expires
enum hrtimer_restart hr_timer_expired_ktime(struct hrtimer *timer)  
{
  if(hr_sample_counter < SAMPLE_SIZE)
  {
    ktime_stop = ktime_get();
    ktime_interval = ktime_to_ns(ktime_sub(ktime_stop,ktime_start));
    ktime_avg+=ktime_interval;
    hr_sample_counter+=1;
    hrtimer_add_expires(timer,hrtime_expires);
    ktime_start = ktime_get();
    return HRTIMER_RESTART;
  } else
  {
    do_div(ktime_avg,SAMPLE_SIZE);
    pr_info("For Hrtimer of 350ns ktime_get() recorded average delay of %lluns with average overhead of %lluns\n",ktime_avg,ktime_avg-350);
  }
  return 0;
}

void hrtimer_overhead_ktime(void)
{
  ktime_avg = 0;
  hr_sample_counter = 0;
  hrtime_expires = ktime_set(0,350);
  hrtimer_init(&hr_timer,CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hr_timer.function = hr_timer_expired_ktime;
  ktime_start = ktime_get();
  hrtimer_start( &hr_timer, hrtime_expires, HRTIMER_MODE_REL);
  ssleep(1);
  hrtimer_cancel(&hr_timer);
}

//Resets IO1 to 0
void set_gpio_low(void)
{
  gpio_set_value(BOARD_IO_PIN,0);
}

//Sets IO1 to 1
void set_gpio_high(void)
{
  gpio_set_value(BOARD_IO_PIN,1);
}

void gpio_set_value_overhead_rdtsc(void)
{
  int index;
  rdtsc_avg = 0;
  for(index=0;index<SAMPLE_SIZE;index++)
  {
    set_gpio_low();
    rdtsc_start = rdtsc();
    set_gpio_high();
    rdtsc_stop = rdtsc();
    rdtsc_interval = rdtsc_stop - rdtsc_start;
    rdtsc_interval*=1000;
    do_div(rdtsc_interval,TICKS_PER_NS);
    rdtsc_avg+=rdtsc_interval;
    ndelay(500);
  }
  do_div(rdtsc_avg,SAMPLE_SIZE);
  pr_info("Average time to set gpio using gpio_set_value() (rdtsc) : %lluns \n",rdtsc_avg);
}

void gpio_set_value_overhead_ktime(void)
{
  int index;
  ktime_avg = 0;
  for(index=0;index<SAMPLE_SIZE;index++)
  {
    set_gpio_low();
    ktime_start = ktime_get();
    set_gpio_high();
    ktime_stop = ktime_get();
    ktime_interval = ktime_to_ns(ktime_sub(ktime_stop,ktime_start));
    ktime_avg+=ktime_interval;
    ndelay(500);
  }
  do_div(ktime_avg,SAMPLE_SIZE);
  pr_info("Average time to set gpio using gpio_set_value() (ktime) : %lluns \n",ktime_avg);
}

void send_bit_0_ndelay(void)
{
  set_gpio_high();
  ndelay(HIGH_VOLTAGE_TIME_0);
  set_gpio_low();
  ndelay(LOW_VOLTAGE_TIME_0);
}

void send_bit_1_ndelay(void)
{
  set_gpio_high();
  ndelay(HIGH_VOLTAGE_TIME_1);
  set_gpio_low();
  ndelay(LOW_VOLTAGE_TIME_1);
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


void iowrite32_overhead_rdtsc(void)
{
  int index;
  rdtsc_avg = 0;
  for(index=0;index<SAMPLE_SIZE;index++)
  {
    rdtsc_start = rdtsc();
    iowrite32_1();
    rdtsc_stop = rdtsc();
    rdtsc_interval = rdtsc_stop - rdtsc_start;
    rdtsc_interval*=1000;
    do_div(rdtsc_interval,TICKS_PER_NS);
    rdtsc_avg+=rdtsc_interval;
  }
  do_div(rdtsc_avg,SAMPLE_SIZE);
  pr_info("Average time to set gpio using iowrite32() (rdtsc) : %lluns \n",rdtsc_avg);
}

void iowrite32_overhead_ktime(void)
{
  int index;
  ktime_avg = 0;
  for(index=0;index<SAMPLE_SIZE;index++)
  {
    ktime_start = ktime_get();
    iowrite32_1();
    ktime_stop = ktime_get();
    ktime_interval = ktime_to_ns(ktime_sub(ktime_stop,ktime_start));
    ktime_avg+=ktime_interval;
  }
  do_div(ktime_avg,SAMPLE_SIZE);
  pr_info("Average time to set gpio using iowrite32() (ktime) : %lluns \n",ktime_avg);
}

void bit_bang_ndelay_gpio(void)
{
  int led_index,bit_index;
  pr_info("ndelay gpio_set_value: Trying to turn on all leds\n");
  for(led_index=0;led_index<16;led_index++)
  {
    for(bit_index=0;bit_index<24;bit_index++)
    {
      send_bit_1_ndelay();
    }
  }

  ssleep(2);
  pr_info("ndelay gpio_set_value: Trying to turn off all leds\n");
  for(led_index=0;led_index<16;led_index++)
  {
    for(bit_index=0;bit_index<24;bit_index++)
    {
      send_bit_0_ndelay();
    }
  }
}

//hrtimer callback function after timer expires
enum hrtimer_restart hr_timer_expired(struct hrtimer *timer)  
{
  send_bit = 1;
  if(hrtimer_index < TOTAL_LED_BITS)
  {
    if(bit_cycle == 1)
    {
      set_gpio_high();
      hrtime_expires = ktime_set(0,HIGH_VOLTAGE_TIME_1);
      hrtimer_add_expires(timer,hrtime_expires);
      bit_cycle = 0;
      return HRTIMER_RESTART;
    } else if(bit_cycle == 0)
    {
      set_gpio_low();
      hrtime_expires = ktime_set(0,LOW_VOLTAGE_TIME_1);
      hrtimer_add_expires(timer,hrtime_expires);
      bit_cycle = 1;
      hrtimer_index+=1;
      return HRTIMER_RESTART;
    }
  } 
  send_bit = 0;
  if(hrtimer_index == TOTAL_LED_BITS && bit_cycle == 1)
  {
      pr_info("hrtimer gpio_set_value: Trying to turn off all leds\n");
  }
  if(hrtimer_index < 2*TOTAL_LED_BITS)
  {
    if(bit_cycle == 1)
    {
      set_gpio_high();
      hrtime_expires = ktime_set(0,HIGH_VOLTAGE_TIME_0);
      hrtimer_add_expires(timer,hrtime_expires);
      bit_cycle = 0;
      return HRTIMER_RESTART;
    } else if(bit_cycle == 0)
    {
      set_gpio_low();
      hrtime_expires = ktime_set(0,LOW_VOLTAGE_TIME_0);
      hrtimer_add_expires(timer,hrtime_expires);
      bit_cycle = 1;
      hrtimer_index+=1;
      return HRTIMER_RESTART;
    }
  }

  return 0;
}

//Trying Bit Banging using hrtimer and gpio_set_value()
void bit_bang_hrtimer_gpio(void)
{
  pr_info("hrtimer gpio_set_value: Trying to turn on all leds\n");
  hrtimer_index = 0;
  bit_cycle = 1;
  hrtime_expires = ktime_set(0,350);
  hrtimer_init(&hr_timer,CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hr_timer.function = hr_timer_expired;
  hrtimer_start( &hr_timer, hrtime_expires, HRTIMER_MODE_REL);
  ssleep(3);
  hrtimer_cancel(&hr_timer);
}

void send_bit_1_ndelay_iowrite32(void)
{   
  iowrite32_1();
  ndelay(700);
  iowrite32_0();
  ndelay(600);
}

void send_bit_0_ndelay_iowrite32(void)
{
  iowrite32_1();
  iowrite32_0();
  ndelay(500);
}

//Resetting GPIOs tyo 0
void reset_gpios(void)
{
  int led_index,bit_index;
  pr_info("Resetting GPIOs using iowrite32()\n");
  for(led_index=0;led_index<16;led_index++)
  {
    for(bit_index=0;bit_index<24;bit_index++)
    {
      send_bit_0_ndelay_iowrite32();
    }
  }
}

//Setting leds to red and then resetting them using ndelay and iowrite bit banging 
void bit_bang_ndelay_iowrite32(void)
{
  int led_index,bit_index;
  int i,l;
  pr_info("ndelay iowrite32: Trying to turn leds red\n");
  for (l=0;l<16;l++)
  {
    for (i=0;i<8;i++)
      send_bit_0_ndelay_iowrite32();
    for (i=0;i<8;i++)
      send_bit_1_ndelay_iowrite32();
    for (i=0;i<8;i++)
      send_bit_0_ndelay_iowrite32();
  }
  ssleep(2);
  pr_info("ndelay iowrite32: Trying to turn off all leds\n");
  ssleep(2);
  for(led_index=0;led_index<16;led_index++)
  {
    for(bit_index=0;bit_index<24;bit_index++)
    {
      send_bit_0_ndelay_iowrite32();
    }
  }
}

//hrtimer callback function after timer expires
enum hrtimer_restart hr_timer_expired_iowrite32(struct hrtimer *timer)  
{
  send_bit = 1;
  if(hrtimer_index < TOTAL_LED_BITS)
  {
    if(bit_cycle == 1)
    {
      iowrite32_1();
      hrtime_expires = ktime_set(0,HIGH_VOLTAGE_TIME_1);
      bit_cycle = 0;
    } else if(bit_cycle == 0)
    {
      iowrite32_0();
      hrtime_expires = ktime_set(0,LOW_VOLTAGE_TIME_1);
      bit_cycle = 1;
      hrtimer_index+=1;
    }
    hrtimer_add_expires(timer,hrtime_expires);
    return HRTIMER_RESTART;
  } 
  send_bit = 0;
  if(hrtimer_index == TOTAL_LED_BITS && bit_cycle == 1)
  {
      pr_info("hrtimer iowrite32: Trying to turn off all leds\n");
  }
  if(hrtimer_index < 2*TOTAL_LED_BITS)
  {
    if(bit_cycle == 1)
    {
      iowrite32_1();
      hrtime_expires = ktime_set(0,10);
      bit_cycle = 0;
    } else if(bit_cycle == 0)
    {
      iowrite32_0();
      hrtime_expires = ktime_set(0,500);
      bit_cycle = 1;
      hrtimer_index+=1;
    }
    hrtimer_add_expires(timer,hrtime_expires);
    return HRTIMER_RESTART;
  }

  return 0;
}

void bit_bang_hrtimer_iowrite32(void)
{
  pr_info("hrtimer iowrite32: Trying to turn on all leds\n");
  hrtimer_index = 0;
  bit_cycle = 1;
  hrtime_expires = ktime_set(0,350);
  hrtimer_init(&hr_timer,CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hr_timer.function = hr_timer_expired_iowrite32;
  hrtimer_start( &hr_timer, hrtime_expires, HRTIMER_MODE_REL);
  ssleep(3);
  hrtimer_cancel(&hr_timer);
}

//Conducting tests
static int __init delay_test_init(void)
{
	int pin_confg_ret;
  struct pci_dev *pdev;
  resource_size_t start = 0, len = 0;
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
    set_gpio_low();

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

    pr_info("Test Initialized\n");

    pr_info("Overhead Measurement. Sample Size %d\n",SAMPLE_SIZE);

    //Overhead measurements of timer functions
    ndelay_overhead_rdtsc();
    ndelay_overhead_ktime();
    hrtimer_overhead_rdtsc();
    hrtimer_overhead_ktime();

    reset_gpios();
    ssleep(3);

    //Overhead Measurement of gpio_set_value()
    gpio_set_value_overhead_rdtsc();
    gpio_set_value_overhead_ktime();
    
    ssleep(3);
    reset_gpios();
    reset_gpios();
    ssleep(1);

    //Overhead Measurement of iowrite32()
    iowrite32_overhead_rdtsc();
    iowrite32_overhead_ktime();

    ssleep(3);
    reset_gpios();
    reset_gpios();
    ssleep(1);

    pr_info("Bit Banging Test\n");
    //Bit Banging using ndelay and gpio_set_value()
    bit_bang_ndelay_gpio();

    ssleep(3);
    reset_gpios();
    ssleep(1);

    //Bit Banging using hrtimer and gpio_set_value()
    bit_bang_hrtimer_gpio();

    ssleep(3);
    reset_gpios();
    ssleep(1);

    //Bit Banging using ndelay and iowrite32()
    bit_bang_ndelay_iowrite32();

    ssleep(3);
    reset_gpios();
    ssleep(1);

    //Bit Banging using ndelay and iowrite32()
    bit_bang_hrtimer_iowrite32();

    ssleep(3);
    reset_gpios();

    return 0;

}

static void __exit delay_test_exit(void)
{
	release_pin(BOARD_IO_PIN);
	release_pin(LEVEL_SHIFTER);
	release_pin(PINMUX_1);
	pr_info("Exiting Test\n");

}

module_init(delay_test_init);
module_exit(delay_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sushant Trivedi & Ravi Bhushan");
MODULE_DESCRIPTION("Timer Delay Test module");