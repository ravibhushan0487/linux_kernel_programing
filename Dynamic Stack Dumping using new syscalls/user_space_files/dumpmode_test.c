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
#define REMOVE_ACTIVE_KPROBES 362 //Removes active kprobe of a process. Not used here. Will be used in signal_test.c

static int child_func(void* arg) {
  int ret,kprobe_id1;
  printf("Inside Spawned Child");
  ret = open("/dev/tty24",O_RDWR);
	if(ret < 0) 
	{
	    printf("UNABLE TO OPEN TTY24\n");
	    exit(1);
	}
	printf("OPENED TTY24\n");
	sleep(1);
	close(ret);

	kprobe_id1 = syscall(INSDUMP, "get_active_kprobes", 2);
	
	syscall(RMDUMP, kprobe_id1);
  exit(0);
}

int main()
{
	int ret,kprobe_id1,kprobe_id2,kprobe_id3;
	int active_kprobes_no;
	char buf[100];
	unsigned long flags = 0;
	// Allocate stack for child task.
	const int STACK_SIZE = 65536;
  	char* stack = malloc(STACK_SIZE);
  	if (!stack) 
	 {
	    perror("malloc");
	    exit(1);
	  }
	
	//Multiple DumpMode Testing
	kprobe_id1 = syscall(INSDUMP, "get_active_kprobes", 0);
	kprobe_id2 = syscall(INSDUMP, "do_fork", 1);
	kprobe_id3 = syscall(INSDUMP, "sys_open", 2);

	if(fork() == 0) {
		printf("fork : inside child process");
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
	    fork();
	    sleep(3);
	    if (clone(child_func, stack + STACK_SIZE, flags | SIGCHLD, buf) == -1) 
		  {
		    perror("clone");
		    exit(1);
		  }

		  int status;
		  if (wait(&status) == -1) {
		    perror("wait");
		    exit(1);
		  }
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
	    sleep(5);
		ret = syscall(RMDUMP, kprobe_id1);
		ret = syscall(RMDUMP, kprobe_id2);
		ret = syscall(RMDUMP, kprobe_id3);
	}

	return 0;
}
