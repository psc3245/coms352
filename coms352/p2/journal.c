#include <stdio.h>
#include "journal.h"

volatile int is_write_data_complete;
volatile int is_journal_txb_complete;
volatile int is_journal_bitmap_complete;
volatile int is_journal_inode_complete;
volatile int is_journal_txe_complete;
volatile int is_write_bitmap_complete;
volatile int is_write_inode_complete;

typedef struct {
    int buffer[BUFFER_SIZE];
    int in;
    int out;
    int count;
    pthread_mutex_t mutex;
    sem_t empty_slots;
    sem_t full_slots;
} circular_buffer_t;

// Buffers
circular_buffer_t request_buffer;
circular_buffer_t journal_metadata_buffer;
circular_buffer_t journal_commit_buffer;

// Buffer functions
void circ_buffer_add(circular_buffer_t *buffer, int write_id);
void circ_buffer_remove(circular_buffer_t *buffer);

// Threads
static pthread_t journal_metadata_write_thread; 
static pthread_t journal_commit_write_thread;
static pthread_t checkpoint_metadata_thread;

// Semaphores
sem_t journal_metadata_wait;
sem_t journal_commit_wait;
sem_t checkpoint_wait;

// Thread worker functions
void *journal_metadata_write_worker(void *arg);
void *journal_commit_write_worker(void *arg);
void *checkpoint_metadata_worker(void *arg);


void init_buffer(circular_buffer_t *buffer) {
        buffer->in = 0;
        buffer->out = 0;
        buffer->count = 0;
        pthread_mutex_init(&buffer->mutex, NULL);
        sem_init(&buffer->empty_slots, 0, BUFFER_SIZE);
        sem_init(&buffer->full_slots, 0, 0);
}

void circ_buffer_add(circular_buffer_t *buffer, int write_id) {
        sem_wait(&buffer -> empty_slots);

        pthread_mutex_lock(&buffer->mutex);

        buffer->buffer[buffer->in] = val;
        buffer->in = (buffer->in + 1) % BUFFER_SIZE;
        buffer->count++;

        pthread_mutex_unlock(&buffer->mutex);

        sem_post(&buffer->full_slots);
}


int circ_buffer_remove(circular_buffer_t *buffer) {
        int val;

        sem_wait(&buffer->full_slots);

        pthread_mutex_lock(&buffer->mutex);

        val = buffer->buffer[buffer->out];
        buffer->out = (buffer->out + 1) % BUFFER_SIZE;
        buffer->count--;

        pthread_mutex_unlock(&buffer->mutex);

        sem_post(&buffer->empty_slots);

        return val;
}

void *journal_metadata_write_worker(void *arg) {
        while (1) {
                int id = buffer_remove(&request_buffer);

                issue_write_data(id);

                is_write_data_complete = 0;
                is_journal_txb_complete = 0;
                is_journal_bitmap_complete = 0;
                is_journal_inode_complete = 0;

                issue_write_data(id);
                issue_journal_txb(id);
                issue_journal_bitmap(id);
                issue_journal_inode(id);

                sem_wait(&journal_metadata_wait);

                if (journal_metadata_buffer.count == BUFFER_SIZE) {
                     printf("thread stuck because of full buffer\n");
                }
                circ_buffer_add(&journal_metadata_buffer, id);
        }
        return NULL;
}

void *journal_commit_write_worker(void *arg) {
        while(1) { 
                int id = circ_buffer_remove(&journal_metadata_buffer);

                is_journal_txe_complete = 0;
                
                issue_journal_txe(id);

                sem_wait(&journal_commit_wait);

                circ_buffer_add(&journal_commit_buffer, id);
        }

        return NULL;
}

void *checkpoint_metadata_worker(void *arg) {
        while(1) { 
                int id = circ_buffer_remove(&journal_commit_buffer);

                is_write_bitmap_complete = 0;
                is_write_inode_complete = 0;

                issue_write_bitmap(id);
                issue_write_inode(id);

                sem_wait(&checkpoint_wait);

                write_complete(id);
        }
        return NULL;
}


/* This function can be used to initialize the buffers and threads.
 */
void init_journal() {

        sem_init(&journal_metadata_wait, 0, 0);
        sem_init(&journal_commit_wait, 0, 0);
        sem_init(&checkpoint_wait, 0, 0);


        init_buffer(&request_buffer);
        init_buffer(&journal_metadata_buffer);
        init_buffer(&journal_commit_buffer);

        // Create Thread 1
        if (pthread_create(&journal_metadata_write_thread, NULL, journal_metadata_write_worker, NULL) != 0) {
                printf("Failed to create journal_metadata_write_thread");
                return 1;
        }

        // Create Thread 2
        if (pthread_create(&journal_commit_write_thread, NULL, journal_commit_write_worker, NULL) != 0) {
                printf("Failed to create journal_commit_write_thread");
                return 1;
        }

        // Create Thread 3
        if (pthread_create(&checkpoint_metadata_thread, NULL, checkpoint_metadata_worker, NULL) != 0) {
                printf("Failed to create checkpoint_metadata_thread");
                return 1;
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
        circ_buffer_add(&request_buffer, write_id);
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


