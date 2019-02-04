1. Copy all the files to same location and run the makefile in the respective folders
2. Run the make file to compile the driver file (Mprobe.c)
3. Copy stuff to Galileo via Ethernet
scp kprobetester root@192.168.1.10:/home/
scp Hash_Driver.ko root@192.168.1.10:/home/
scp Mprobe.ko root@192.168.1.10:/home/

4. In galileo board, execute the following command:
cd /home/
insmod Hash_Driver.ko
insmod Mprobe.ko
./kprobetester 1

To Check Output
to check kernel output: dmesg|grep “Mprobe”|grep “HT530”
