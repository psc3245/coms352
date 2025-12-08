#include <stdio.h>
#include <semaphore.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "journal.h"

// Create a struct for a circular buffer
// It should have an array to hold the data, circular indexing with in and out indexes, count
// Also needs semaphore locking for keeping track of empty, full, and general access
typedef struct {
        int buffer[BUFFER_SIZE];
        int in;
        int out;
        int count;
        sem_t empty;
        sem_t full;
        sem_t lock;
} circular_buffer_t;

// Function signatures for circular buffer usage
void circ_buf_init(circular_buffer_t *buf);
void circ_buf_add(circular_buffer_t *buf, int item);
int circ_buf_remove(circular_buffer_t *buf);

// Initialize the variables and semaphores, nothing too complicated
void circ_buf_init(circular_buffer_t *buf) {
        buf->in = 0;
        buf->out = 0;
        buf->count = 0;
        sem_init(&buf->empty, 0, BUFFER_SIZE);
        sem_init(&buf->full, 0, 0);
        sem_init(&buf->lock, 0, 1);
}

void circ_buf_add(circular_buffer_t *buf, int item) {
        // First, check if we're full
        // If so, print a message that we have a full buffer and wait
        if (sem_trywait(&buf->empty) == -1) {
                if (errno == EAGAIN) {
                printf("thread stuck because of full buffer\n");
                fflush(stdout);
                }
                // Leave the wait on empty out of if statement so we ensure the buffer is empty
                sem_wait(&buf->empty);
        }
        // Wait on the access lock semaphore
        sem_wait(&buf->lock);
        // Add item, update count, and set circular indexing
        buf->buffer[buf->in] = item;
        buf->count++;
        buf->in = (buf->in + 1) % BUFFER_SIZE;
        // Release our locks and signal that a slot is full now
        sem_post(&buf->lock);
        sem_post(&buf->full);
}

int circ_buf_remove(circular_buffer_t *buf) {
        int val;
        // Wait for a spot to be full and for the lock on the buffer
        sem_wait(&buf->full);
        sem_wait(&buf->lock);
        // Remove item, update count, and set circular indexing
        val = buf->buffer[buf->out];
        buf->count --;
        buf->out = (buf->out + 1) % BUFFER_SIZE;
        // Signal semaphores to release locks
        sem_post(&buf->lock);
        sem_post(&buf->empty);
        return val;
}

// Function signature 
void *journal_metadata_write_worker(void *arg);
void *journal_commit_write_worker(void *arg);
void *checkpoint_metadata_worker(void *arg);

// Buffers for each stage
static circular_buffer_t request_buffer;
static circular_buffer_t journal_metadata_complete_buffer;
static circular_buffer_t journal_commit_complete_buffer;

// Threads for each stage
static pthread_t journal_metadata_write_thread;
static sem_t journal_metadata_write_thread_sem;

static pthread_t journal_commit_write_thread;
static sem_t journal_commit_write_thread_sem;

static pthread_t checkpoint_metadata_thread;
static sem_t checkpoint_metadata_thread_sem;

// Initialize buffers and setup threads
void init_journal() {
        // Call buf init on each buffer
        circ_buf_init(&request_buffer);
        circ_buf_init(&journal_metadata_complete_buffer);
        circ_buf_init(&journal_commit_complete_buffer);

        // Call sem init on each semaphore
        sem_init(&journal_metadata_write_thread_sem, 0, 0);
        sem_init(&journal_commit_write_thread_sem, 0, 0);
        sem_init(&checkpoint_metadata_thread_sem, 0, 0);

        // Setup each thread with its worker function
        if (pthread_create(&journal_metadata_write_thread, NULL, journal_metadata_write_worker, NULL) != 0) {
                fprintf(stderr, "Failed to create journal_metadata_write_thread\n");
        }
        if (pthread_create(&journal_commit_write_thread, NULL, journal_commit_write_worker, NULL) != 0) {
                fprintf(stderr, "Failed to create journal_commit_write_thread\n");
        }
        if (pthread_create(&checkpoint_metadata_thread, NULL, checkpoint_metadata_worker, NULL) != 0) {
                fprintf(stderr, "Failed to create checkpoint_metadata_thread\n");
        }
}

// First stage worker function
void* journal_metadata_write_worker(void *arg) {
        // Loop forever
        while(1) {
                // Wait for a request to process
                int write_id = circ_buf_remove(&request_buffer);
                // Issue the first four steps of a journal
                issue_write_data(write_id);
                issue_journal_txb(write_id);
                issue_journal_bitmap(write_id);
                issue_journal_inode(write_id);

                // Wait for each of the stages to sem post
                for(int i = 0; i < 4; i++) {
                        sem_wait(&journal_metadata_write_thread_sem);
                }

                // Move the request to the next stage of the pipeline
                circ_buf_add(&journal_metadata_complete_buffer, write_id);
        }
}

void* journal_commit_write_worker(void *arg) {

        for(;;) {
                // Wait for a request to process from the buffer
                int write_id = circ_buf_remove(&journal_metadata_complete_buffer);
                
                // Issue the write for this stage
                issue_journal_txe(write_id);
                
                // Signal we are done
                sem_wait(&journal_commit_write_thread_sem);
                
                // Move the request to the next stage of the pipeline
                circ_buf_add(&journal_commit_complete_buffer, write_id);
        }
}

void* checkpoint_metadata_worker(void *arg) {

        for(;;) {
                // Wait for a request to process
                int write_id = circ_buf_remove(&journal_commit_complete_buffer);
        
                // Issue the writes for this step
                issue_write_bitmap(write_id);
                issue_write_inode(write_id);
                
                // Wait for the signals for each of the steps
                for(int i = 0; i < 2; i++) {
                    sem_wait(&checkpoint_metadata_thread_sem);
                }
                // Call the final function
                write_complete(write_id);

        }

}

// Since we have the threads setup to wait for the requests, all we need to do is queue this one up
void request_write(int write_id) {
        circ_buf_add(&request_buffer, write_id);
}


/* These functions have been changed from a boolean flag to a semaphore
 * Simply signal the semaphore to indicate that we are ready to move on in the worker functions
 */
void journal_txb_complete(int write_id) {
        sem_post(&journal_metadata_write_thread_sem);
}

void journal_bitmap_complete(int write_id) {
        sem_post(&journal_metadata_write_thread_sem);
}

void journal_inode_complete(int write_id) {
        sem_post(&journal_metadata_write_thread_sem);
}

void write_data_complete(int write_id) {
        sem_post(&journal_metadata_write_thread_sem);
}

void journal_txe_complete(int write_id) {
    sem_post(&journal_commit_write_thread_sem);
}

void write_bitmap_complete(int write_id) {
    sem_post(&checkpoint_metadata_thread_sem);
}

void write_inode_complete(int write_id) {
    sem_post(&checkpoint_metadata_thread_sem);
}

