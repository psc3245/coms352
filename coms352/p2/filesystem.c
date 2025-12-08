#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "journal.h"

void write_complete(int write_id) {
	printf("test write complete %d\n", write_id);
}

int main(int argc, char *argv[]) {
	init_journal();

	for (int i=0; i<16; i++) {
		printf("requesting test write %d\n", i);
		request_write(i);
	}

	journal_wait_for_all();


	return 0;
}
