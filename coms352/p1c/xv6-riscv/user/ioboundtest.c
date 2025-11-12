#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Runs a CPU-intensive loop
void cpu_bound_loop() {
  long i;
  for (i = 0; i < 20000000000; i++) {
    asm volatile("nop");
  }
}

// Runs an I/O-intensive loop
void io_bound_loop() {
  for (int i = 0; i < 100; i++) {
    // sleep(1) simulates I/O by yielding the CPU
    pause(1);
  }
}

/*
 * ioboundtest.c
 * Tests how schedulers handle mixed workloads.
 * Creates 4 children:
 * 2 CPU-bound (long loop)
 * 2 I/O-bound (sleep loop)
 *
 * Expected behavior:
 * - RR & RRSP: The 2 CPU-bound processes will dominate the CPU. The I/O-bound
 * processes will sleep, wake up, run briefly, and sleep again.
 * - MLFQ: This is the key test. The CPU-bound processes will use their
 * quanta and be demoted to Q0. The I/O-bound processes will call sleep(),
 * which is a *voluntary* release of the CPU (not a preemption).
 * They will *not* be demoted. They will stay in Q1, get high priority
 * every time they wake up, run quickly, and sleep again.
 * The I/O processes should finish *much* faster.
 */
int
main(int argc, char *argv[])
{
  printf("Starting I/O vs CPU test (2 CPU-bound, 2 I/O-bound)...\n");

  // startLogging();

  for (int i = 0; i < 4; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    } else if (pid == 0) {
      // Child process
      if (i < 2) {
        // CPU-bound processes
        printf("CPU-bound process (PID: %d) starting CPU loop.\n", getpid());
        cpu_bound_loop();
        printf("CPU-bound process (PID: %d) finished.\n", getpid());
      } else {
        // I/O-bound processes
        printf("I/O-bound process (PID: %d) starting I/O loop.\n", getpid());
        io_bound_loop();
        printf("I/O-bound process (PID: %d) finished.\n", getpid());
      }
      exit(0);
    }
  }

  // Parent process waits for all children to finish
  for (int i = 0; i < 4; i++) {
    wait(0);
  }

  // stopLogging();
  
  printf("I/O vs CPU test complete.\n");
  exit(0);
}