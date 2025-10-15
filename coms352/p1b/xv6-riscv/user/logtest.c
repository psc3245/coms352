#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{

	startLogging();
	
	int pid;
	pid = fork();

	if (pid == 0) {
		nice(getpid(), 10);

	    volatile int calc=0;
		for (int i=0; i<100000; i++) {
			for (int j=0; j<10000; j++) {
				calc += i;
			}
		}

  } else if (pid > 0) {
    wait(0);
    stopLogging();

  } else {
    printf("Fork has failed.");
    exit(1);
  }

	exit(0);
}