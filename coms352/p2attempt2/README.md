COM S 352 Project 2
Name: Peter Collins
NetID: psc3245

### Overview:
- We were tasked with implementing the journaling layer for a file system
- This layer was required to be a concurrent application with three threads, each representing distict stages of the process
- Concurrency was implemented with upmost efficiency and thread safety

### Implementation

#### Circular Buffer
- The most important part of my solution to the problem is the circular buffer data structure
- This structure utilizes three sempahores to ensure thread safety
- It is a classic example of the producer consumer problem. Thread safety hinges on the ability to ensure that threads wait for the producer before them to produce an item to work on before the consumer tries to access it.
- This implementation ensures safety between threads and prevents race conditions as well as uses circular indexing to ensure the correct item is returned upon removal and items added go in the correct spot

#### Threads and Worker Functions
- Three threads are utilized in conjunction with three circular buffers to create the pipeline
- The threads represent the consumer half of the producer consumer problem
- The threads each execute a worker function that uses an infinite loop to ask a circular buffer for a item to work on
- Upon recieving the write_id, the worker calls the necessary function, waits for sem_post from each, and sends the write_id to the next stage
- In total, three threads, three buffers, and six total semaphores were used to achieve safe and efficient concurrency in this application

### Testing
- We were also tasked with implmenting a test case, one where the very first transaction ended commit was delayed
- This would cause the first buffer to fill up, triggering a debugging related message that the buffer was full to be displayed
- The behavior was as expected: process the first requests quickly until the first transaction (write_id 0) was finished waiting for the delay
- The message "thread stuck because of full buffer" would be printed during the delay, as no more requests could be added to the first buffer
- Once the thread finishes its artificial delay, the requests will be processed sequentially as expected, moving through each stage of the pipeline

#### Actual log output:
- The output matches expected behavior:

requesting test write 0
...
requesting test write 9
thread stuck because of full buffer
issue write data 0
...
issue journal bitmap 9
issue journal inode 9
thread stuck because of full buffer
requesting test write 10
...
issue journal txe 15
issue write bitmap 15
issue write inode 15
test write complete 15

- Each time a buffer fills because a large number of transactions are taking place, the message is printed
- The pipeline completes each request in order and finishes all 16 requests


### Known Issues
- There are no known issues at this time