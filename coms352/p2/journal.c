#include <stdio.h>
#include "journal.h"

int is_write_data_complete;
int is_journal_txb_complete;
int is_journal_bitmap_complete;
int is_journal_inode_complete;
int is_journal_txe_complete;
int is_write_bitmap_complete;
int is_write_inode_complete;

typedef struct {
    int buffer[BUFFER_SIZE];
    int in;
    int out;
    int count;
    pthread_mutex_t mutex;
    sem_t empty_slots;
    sem_t full_slots;
} circular_buffer_t;

circular_buffer_t request_buffer;
circular_buffer_t journal_metadata_buffer;
circular_buffer_t journal_commit_buffer;

static pthread_t journal_metadata_write_thread; 
static pthread_t journal_commit_write_thread;
static pthread_t checkpoint_metadata_thread;

sem_t journal_metadata_wait;
sem_t journal_commit_wait;
sem_t checkpoint_wait;

void *journal_metadata_write_worker(void *arg);
void *journal_commit_write_worker(void *arg);
void *checkpoint_metadata_worker(void *arg);



/* This function can be used to initialize the buffers and threads.
 */
void init_journal() {
    // Initialize your semaphores/mutexes here first!
    // ...

        // Create Thread 1
        if (pthread_create(&journal_metadata_write_thread, NULL, journal_metadata_write_worker, NULL) != 0) {
                perror("Failed to create journal_metadata_write_thread");
        }

        // Create Thread 2
        if (pthread_create(&journal_commit_write_thread, NULL, journal_commit_write_worker, NULL) != 0) {
                perror("Failed to create journal_commit_write_thread");
        }

        // Create Thread 3
        if (pthread_create(&checkpoint_metadata_thread, NULL, checkpoint_metadata_worker, NULL) != 0) {
                perror("Failed to create checkpoint_metadata_thread");
        }
}


/* This function is called by the file system to request writing data to
 * persistent storage.
 *
 * This simple version does not correctly deal with concurrency. It issues
 * all writes in the correct order, but it assumes issued writes always
 * complete immediately and therefore, it doesn't wait for each phase    
 * to complete.
 */
void request_write(int write_id) {

        // write data and journal metadata
        is_write_data_complete = 0;
        is_journal_txb_complete = 0;
        is_journal_bitmap_complete = 0;
        is_journal_inode_complete = 0;
        issue_write_data(write_id);
        issue_journal_txb(write_id);
        issue_journal_bitmap(write_id);
        issue_journal_inode(write_id);

        // normally we should wait here for the 4 issues to complete,
        // but this simple implementation doesn’t have concurrency,
        // so instead we will just call it an error if anything isn’t
        // complete in this version of the code
        if (!is_write_data_complete || !is_journal_txb_complete
                || !is_journal_bitmap_complete
                || !is_journal_inode_complete){
                printf("ERROR: writing data or journal failed!");
                return;
        }


        // commit transaction by writing txe
        is_journal_txe_complete = 0;
        issue_journal_txe();

        // normally we should wait here for the issue to complete,
        // but this simple implementation doesn’t have concurrency,
        // so instead we will just call it an error if it isn’t
        // complete in this version of the code
        if (!is_journal_txe_complete) {
                printf("ERROR: writing txe to journal failed!");
                return;
        }

        // checkpoint by writing metadata
        is_write_bitmap_complete = 0;
        is_write_inode_complete = 0;
        issue_write_bitmap(write_id);
        issue_write_inode(write_id);

        // normally we should wait here for the 2 issues to complete,
        // but this simple implementation doesn’t have concurrency,
        // so instead we will just call it an error if anything isn’t
        // complete in this version of the code
        if (!is_write_bitmap_complete || !is_write_inode_complete) {
                printf("ERROR: writing metadata failed!");
                return;
        }


        // tell the file system that the write is complete
        write_complete(write_id);
}

// Function YOU must write
void *journal_metadata_write_worker(void *arg) {
    while(1) { // <--- The infinite loop required by the PDF
        // 1. Wait for "buffer not empty"
        // 2. Take write_id out
        // 3. Do the work (call issue_... functions)
    }
}

void *journal_commit_write_worker(void *arg) {

}

void *checkpoint_metadata_worker(void *arg) {
        
}


/* This function is called by the block service when writing the txb block
 * to persistent storage is complete (e.g., it is physically written to  
 * disk).
 */
void journal_txb_complete(int write_id) {
        is_journal_txb_complete = 1;
}

void journal_bitmap_complete(int write_id) {
        is_journal_bitmap_complete = 1;
}

void journal_inode_complete(int write_id) {
        is_journal_inode_complete = 1;
}

void write_data_complete(int write_id) {
        is_write_data_complete = 1;
}

void journal_txe_complete(int write_id) {
        is_journal_txe_complete = 1;
}

void write_bitmap_complete(int write_id) {
        is_write_bitmap_complete = 1;
}

void write_inode_complete(int write_id) {
        is_write_inode_complete = 1;
}


