// journal.c
// Journaling implementation — "second fix": completion handlers do NOT issue any new I/O.
// They only mark completion, decrement pending_io, post semaphores, and attempt to free the slot.
//
// Usage: compile with your existing block layer (issue_* implementations) and main.
// Call init_journal() at startup, then request_write(id) to queue requests.
// Call journal_wait_for_all() from main() to block until all outstanding requests complete.
//
// Assumptions:
// - write_id is used as the slot index in [0, MAX_REQUESTS).
// - BUFFER_SIZE is defined in journal.h.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <unistd.h>
#include "journal.h"

#define MAX_REQUESTS 100

typedef struct {
    int write_id;
    int in_use;
    // completion flags
    int write_data_complete;
    int journal_txb_complete;
    int journal_bitmap_complete;
    int journal_inode_complete;
    int journal_txe_complete;
    int write_bitmap_complete;
    int write_inode_complete;
    // lifecycle helpers
    int pending_io;        // number of outstanding IOs issued for this slot
    int checkpoint_done;   // set when checkpoint stage has called write_complete
    // PER-REQUEST SEMAPHORES
    sem_t metadata_wait;
    sem_t commit_wait;
    sem_t checkpoint_wait;
} request_state_t;

static request_state_t request_states[MAX_REQUESTS];
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t state_free_cond;

/* Note: write_id is used as direct slot index here (0..MAX_REQUESTS-1).
   If you use external/unbounded request IDs, switch to a slot allocator. */
static inline request_state_t* get_request_state(int write_id) {
    if (write_id < 0 || write_id >= MAX_REQUESTS) return NULL;
    return &request_states[write_id];
}

typedef struct {
    int buffer[BUFFER_SIZE];
    int in;
    int out;
    int count;
    pthread_mutex_t mutex;
    sem_t empty_slots;
    sem_t full_slots;
} circular_buffer_t;

/* Buffers */
static circular_buffer_t request_buffer;
static circular_buffer_t journal_metadata_buffer;
static circular_buffer_t journal_commit_buffer;

/* Forward declarations for block-layer calls (implemented in block layer) */
void issue_write_data(int write_id);
void issue_journal_txb(int write_id);
void issue_journal_bitmap(int write_id);
void issue_journal_inode(int write_id);
void issue_journal_txe(int write_id);
void issue_write_bitmap(int write_id);
void issue_write_inode(int write_id);

/* Utility: try to free a slot if checkpoint done and no pending IOs.
   Must be called with state_mutex held (or will lock internally if needed). */
static void try_free_slot_if_done_locked(request_state_t *state) {
    if (!state) return;
    if (state->in_use && state->checkpoint_done && state->pending_io == 0) {
        int old_id = state->write_id;
        state->write_id = -1;
        state->write_data_complete = 0;
        state->journal_txb_complete = 0;
        state->journal_bitmap_complete = 0;
        state->journal_inode_complete = 0;
        state->journal_txe_complete = 0;
        state->write_bitmap_complete = 0;
        state->write_inode_complete = 0;
        state->checkpoint_done = 0;
        state->in_use = 0;
        pthread_cond_signal(&state_free_cond);
        (void)old_id; // could log if desired
    }
}

/* Circular buffer helpers */
static void init_buffer(circular_buffer_t *buffer) {
    buffer->in = 0;
    buffer->out = 0;
    buffer->count = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    sem_init(&buffer->empty_slots, 0, BUFFER_SIZE);
    sem_init(&buffer->full_slots, 0, 0);
}

static void circ_buffer_add(circular_buffer_t *buffer, int write_id) {
    sem_wait(&buffer->empty_slots);
    pthread_mutex_lock(&buffer->mutex);
    buffer->buffer[buffer->in] = write_id;
    buffer->in = (buffer->in + 1) % BUFFER_SIZE;
    buffer->count++;
    pthread_mutex_unlock(&buffer->mutex);
    sem_post(&buffer->full_slots);
}

