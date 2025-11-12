#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Runs a CPU-intensive loop
void cpu_bound_loop() {
  long i;
  for (i = 0; i < 2000000000; i++) {
    asm volatile("nop");
  }
}

/*
 * nicetest.c
 * Tests priority-based scheduling.
 * Creates 4 CPU-bound children:
 * 2 with high priority (nice: -15)
 * 2 with low priority (nice: 15)
 */
int
main(int argc, char *argv[])
{
  printf("Starting nice test (2 high-pri, 2 low-pri)...\n");

  for (int i = 0; i < 4; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    } else if (pid == 0) {
      // Child process
      if (i < 2) {
        // High priority processes
        nice(getpid(), -15); // Set nice value to -5 (Priority 25)
        printf("High-priority process (PID: %d) starting CPU loop.\n", getpid());
        cpu_bound_loop();
        printf("High-priority process (PID: %d) finished.\n", getpid());
      } else {
        // Low priority processes
        nice(getpid(), 15); // Set nice value to 5 (Priority 15)
        printf("Low-priority process (PID: %d) starting CPU loop.\n", getpid());
        cpu_bound_loop();
        printf("Low-priority process (PID: %d) finished.\n", getpid());
      }
      exit(0);
    }
  }

  // Parent process waits for all children to finish
  for (int i = 0; i < 4; i++) {
    wait(0);
  }
  
  printf("Nice test complete.\n");
  exit(0);
}