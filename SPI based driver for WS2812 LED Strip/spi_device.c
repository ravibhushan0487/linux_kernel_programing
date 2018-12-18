/* 
Assignment 3 Part 1 Creating SPI Device
Sushant Trivedi (1213366971) and Ravi Bhushan (1214347783)
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>

#define MY_BUS_NUM 1
static struct spi_device *spi;

//SLAVE DEVICE: DATA STRUCTURE
static struct spi_board_info spi_device_info = {
        .modalias = "WS2812",
        .max_speed_hz = 6666666, //speed your device (slave) can handle
        .bus_num = MY_BUS_NUM,
        .chip_select = 1,
        .mode = SPI_MODE_0,
};

static int __init spi_init(void)
{
    int ret;
    struct spi_master *master;
     
    //GET SPI PORT/PINS FROM BUS NO
    master = spi_busnum_to_master( spi_device_info.bus_num );
    if( !master )
    {
        printk("SPI_DEVICE: Failed to find Master\n");
        return -ENODEV;
    }
     
    //CREATE A NEW SPI DEVICE
    spi = spi_new_device( master, &spi_device_info );
    if(!spi)
    {
        printk("SPI_DEVICE: Failed to create New Device\n");
        return -ENODEV;
    }
     
    pr_info("SPI_DEVICE: Device Created %s\n", spi->modalias);
    
    spi->bits_per_word = 16;
    ret = spi_setup(spi);
    if( ret )
    {
        printk("SPI_DEVICE: SPI Setup Failed\n");
        spi_unregister_device(spi);
        return -ENODEV;
    }
 
    pr_info("SPI_DEVICE: INIT COMPLETED\n");
    return 0;
}
 
 
static void __exit spi_exit(void)
{
    if(spi) //Global spi_device variable
    {
        spi_unregister_device(spi);
    }
    pr_info("SPI: EXIT COMPLETED\n");
}
 
module_init(spi_init);
module_exit(spi_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sushant Trivedi & Ravi Bhushan");
MODULE_DESCRIPTION("SPI Device module");