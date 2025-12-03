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

bool is_journal_write_complete;
bool is_journal_metadata_complete;
bool is_jounral_commit_complete;

void init_buffer(circular_buffer_t *buffer) {
        buffer->in = 0;
        buffer->out = 0;
        buffer->count = 0;
        pthread_mutex_init(&buffer->mutex, NULL);
        sem_init(&buffer->empty_slots, 0, BUFFER_SIZE);
        sem_init(&buffer->full_slots, 0, 0);
}


/* This function can be used to initialize the buffers and threads.
 */
void init_journal() {
    // Initialize your semaphores/mutexes here first!
    // ...


        is_journal_write_complete = false;
        is_journal_metadata_complete = false;
        is_jounral_commit_complete = false;

        sem_init(&journal_metadata_wait, 0, 0);
        sem_init(&journal_commit_wait, 0, 0);
        sem_init(&checkpoint_wait, 0, 0);


        init_buffer(&request_buffer);
        init_buffer(&journal_metadata_buffer);
        init_buffer(&journal_commit_buffer);

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

        
}

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

void write_data_complete(int write_id) {
        is_write_data_complete = 1;

        if (is_journal_txb_complete && is_journal_bitmap_complete && is_journal_inode_complete) {
                sem_post(&journal_metadata_wait);
        }
}

void journal_txb_complete(int write_id) {
        is_journal_txb_complete = 1;

        if (is_write_data_complete && is_journal_bitmap_complete && is_journal_inode_complete) {
                sem_post(&journal_metadata_wait);
        }
}

void journal_bitmap_complete(int write_id) {
        is_journal_bitmap_complete = 1;

        if (is_write_data_complete && is_journal_txb_complete && is_journal_inode_complete) {
                sem_post(&journal_metadata_wait);
        }
}

void journal_inode_complete(int write_id) {
        is_journal_inode_complete = 1;

        if (is_write_data_complete && is_journal_txb_complete && is_journal_bitmap_complete) {
                sem_post(&journal_metadata_wait);
        }
}



void journal_txe_complete(int write_id) {
        is_journal_txe_complete = 1;

        sem_post(&journal_commit_wait);
}

void write_bitmap_complete(int write_id) {
        is_write_bitmap_complete = 1;

        if (is_write_inode_complete) {
                sem_post(&checkpoint_wait);
        }
}

void write_inode_complete(int write_id) {
        is_write_inode_complete = 1;

        if (is_write_bitmap_complete) {
                sem_post(&checkpoint_wait);
        }
}


