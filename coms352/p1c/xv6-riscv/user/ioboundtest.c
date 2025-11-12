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
 */
int
main(int argc, char *argv[])
{
  printf("Starting I/O vs CPU test (2 CPU-bound, 2 I/O-bound)...\n");

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
  
  printf("I/O vs CPU test complete.\n");
  exit(0);
}