
Sensor Setup
------------
Sensor 1: Trigger Pin: IO8
Sensor 1: Echo Pin: IO9

Sensor 2: Trigger Pin: IO7
Sensor 2: Echo Pin: IO6

COMMANDS TO EXECUTED THE DRIVERs
--------------------------------
HCSR_DRIVER
1. Copy all the files to same location and run the makefile in the respective folders
2. Run the make file to compile the driver file (HCSR_Driver.c)
3. Copy stuff to Galileo via Ethernet
scp HCSR_Driver.ko root@192.168.1.5:/home/
scp hcsrtester root@192.168.1.5:/home/	

4. In galileo board, execute the following command:

cd /home/
insmod HCSR_Driver.ko devices=3
./hcsrtester

You will get the logs of measurement for two sensors.

