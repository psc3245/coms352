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
 * 2 with high priority (nice: -5)
 * 2 with low priority (nice: 5)
 *
 * Expected behavior:
 * - RR: Ignores nice values. All 4 processes share time equally.
 * - RRSP: Strict priority. The 2 'nice -5' processes should run exclusively,
 * sharing time via RR. The 'nice 5' processes should *never* run
 * until the high-priority ones are finished (i.e., they will starve).
 * - MLFQ: 'nice -5' processes start in Q2. 'nice 5' processes start in Q1.
 * The Q2 processes will run, be demoted to Q1, and then all 4 will
 * eventually end up in Q0, running round-robin.
 */
int
main(int argc, char *argv[])
{
  printf("Starting nice test (2 high-pri, 2 low-pri)...\n");

  // startLogging();

  for (int i = 0; i < 4; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    } else if (pid == 0) {
      // Child process
      if (i < 2) {
        // High priority processes
        nice(getpid(), -5); // Set nice value to -5 (Priority 25)
        printf("High-priority process (PID: %d) starting CPU loop.\n", getpid());
        cpu_bound_loop();
        printf("High-priority process (PID: %d) finished.\n", getpid());
      } else {
        // Low priority processes
        nice(getpid(), 5); // Set nice value to 5 (Priority 15)
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

  // stopLogging();
  
  printf("Nice test complete.\n");
  exit(0);
}