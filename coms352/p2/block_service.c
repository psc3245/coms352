#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "journal.h"

void *delayed_txe_thread(void *arg) {
    int write_id = *(int*)arg;
    free(arg);                 // free the heap memory passed in
    printf("\n[BlockService] SIMULATION: Disk is slow writing TXE... (Thread Sleeping)\n");
    sleep(1);
    printf("[BlockService] SIMULATION: Disk finished writing TXE.\n");

    journal_txe_complete(write_id);
    return NULL;
}

void issue_journal_txe(int write_id) {
    printf("issue journal txe %d\n", write_id);

    static int is_first_time = 1;

    if (is_first_time) {
        is_first_time = 0;
        pthread_t tid;
        int *temp_id = malloc(sizeof(int));
        if (!temp_id) {
            perror("malloc");
            journal_txe_complete(write_id); // fallback: complete synchronously
            return;
        }
        *temp_id = write_id;
        if (pthread_create(&tid, NULL, delayed_txe_thread, temp_id) != 0) {
            perror("pthread_create");
            free(temp_id);
            journal_txe_complete(write_id); // fallback
            return;
        }
        /* We won't join this thread later; detach it so resources are reclaimed. */
        pthread_detach(tid);
    } else {
        journal_txe_complete(write_id);
    }
}

void issue_journal_txb(int write_id) {
	printf("issue journal txb %d\n", write_id);
	journal_txb_complete(write_id);
}

void issue_journal_bitmap(int write_id) {
	printf("issue journal bitmap %d\n", write_id);
	journal_bitmap_complete(write_id);
}

void issue_journal_inode(int write_id) {
	printf("issue journal inode %d\n", write_id);
	journal_inode_complete(write_id);
}

void issue_write_data(int write_id) {
	printf("issue write data %d\n", write_id);
	write_data_complete(write_id);
}

void issue_write_bitmap(int write_id) {
	printf("issue write bitmap %d\n", write_id);
	write_bitmap_complete(write_id);
}

void issue_write_inode(int write_id) {
	printf("issue write inode %d\n", write_id);
	write_inode_complete(write_id);
}
