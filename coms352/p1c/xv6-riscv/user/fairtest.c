#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Runs a CPU-intensive loop
void cpu_bound_loop() {
  long i;
  for (i = 0; i < 2000000000; i++) {
    asm volatile("nop"); // No-operation assembly instruction
  }
}

/*
 * fairtest.c
 * Tests the fairness of the schedulers.
 * Creates 4 identical CPU-bound child processes.
 *
 * Expected behavior:
 * - RR: All 4 processes should share CPU time roughly equally in a round-robin fashion.
 * - RRSP: All processes have the same default nice value (0), so they have the same
 * priority. Should behave identically to RR.
 * - MLFQ: All processes start in Q1 (nice=0). They will use their quanta and be
 * demoted to Q0, where they will run round-robin.
 */
int
main(int argc, char *argv[])
{
  printf("Starting fairness test (4 CPU-bound processes)...\n");

  // Start logging. You can also do this from the xv6 shell.
  // startLogging();

  for (int i = 0; i < 4; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    } else if (pid == 0) {
      // Child process
      printf("Process %d (PID: %d) starting CPU loop.\n", i + 1, getpid());
      cpu_bound_loop();
      printf("Process %d (PID: %d) finished.\n", i + 1, getpid());
      exit(0);
    }
  }

  // Parent process waits for all children to finish
  for (int i = 0; i < 4; i++) {
    wait(0);
  }

  // Stop logging.
  // stopLogging();

  printf("Fairness test complete.\n");
  exit(0);
}