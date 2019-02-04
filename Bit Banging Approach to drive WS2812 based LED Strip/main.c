
/* 
User level Program to for Bit Banging approach using ndelay and iowrite32.

*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

//IOCTL Commands
#define RESET _IOWR(530,1, int)

//Led Details
struct led_data
{
	int led_number;
	int red_intensity;
	int green_intensity;
	int blue_intensity;
};

int main()
{
	int fd_ws2812, ioctl_return;
	int total_leds,repeat_loop,led_color;
	int index,row_index;
	int repeat_infinity = 0;

	//Store RGB value of different colors
	int color_panel[12][3] = 
	{
		{255,0,0},{0,255,0},{0,0,255},{255,255,0}, //Red, Green, Blue, Yellow
		{255,255,255},{138,43,226},{0,255,255},{255,0,255}, //White, Violet, Cyan, Magneta
		{255,165,0},{139,69,19},{128,0,0},{255,105,180}, //Orange, Brown, Maroon, Pink
	};
	int led_rgb[12][3] = {{0}};

	struct led_data *led;
	led = (struct led_data *)malloc(sizeof(struct led_data));

	//Open device
	fd_ws2812 = open("/dev/WS2812",O_RDWR);
	if(fd_ws2812 < 0)
	{
		printf("Unable to open WS2812 device\n");
		exit(0);
	}

	//Reset the leds and gpios
	ioctl_return = ioctl(fd_ws2812, RESET, 1); 
    if (ioctl_return < 0) {
        printf ("IOCTL call RESET of WS2812 failed with return value:%d\n", ioctl_return);
    }


    /*User Interface Program
      **********************/
    do
    {
    	printf("How many LEDs do you want to light up?[1-16]");
    	scanf("%d",&total_leds);
    	if(total_leds > 0 && total_leds <=16)
    	{
    		break;
    	} else
    	{
    		printf("Enter a number between 1 and 16\n");
    	}
    }while(1);

    printf("How many times do you want the circular display to repeat?[0=infinite]");
    scanf("%d",&repeat_loop);
    

    printf("Color Panel\n");
    printf("1 - Red\t\t2 - Green\t3 - Blue\t4 - Yellow\n");
    printf("5 - White\t6 - Violet\t7 - Cyan\t8 - Magneta\n");
    printf("9 - Orange\t10 - Brown\t11 - Maroon\t12 - Pink\n");
    printf("13 - Custom Color(Need to enter RGB value)\n");

    for(row_index=0;row_index<total_leds;row_index++)
    {
    	do
	    {
	    	printf("LED %d color[1-13]",row_index+1);
	    	scanf("%d",&led_color);
	    	if(total_leds > 0 && led_color <=13)
	    	{
	    		if(led_color == 13) {
	    			printf("r[0-255]=");
	    			scanf("%d",&led_rgb[row_index][0]);
	    			printf("g[0-255]=");
	    			scanf("%d",&led_rgb[row_index][1]);
	    			printf("b[0-255]=");
	    			scanf("%d",&led_rgb[row_index][2]);
	    		} else
	    		{
	    			led_rgb[row_index][0] = color_panel[led_color-1][0];
	    			led_rgb[row_index][1] = color_panel[led_color-1][1];
	    			led_rgb[row_index][2] = color_panel[led_color-1][2];
	    		}
	    		
	    		break;
	    	} else
	    	{
	    		printf("Enter a number between 1 and 17\n");
	    	}
	    }while(1);
    }

	if(repeat_loop == 0)
	{
		repeat_loop = 1;
		repeat_infinity = 1;
	}

	//Write RGB values of leds to the device
	do
	{
		for(index=0;index<repeat_loop*16;index++)
		{
			for(row_index=index;row_index<index+total_leds;row_index++)
			{
			    led->led_number = row_index%16;
			    led->red_intensity = led_rgb[row_index - index][0];
			    led->green_intensity = led_rgb[row_index - index][1];
			    led->blue_intensity = led_rgb[row_index - index][2];
				write(fd_ws2812,led,sizeof(struct led_data));
			}
			sleep(1);
			for(row_index=index;row_index<index+total_leds;row_index++)
			{
			    led->led_number = row_index%16;
			    led->red_intensity = 0;
			    led->green_intensity = 0;
			    led->blue_intensity = 0;
				write(fd_ws2812,led,sizeof(struct led_data));
			}
		}
	}while(repeat_infinity == 1);
	
	ioctl_return = ioctl(fd_ws2812, RESET, 1); 
    if (ioctl_return < 0) {
        printf ("IOCTL call RESET of WS2812 failed with return value:%d\n", ioctl_return);
    }

	close(fd_ws2812);
	return 0;
}