static int circ_buffer_remove(circular_buffer_t *buffer) {
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

/* Worker threads */
static pthread_t journal_metadata_write_thread;
static pthread_t journal_commit_write_thread;
static pthread_t checkpoint_metadata_thread;

/* journal_metadata_write_worker:
   Issues 4 metadata IOs per request and waits for their completion (metadata_wait).
   We increment pending_io before each issue_* call to ensure lifecycle tracking. */
static void *journal_metadata_write_worker(void *arg) {
    (void)arg;
    while (1) {
        int id = circ_buffer_remove(&request_buffer);
        request_state_t *state = get_request_state(id);
        if (!state) continue;

        /* Reset metadata flags under lock */
        pthread_mutex_lock(&state_mutex);
        state->write_data_complete = 0;
        state->journal_txb_complete = 0;
        state->journal_bitmap_complete = 0;
        state->journal_inode_complete = 0;
        pthread_mutex_unlock(&state_mutex);

        /* Issue the four metadata operations; increment pending_io BEFORE issuing */
        pthread_mutex_lock(&state_mutex);
        state->pending_io += 1;
        pthread_mutex_unlock(&state_mutex);
        issue_write_data(id);

        pthread_mutex_lock(&state_mutex);
        state->pending_io += 1;
        pthread_mutex_unlock(&state_mutex);
        issue_journal_txb(id);

        pthread_mutex_lock(&state_mutex);
        state->pending_io += 1;
        pthread_mutex_unlock(&state_mutex);
        issue_journal_bitmap(id);

        pthread_mutex_lock(&state_mutex);
        state->pending_io += 1;
        pthread_mutex_unlock(&state_mutex);
        issue_journal_inode(id);

        /* Wait for the 4 metadata completions (metadata_wait) */
        sem_wait(&state->metadata_wait);

        /* Push into next stage */
        circ_buffer_add(&journal_metadata_buffer, id);
    }
    return NULL;
}

/* journal_commit_write_worker:
   Waits for metadata->commit stage, issues TXE, waits for TXE completion (commit_wait),
   then pushes to checkpoint stage. */
static void *journal_commit_write_worker(void *arg) {
    (void)arg;
    while (1) {
        int id = circ_buffer_remove(&journal_metadata_buffer);
        request_state_t *state = get_request_state(id);
        if (!state) continue;

        pthread_mutex_lock(&state_mutex);
        state->journal_txe_complete = 0;
        pthread_mutex_unlock(&state_mutex);

        /* issue journal txe (increment pending_io first) */
        pthread_mutex_lock(&state_mutex);
        state->pending_io += 1;
        pthread_mutex_unlock(&state_mutex);
        issue_journal_txe(id);

        /* wait for commit completion */
        sem_wait(&state->commit_wait);

        circ_buffer_add(&journal_commit_buffer, id);
    }
    return NULL;
}

/* checkpoint_metadata_worker:
   Issues final writes (bitmap & inode), waits for their completions (checkpoint_wait),
   calls write_complete(id), and then marks checkpoint_done and attempts to free the slot.
   Note: freed only when pending_io hits 0 too. */
static void *checkpoint_metadata_worker(void *arg) {
    (void)arg;
    while (1) {
        int id = circ_buffer_remove(&journal_commit_buffer);
        request_state_t *state = get_request_state(id);
        if (!state) continue;

        pthread_mutex_lock(&state_mutex);
        state->write_bitmap_complete = 0;
        state->write_inode_complete = 0;
        pthread_mutex_unlock(&state_mutex);

        /* Issue the two final I/O ops and increment pending_io BEFORE issuing */
        pthread_mutex_lock(&state_mutex);
        state->pending_io += 1;
        pthread_mutex_unlock(&state_mutex);
        issue_write_bitmap(id);

        pthread_mutex_lock(&state_mutex);
        state->pending_io += 1;
        pthread_mutex_unlock(&state_mutex);
        issue_write_inode(id);

        /* Wait for both write bitmap & inode completions */
        sem_wait(&state->checkpoint_wait);

        /* Final user-visible completion */
        write_complete(id);

        /* Mark checkpoint done and try to free (free deferred until pending_io==0) */
        pthread_mutex_lock(&state_mutex);
        state->checkpoint_done = 1;
        try_free_slot_if_done_locked(state);
        pthread_mutex_unlock(&state_mutex);
    }
    return NULL;
}

/* Initialization */
void init_journal(void) {
    pthread_cond_init(&state_free_cond, NULL);

    for (int i = 0; i < MAX_REQUESTS; i++) {
        request_states[i].in_use = 0;
        request_states[i].write_id = -1;
        request_states[i].write_data_complete = 0;
        request_states[i].journal_txb_complete = 0;
        request_states[i].journal_bitmap_complete = 0;
        request_states[i].journal_inode_complete = 0;
        request_states[i].journal_txe_complete = 0;
        request_states[i].write_bitmap_complete = 0;
        request_states[i].write_inode_complete = 0;
        request_states[i].pending_io = 0;
        request_states[i].checkpoint_done = 0;
        /* Initialize semaphores once and keep valid for process lifetime */
        sem_init(&request_states[i].metadata_wait, 0, 0);
        sem_init(&request_states[i].commit_wait, 0, 0);
        sem_init(&request_states[i].checkpoint_wait, 0, 0);
    }

    init_buffer(&request_buffer);
    init_buffer(&journal_metadata_buffer);
    init_buffer(&journal_commit_buffer);

    __sync_synchronize();

    if (pthread_create(&journal_metadata_write_thread, NULL, journal_metadata_write_worker, NULL) != 0) {
        fprintf(stderr, "Failed to create journal_metadata_write_thread\n");
        exit(1);
    }
    if (pthread_create(&journal_commit_write_thread, NULL, journal_commit_write_worker, NULL) != 0) {
        fprintf(stderr, "Failed to create journal_commit_write_thread\n");
        exit(1);
    }
    if (pthread_create(&checkpoint_metadata_thread, NULL, checkpoint_metadata_worker, NULL) != 0) {
        fprintf(stderr, "Failed to create checkpoint_metadata_thread\n");
        exit(1);
    }
}

/* request_write: claim the slot (here using write_id as slot index).
   Reset flags/fields. pending_io remains zero until we increment before each issue_* call. */
void request_write(int write_id) {
    request_state_t *state = get_request_state(write_id);
    if (!state) return;

    pthread_mutex_lock(&state_mutex);
    while (state->in_use) {
        pthread_cond_wait(&state_free_cond, &state_mutex);
    }

    state->in_use = 1;
    state->write_id = write_id;
    state->write_data_complete = 0;
    state->journal_txb_complete = 0;
    state->journal_bitmap_complete = 0;
    state->journal_inode_complete = 0;
    state->journal_txe_complete = 0;
    state->write_bitmap_complete = 0;
    state->write_inode_complete = 0;
    state->pending_io = 0;
    state->checkpoint_done = 0;
    pthread_mutex_unlock(&state_mutex);

    circ_buffer_add(&request_buffer, write_id);
}

/* Helper to decrement pending_io and try freeing slot.
   Must be called after completion has been processed (and will lock internally). */
static void decrement_pending_and_maybe_free(request_state_t *state) {
    if (!state) return;
    pthread_mutex_lock(&state_mutex);
    if (state->pending_io > 0) state->pending_io -= 1;
    try_free_slot_if_done_locked(state);
    pthread_mutex_unlock(&state_mutex);
}

/* Completion handlers -- IMPORTANT: they DO NOT call issue_* (no new IO issued here).
   They set flags, post semaphores as needed, decrement pending_io, and attempt to free. */

void write_data_complete(int write_id) {
    request_state_t *state = get_request_state(write_id);
    if (!state) return;

    pthread_mutex_lock(&state_mutex);
    // We allow completions to run even if in_use is 0, so they can drain pending_io.
    state->write_data_complete = 1;
    if (state->write_data_complete &&
        state->journal_txb_complete &&
        state->journal_bitmap_complete &&
        state->journal_inode_complete) {
        sem_post(&state->metadata_wait);
    }
    pthread_mutex_unlock(&state_mutex);

    decrement_pending_and_maybe_free(state);
}

void journal_txb_complete(int write_id) {
    request_state_t *state = get_request_state(write_id);
    if (!state) return;

    pthread_mutex_lock(&state_mutex);
    state->journal_txb_complete = 1;
    if (state->write_data_complete &&
        state->journal_txb_complete &&
        state->journal_bitmap_complete &&
        state->journal_inode_complete) {
        sem_post(&state->metadata_wait);
    }
    pthread_mutex_unlock(&state_mutex);

    decrement_pending_and_maybe_free(state);
}

void journal_bitmap_complete(int write_id) {
    request_state_t *state = get_request_state(write_id);
    if (!state) return;

    pthread_mutex_lock(&state_mutex);
    state->journal_bitmap_complete = 1;
    if (state->write_data_complete &&
        state->journal_txb_complete &&
        state->journal_bitmap_complete &&
        state->journal_inode_complete) {
        sem_post(&state->metadata_wait);
    }
    pthread_mutex_unlock(&state_mutex);

    decrement_pending_and_maybe_free(state);
}

void journal_inode_complete(int write_id) {
    request_state_t *state = get_request_state(write_id);
    if (!state) return;

    pthread_mutex_lock(&state_mutex);
    state->journal_inode_complete = 1;
    if (state->write_data_complete &&
        state->journal_txb_complete &&
        state->journal_bitmap_complete &&
        state->journal_inode_complete) {
        sem_post(&state->metadata_wait);
    }
    pthread_mutex_unlock(&state_mutex);

    decrement_pending_and_maybe_free(state);
}

void journal_txe_complete(int write_id) {
    request_state_t *state = get_request_state(write_id);
    if (!state) return;

    pthread_mutex_lock(&state_mutex);
    state->journal_txe_complete = 1;
    pthread_mutex_unlock(&state_mutex);

    sem_post(&state->commit_wait);

    decrement_pending_and_maybe_free(state);
}

void write_bitmap_complete(int write_id) {
    request_state_t *state = get_request_state(write_id);
    if (!state) return;

    pthread_mutex_lock(&state_mutex);
    state->write_bitmap_complete = 1;
    if (state->write_bitmap_complete && state->write_inode_complete) {
        sem_post(&state->checkpoint_wait);
    }
    pthread_mutex_unlock(&state_mutex);

    decrement_pending_and_maybe_free(state);
}

void write_inode_complete(int write_id) {
    request_state_t *state = get_request_state(write_id);
    if (!state) return;

    pthread_mutex_lock(&state_mutex);
    state->write_inode_complete = 1;
    if (state->write_bitmap_complete && state->write_inode_complete) {
        sem_post(&state->checkpoint_wait);
    }
    pthread_mutex_unlock(&state_mutex);

    decrement_pending_and_maybe_free(state);
}

/* Wait until all request slots are free (useful for tests) */
void journal_wait_for_all(void) {
    pthread_mutex_lock(&state_mutex);
    for (;;) {
        bool any_in_use = false;
        for (int i = 0; i < MAX_REQUESTS; ++i) {
            if (request_states[i].in_use) {
                any_in_use = true;
                break;
            }
        }
        if (!any_in_use) break;
        pthread_cond_wait(&state_free_cond, &state_mutex);
    }
    pthread_mutex_unlock(&state_mutex);
}
