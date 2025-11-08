#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


// Logtest is a user space tool to demonstrate the logging capabilities.
// This is done by setting a process's nice value to very high, 
// then showing how the higher nice value allows the process to control the cpu for longer stretches of time.
int
main(int argc, char *argv[])
{
	startLogging();
	
	// Fork the current process to create a child process
	int pid;
	pid = fork();

	// If we are in the child process, set the nice value and hog the cpu
	if (pid == 0) {
		nice(getpid(), 10);

	    volatile int calc=0;
		for (int i=0; i<100000; i++) {
			for (int j=0; j<10000; j++) {
				calc += i;
			}
		}

  } 
  // If we are in parent process, wait and allow child to use CPU
  else if (pid > 0) {
    wait(0);
    stopLogging();
  } 
  // If fork has failed, print a message and exit
  else {
    printf("Fork has failed.");
    exit(1);
  }

	exit(0);
}