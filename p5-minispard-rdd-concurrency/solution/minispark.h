#ifndef __minispark_h__
#define __minispark_h__

#include <pthread.h>
#include "list.h"

#define MAXDEPS (2)
#define TIME_DIFF_MICROS(start, end) \
  (((end.tv_sec - start.tv_sec) * 1000000L) + ((end.tv_nsec - start.tv_nsec) / 1000L))

struct RDD;
struct List;

typedef struct RDD RDD; // fo`rward decl. of struct RDD
// typedef struct List List;  // forward decl. of List.
// Minimally, we assume "list_add_elem(List *l, void*)"

// Different function pointer types used by minispark
typedef void* (*Mapper)(void* arg);
typedef int (*Filter)(void* arg, void* pred);
typedef void* (*Joiner)(void* arg1, void* arg2, void* arg);
typedef unsigned long (*Partitioner)(void *arg, int numpartitions, void* ctx);
typedef void (*Printer)(void* arg);

typedef enum {
  MAP,
  FILTER,
  JOIN,
  PARTITIONBY,
  FILE_BACKED
} Transform;

struct RDD {    
  Transform trans; // transform type, see enum
  void* fn; // transformation function
  void* ctx; // used by minispark lib functions
  List* partitions; // list of partitions
  
  RDD* dependencies[MAXDEPS];
  int numdependencies; // 0, 1, or 2
  int numpartitions;  // # of partitions in this RDD

  // you may want extra data members here
  int complete; // set to 0 initially, 1 = RDD has been materialized
  // when a partition in the RDD is completed, it adds 1 to the count of completed_partitions
  // thus when completed_partitions == numpartitions, set complete flag to 1, thus RDD is complete!
  volatile int completed_partitions; // counter for materialized partitions, init to 0
  pthread_mutex_t rdd_lock;
  pthread_mutex_t* partition_locks; // array of mutexes for output partitions for partition_helper() only
  // signaled when cv becomes 1, only the LAST partition to finish (the one that sets completed flag)
  // thus, wakes up the dependent RDD and it can now continue.
  pthread_cond_t completed_cv; 
  int completion_task_goal; // num of tasks that need to be completed for this RDD stage (test 19)
 };

typedef struct {
  struct timespec created;
  struct timespec scheduled;
  size_t duration; // in usec
  RDD* rdd;
  int pnum;
} TaskMetric;

typedef struct {
  List* metrics;
  pthread_mutex_t lock;
  pthread_cond_t available;
} MetricQueue;

typedef struct {
  RDD* rdd;
  int pnum;
  TaskMetric* metric;
} Task;

// CHANGE BELOW AS NEEDED
typedef struct {
  List* tasks;  // list of Task* pointers
  pthread_mutex_t lock;
  pthread_cond_t available;
  // pthread_cond_t completed; not used right now.
} WorkQueue;

typedef struct {
  pthread_t* threads; // array storing [numthreads] thread IDs
  int num_threads; // # of worker threads
  WorkQueue* wq; // pointer to shared work queue 
  int shutdown; // flag to signal threads to exit, 0 = running, 1 = shutting down

  // count of tasks currently being processed by workers plus tasks still in the queue   
  int running_tasks; // thread_pool_submit increments, workers decrement after finishing a task.

  pthread_mutex_t pool_lock; // meant to protect shutdown and running_tasks
  pthread_cond_t  pool_idle_cv; // signaled when potentially idle

} ThreadPool;



// ThreadPool* global_thread_pool = NULL;

//////// actions ////////

// Return the total number of elements in "dataset"
int count(RDD* dataset);

// Print each element in "dataset" using "p".
// For example, p(element) for all elements.
void print(RDD* dataset, Printer p);

//////// transformations ////////

// Create an RDD with "rdd" as its dependency and "fn"
// as its transformation.
RDD* map(RDD* rdd, Mapper fn);

// Create an RDD with "rdd" as its dependency and "fn"
// as its transformation. "ctx" should be passed to "fn"
// when it is called as a Filter
RDD* filter(RDD* rdd, Filter fn, void* ctx);

// Create an RDD with two dependencies, "rdd1" and "rdd2"
// "ctx" should be passed to "fn" when it is called as a
// Joiner.
RDD* join(RDD* rdd1, RDD* rdd2, Joiner fn, void* ctx);

// Create an RDD with "rdd" as a dependency. The new RDD
// will have "numpartitions" number of partitions, which
// may be different than its dependency. "ctx" should be
// passed to "fn" when it is called as a Partitioner.
RDD* partitionBy(RDD* rdd, Partitioner fn, int numpartitions, void* ctx);

// Create an RDD which opens a list of files, one per
// partition. The number of partitions in the RDD will be
// equivalent to "numfiles."
RDD* RDDFromFiles(char* filenames[], int numfiles);

//////// MiniSpark ////////
// Submits work to the thread pool to materialize "rdd".
void execute(RDD* rdd);

// Creates the thread pool and monitoring thread.
void MS_Run();

// Waits for work to be complete, destroys the thread pool, and frees
// all RDDs allocated during runtime.
void MS_TearDown();




//////// Work Queue methods   ////////////

// create the work queue
WorkQueue* work_queue_init();

// get rid of all the worker queues and free the memory
void work_queue_destroy(WorkQueue* wq);

// add task to the start of work queue
// returns 0 on success
int work_queue_enqueue(WorkQueue* wq, Task* task);

// remove task from the end of work queue and return task
// must handle waiting if empty
Task* work_queue_dequeue(WorkQueue* wq);

//////// Metric Queue methods   ////////////

// create the metric queue
MetricQueue* metric_queue_init();

// get rid of all the metrics in queue and free the memory
void metric_queue_destroy(MetricQueue* mq);

// add metric to the start of metric queue
// returns 0 on success
int metric_queue_enqueue(MetricQueue* mq, TaskMetric* metric);

// remove metric from the end of metric queue and return TaskMetric
// must handle waiting if empty
TaskMetric* metric_queue_dequeue(MetricQueue* mq);

//////// Worker Function ///////////////////
void *worker_function(void *arg);

//////// Monitor Function ///////////////////
void *monitor_function(void *arg);

//////// Thread Pool methods   ////////////

// create the pool with
// numthreads threads. Do any necessary allocations.
ThreadPool* thread_pool_init(int numthreads);

// join all the threads and deallocate any
// memory used by the pool.
void thread_pool_destroy();

// returns when the work queue is empty and all
// threads have finished their tasks. You can use it to wait until all
// the tasks in the queue are finished. For example, you would not want
// to count before RDD is fully materialized.
void thread_pool_wait();

// adds a task to the work queue.
// returns 0 on success
int thread_pool_submit(Task* task);

#endif // __minispark_h__
