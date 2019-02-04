
Kernel Files Includes
1. Final.Patch
2. dynamic_dump_stack.c
3. syscalls.h
4. syscall_32.tbl
5. Kconfig.debug


User Test Program Files
1.  dumpmode_test.c
	Used to test the different dump modes execution as per requirement. User doesn’t need to edit the code here. All he needs to analyse
2. kallsyms_lookup_test.c
	Used to test the whether the text section kernel commands are whom kprobe are attached to.
3. signal_test.c
	Used to implement whether we are able to exit the kernel


Updating MakeFile
Update the makefile as per the following instructions
GALILEO_USER → Enter the user name. Default “root”
GALILEO_IP → Enter Galileo IO. Default “192.168.1.5”
IOT_HOME → Enter sysroots address


Instructions To Setup

1. STEPS FOR PATCHING AND BUILDING:

To generate a patch run the diff command with options -rNu between the update linux source directory and the original source 

 e.g. diff -rNu source_orig/ new_source/ >Final.patch

To apply the patch copy the original linux source(with the name kernel) and the Final.patch into a new directory and run the following commands. Note that this patch has been generated on a built kernel 
	
        ARCH=x86 LOCALVERSION= CROSS_COMPILE=i586-poky-linux- make -j4
        patch -p0 < Final.patch

After the patch has been applied build the kernel using the following commands to build the new kernel (from the new kernel source directory) : 

Make appropriate changes to path_to_sdk and 

1. export PATH=path_to_sdk/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$PATH

2. Cross compile the kernel using : 

ARCH=x86 LOCALVERSION= CROSS_COMPILE=i586-poky-linux- make -j4

3.  Build and extract the kernel modules from the build to a target directory (e.g ../galileo-install) using the command : 

ARCH=x86 LOCALVERSION= INSTALL_MOD_PATH=../galileo-install CROSS_COMPILE=i586-poky-linux- make modules_install

4.  Extract the kernel image (bzImage) from the build to a target directory (e.g ../galileo-install)

cp arch/x86/boot/bzImage ../galileo-install/

5.  Install the new kernel and modules from the target directory (e.g ../galileo-install) to your micro
SD card
 Replace the bzImage found in the first partition (ESP) of your micro SD card with the one from your target directory (backup the bzImage on the micro SD card e.g. rename it to bzImage.old)

6. Then Copy the kernel modules from the target directory to the /lib/modules/ directory found in the second partition of your micro SD card.

2. Using the Raw files
-Replace syscall_32.tbl in /arch/x86/syscall/
-Replace syscall.h in /include/linux/
-Replace dynamic_dump_stack.c, Kconfig.debug, Makefile in /lib directory
and compile the kernel as above instruction.

Instructions to Run

1. Edit the Makefile as per the instructions above
2. From the /home directory call the

./dumpmode_test
./kallsyms_lookup_test
./signal_test


