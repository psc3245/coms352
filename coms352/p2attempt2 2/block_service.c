#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "journal.h"

// Flag to know if we need to delay the first txe for testing purposes
static int first_txe = 1;

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

// Worker function for other thread for 
static void *txe_sleep_thread_worker(void* arg) {
	int write_id = (int)(long)arg;
	sleep(1);
	printf("delaying first txe");
	journal_txe_complete(write_id);
	return NULL;
}

void issue_journal_txe(int write_id) {
    printf("issue journal txe %d\n", write_id);
    
    // If it is the first txe, delay
    if (first_txe) {
        first_txe = 0;
        // Create a thread and start the worker function to sleep and simulate a delay
        pthread_t t;
        if (pthread_create(&t, NULL, txe_sleep_thread_worker, (void*)(long)write_id) != 0) {
                fprintf(stderr, "Failed to create journal_metadata_write_thread\n");
        }
        // Detach from the thread
        pthread_detach(t);
    } else {
    	// If not first time, write normally
        journal_txe_complete(write_id);
    }
}

void issue_write_bitmap(int write_id) {
    printf("issue write bitmap %d\n", write_id);
    write_bitmap_complete(write_id);
}

void issue_write_inode(int write_id) {
    printf("issue write inode %d\n", write_id);
    write_inode_complete(write_id);
}