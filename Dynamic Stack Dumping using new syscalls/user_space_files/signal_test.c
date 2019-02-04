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
#define REMOVE_ACTIVE_KPROBES 362 //Removes active kprobe of a process

void handle_signal(int signal) {

    // Find out which signal we're handling
    switch (signal) {
        case SIGTERM:
            syscall(REMOVE_ACTIVE_KPROBES);
            exit(0);
            break;
        case SIGKILL:
            syscall(REMOVE_ACTIVE_KPROBES);
            exit(0);
            break;
        case SIGINT:
           	syscall(REMOVE_ACTIVE_KPROBES);
            exit(0);
            break;
        case SIGSTOP:
        	syscall(REMOVE_ACTIVE_KPROBES);
            exit(0);
        	break;
        default:
            printf("Caught wrong signal: %d\n", signal);
            exit(0);
            return;
    }
}

void fnExit1(void) {
    printf("exiting program\n");
    syscall(REMOVE_ACTIVE_KPROBES);
}

void handle_sigint(int sig){
    printf("pressed ctrl+c\n");
    syscall(REMOVE_ACTIVE_KPROBES);
}

int main()
{
	int ret,kprobe_id1,kprobe_id2,kprobe_id3;
	int active_kprobes_no;
	
	 atexit (fnExit1);
	 signal(SIGINT, handle_sigint);
	 signal(SIGTERM, handle_sigint);
	 signal(SIGKILL, handle_sigint);
	 signal(SIGSTOP, handle_sigint);
	//Multiple Kprobes
	kprobe_id1 = syscall(INSDUMP, "get_active_kprobes", 1);
	kprobe_id2 = syscall(INSDUMP, "sys_open", 2);

	
	active_kprobes_no = syscall(ACTIVE_KPROBES); //Get Active Kprobes
	active_kprobes_no = syscall(ACTIVE_KPROBES); //Get Active Kprobes
	printf("total active kprobes: %d",active_kprobes_no);
	sleep(3);
	
	ret = open("/dev/tty24",O_RDWR);
	if(ret < 0) 
    {
        printf("UNABLE TO OPEN TTY24\n");
        exit(1);
    }
    printf("OPENED TTY24\n");
    sleep(1);
    close(ret);

	ret = syscall(RMDUMP, kprobe_id2);
	active_kprobes_no = syscall(ACTIVE_KPROBES);
	active_kprobes_no = syscall(ACTIVE_KPROBES);
	ret = syscall(RMDUMP, kprobe_id1);
	sleep(5);
	//Multiple DumpMode Testing
	kprobe_id1 = syscall(INSDUMP, "get_active_kprobes", 0);
	kprobe_id2 = syscall(INSDUMP, "do_fork", 1);
	kprobe_id3 = syscall(INSDUMP, "sys_open", 2);

	if(fork() == 0) {
		printf("fork : inside child process");
		active_kprobes_no = syscall(ACTIVE_KPROBES); 
		 atexit (fnExit1);
	 	 signal(SIGINT, handle_sigint);
		 signal(SIGTERM, handle_sigint);
		 signal(SIGKILL, handle_sigint);
		 signal(SIGSTOP, handle_sigint);
	
		ret = syscall(RMDUMP, kprobe_id1);
		ret = open("/dev/tty23",O_RDWR);
	    if(ret < 0) 
	    {
	        printf("UNABLE TO OPEN TTY23\n");
	        exit(1);
	    }   
		kprobe_id2 = syscall(INSDUMP, "do_fork", 1);
	    printf("OPENED TTY23\n");
	    fork();
	    sleep(3);
	    exit(0);
	} else {
		printf("fork : inside parent");
		active_kprobes_no = syscall(ACTIVE_KPROBES);
		ret = open("/dev/tty23",O_RDWR);
	    if(ret < 0) 
	    {
	        printf("UNABLE TO OPEN TTY23\n");
	        exit(1);
	    }
	    printf("OPENED TTY23\n");

		ret = syscall(RMDUMP, kprobe_id1);
		ret = syscall(RMDUMP, kprobe_id2);
		ret = syscall(RMDUMP, kprobe_id3);
		exit(0);
	}

	return 0;
}
