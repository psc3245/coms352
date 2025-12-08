#include <stdio.h>
#include <semaphore.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "journal.h"

typedef struct {
        int buffer[BUFFER_SIZE];
        int in;
        int out;
        int count;
        sem_t empty;
        sem_t full;
        sem_t lock;
} circular_buffer_t;

void circ_buf_init(circular_buffer_t *buf);
void circ_buf_add(circular_buffer_t *buf, int item);
int circ_buf_remove(circular_buffer_t *buf);

void circ_buf_init(circular_buffer_t *buf) {
        buf->in = 0;
        buf->out = 0;
        buf->count = 0;
        sem_init(&buf->empty, 0, BUFFER_SIZE);
        sem_init(&buf->full, 0, 0);
        sem_init(&buf->lock, 0, 1);
}

void circ_buf_add(circular_buffer_t *buf, int item) {
        if (sem_trywait(&buf->empty) == -1) {
                if (errno == EAGAIN) {
                printf("thread stuck because of full buffer\n");
                fflush(stdout);
                }
                sem_wait(&buf->empty);
        }
        sem_wait(&buf->lock);
        buf->buffer[buf->in] = item;
        buf->count++;
        buf->in = (buf->in + 1) % BUFFER_SIZE;
        sem_post(&buf->lock);
        sem_post(&buf->full);
}

int circ_buf_remove(circular_buffer_t *buf) {
        int val;
        sem_wait(&buf->full);
        sem_wait(&buf->lock);
        val = buf->buffer[buf->out];
        buf->count --;
        buf->out = (buf->out + 1) % BUFFER_SIZE;
        sem_post(&buf->lock);
        sem_post(&buf->empty);
        return val;
}

void *journal_metadata_write_worker(void *arg);
void *journal_commit_write_worker(void *arg);
void *checkpoint_metadata_worker(void *arg);

static pthread_mutex_t completion_lock;
static pthread_cond_t completion_cv;

static circular_buffer_t request_buffer;
static circular_buffer_t journal_metadata_complete_buffer;
static circular_buffer_t journal_commit_complete_buffer;

static pthread_t journal_metadata_write_thread;
static sem_t journal_metadata_write_thread_sem;

static pthread_t journal_commit_write_thread;
static sem_t journal_commit_write_thread_sem;

static pthread_t checkpoint_metadata_thread;
static sem_t checkpoint_metadata_thread_sem;

/* This function can be used to initialize the buffers and threads.
 */
void init_journal() {
	// initialize buffers and threads here

        circ_buf_init(&request_buffer);
        circ_buf_init(&journal_metadata_complete_buffer);
        circ_buf_init(&journal_commit_complete_buffer);

        sem_init(&journal_metadata_write_thread_sem, 0, 0);
        sem_init(&journal_commit_write_thread_sem, 0, 0);
        sem_init(&checkpoint_metadata_thread_sem, 0, 0);


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

void* journal_metadata_write_worker(void *arg) {

        while(1) {
                int write_id = circ_buf_remove(&request_buffer);

                issue_write_data(write_id);
                issue_journal_txb(write_id);
                issue_journal_bitmap(write_id);
                issue_journal_inode(write_id);

                // wait for each of the stages to sem post
                for(int i = 0; i < 4; i++) {
                        sem_wait(&journal_metadata_write_thread_sem);
                }


                circ_buf_add(&journal_metadata_complete_buffer, write_id);
        }
}

void* journal_commit_write_worker(void *arg) {

        for(;;) {
                int write_id = circ_buf_remove(&journal_metadata_complete_buffer);
                
                issue_journal_txe(write_id);
                
                sem_wait(&journal_commit_write_thread_sem);
                
                circ_buf_add(&journal_commit_complete_buffer, write_id);
        }
}

void* checkpoint_metadata_worker(void *arg) {

        for(;;) {

                int write_id = circ_buf_remove(&journal_commit_complete_buffer);
        
                issue_write_bitmap(write_id);
                issue_write_inode(write_id);
                
                for(int i = 0; i < 2; i++) {
                    sem_wait(&checkpoint_metadata_thread_sem);
                }
                
                write_complete(write_id);

        }

}

void request_write(int write_id) {
        circ_buf_add(&request_buffer, write_id);
}


/* This function is called by the block service when writing the txb block
 * to persistent storage is complete (e.g., it is physically written to 
 * disk).
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

