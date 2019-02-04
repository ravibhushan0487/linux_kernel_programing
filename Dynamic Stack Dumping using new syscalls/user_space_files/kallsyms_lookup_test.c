#include <linux/random.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>

#define INSDUMP 359
#define RMDUMP 360
#define ACTIVE_KPROBES 361 //Returns total active kprobes
#define REMOVE_ACTIVE_KPROBES 362 //Removes active kprobe of a process.

int main()
{
	int ret,kprobe_id1,kprobe_id2,kprobe_id3;
	int active_kprobes_no;
	
	//Multiple DumpMode Testing
	kprobe_id1 = syscall(INSDUMP, "get_active_kprobes", 0);
	kprobe_id2 = syscall(INSDUMP, "sys_bpf", 1);
	kprobe_id3 = syscall(INSDUMP, "sys_open", 2);

	
	active_kprobes_no = syscall(ACTIVE_KPROBES); 
	printf("Active Kprobes:%d",active_kprobes_no);
	ret = syscall(RMDUMP, kprobe_id1);
	ret = open("/dev/tty23",O_RDWR);
    if(ret < 0) 
    {
        printf("UNABLE TO OPEN TTY23\n");
        exit(1);
    }
    printf("OPENED TTY23\n");
   
	active_kprobes_no = syscall(ACTIVE_KPROBES);
	ret = open("/dev/tty23",O_RDWR);
    if(ret < 0) 
    {
        printf("UNABLE TO OPEN TTY23\n");
        exit(1);
    }
    printf("OPENED TTY23\n");
    sleep(5);
	ret = syscall(RMDUMP, kprobe_id1);
	ret = syscall(RMDUMP, kprobe_id2);
	ret = syscall(RMDUMP, kprobe_id3);

	return 0;
}
