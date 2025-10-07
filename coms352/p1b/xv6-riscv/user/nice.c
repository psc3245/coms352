#include "kernel/types.h"
#include "user/user.h"

int nice(int pid, int inc) {
}

int main(int argc, char* argv[]) {
	int pid, inc;

	if (argc != 2) {
		exit(0);
	}
	pid = argv[0];
	inc = argv[1];

	
}
