1. Copy all the files to same location and run the makefile in the respective folders
2. Run the make file to compile the driver file (Hash_Driver.c)
3. Copy stuff to Galileo via Ethernet
scp hashtester root@192.168.1.10:/home/
scp Hash_Driver.ko root@192.168.1.10:/home/

4. In galileo board, execute the following command:

cd /home/
insmod Hash_Driver.ko
./hashtester

