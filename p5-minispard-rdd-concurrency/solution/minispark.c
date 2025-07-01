#define _POSIX_C_SOURCE 199309L
#define _GNU_SOURCE
#include "minispark.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sched.h>
#include <time.h>
#include <string.h>
#include "keyvalue.h"


ThreadPool* global_thread_pool = NULL;
MetricQueue* global_metrics_queue = NULL;
pthread_t monitor_thread;
volatile int global_shutdown_requested = 0;


// Working with metrics...
// Recording the current time in a `struct timespec`:
//    clock_gettime(CLOCK_MONOTONIC, &metric->created);
// Getting the elapsed time in microseconds between two timespecs:
//    duration = TIME_DIFF_MICROS(metric->created, metric->scheduled);
// Use `print_formatted_metric(...)` to write a metric to the logfile. 
void print_formatted_metric(TaskMetric* metric, FILE* fp) {
  fprintf(fp, "RDD %p Part %d Trans %d -- creation %10jd.%06ld, scheduled %10jd.%06ld, execution (usec) %ld\n",
	  metric->rdd, metric->pnum, metric->rdd->trans,
	  metric->created.tv_sec, metric->created.tv_nsec / 1000,
	  metric->scheduled.tv_sec, metric->scheduled.tv_nsec / 1000,
	  metric->duration);
}

int max(int a, int b)
{
  return a > b ? a : b;
}

RDD *create_rdd(int numdeps, Transform t, void *fn, ...)
{
  RDD *rdd = malloc(sizeof(RDD));
  if (rdd == NULL)
  {
    printf("error mallocing new rdd\n");
    exit(1);
  }

  va_list args;
  va_start(args, fn);

  int maxpartitions = 0;
  for (int i = 0; i < numdeps; i++)
  {
    RDD *dep = va_arg(args, RDD *);
    rdd->dependencies[i] = dep;
    maxpartitions = max(maxpartitions, dep->partitions->size);
  }
  va_end(args);

  rdd->numdependencies = numdeps;
  rdd->trans = t;
  rdd->fn = fn;
  rdd->partitions = NULL;
  rdd->complete = 0;
  rdd->completed_partitions = 0;
  if (pthread_mutex_init(&rdd->rdd_lock, NULL) != 0) {
    exit(1);
  }
  if (pthread_cond_init(&rdd->completed_cv, NULL) != 0) {
    exit(1);
  }
  return rdd;
}

/* RDD constructors */
RDD *map(RDD *dep, Mapper fn)
{
  return create_rdd(1, MAP, fn, dep);
}

RDD *filter(RDD *dep, Filter fn, void *ctx)
{
  RDD *rdd = create_rdd(1, FILTER, fn, dep);
  rdd->ctx = ctx;
  return rdd;
}

RDD *partitionBy(RDD *dep, Partitioner fn, int numpartitions, void *ctx)
{
  RDD *rdd = create_rdd(1, PARTITIONBY, fn, dep);
  rdd->numpartitions = numpartitions;
  rdd->ctx = ctx;
  return rdd;
}

RDD *join(RDD *dep1, RDD *dep2, Joiner fn, void *ctx)
{
  RDD *rdd = create_rdd(2, JOIN, fn, dep1, dep2);
  rdd->ctx = ctx;
  return rdd;
}

/* A special mapper */
void *identity(void *arg)
{
  return arg;
}

/* Special RDD constructor.
 * By convention, this is how we read from input files. */
RDD *RDDFromFiles(char **filenames, int numfiles)
{
  // initialize thread pool if not already done
  if (global_thread_pool == NULL) {
    MS_Run();
  }

  RDD *rdd = malloc(sizeof(RDD));
  if (!rdd) {
    free(rdd);
    exit(1);
  }
  rdd->partitions = list_init();

  for (int i = 0; i < numfiles; i++)
  {
    // printf("%s\n", filenames[i]);
    FILE *fp = fopen(filenames[i], "r");
    if (fp == NULL) {
      perror("fopen");
      exit(1);
    }
    list_add_elem(rdd->partitions, fp);
  }

  rdd->numdependencies = 0;
  rdd->trans = MAP;
  rdd->fn = (void *)identity;
  rdd->numpartitions = numfiles;
  rdd->complete = 0;
  rdd->completed_partitions = 0;
  if (pthread_mutex_init(&rdd->rdd_lock, NULL) != 0) {
    return NULL;
  }
  if (pthread_cond_init(&rdd->completed_cv, NULL) != 0) {
    return NULL;
  }
  return rdd;
}

//////// Worker Queue methods ///////////////

WorkQueue* work_queue_init() {
  WorkQueue* wq = (WorkQueue*) malloc(sizeof(WorkQueue));
  if (wq == NULL) {
    return NULL;
  }
  wq->tasks = list_init();
  if (wq->tasks == NULL) {
    return NULL;
  }
  if (pthread_mutex_init(&wq->lock, NULL) != 0) {
    return NULL;
  }
  if (pthread_cond_init(&wq->available, NULL) != 0) {
    return NULL;
  }

  return wq;
}

int work_queue_enqueue(WorkQueue* wq, Task* task) {
  pthread_mutex_lock(&wq->lock);
  if (list_add_elem(wq->tasks, task) != 0) {
    return -1;
  }
  // signal one waiting worker thread to handle this task
  pthread_cond_signal(&wq->available);
  pthread_mutex_unlock(&wq->lock); 
  return 0;
}

Task* work_queue_dequeue(WorkQueue* wq) {
  pthread_mutex_lock(&wq->lock);
  // checking task for now
  // add shut down logic
  while(list_get_size(wq->tasks) == 0) {
    pthread_cond_wait(&wq->available, &wq->lock);
  }
  Task* task = NULL;
  if (list_get_size(wq->tasks) > 0) {
    task = list_remove_elem(wq->tasks);
  }
  pthread_mutex_unlock(&wq->lock);
  return task;
};

// assuming this is called during thread_pool_destroy so we are assuming that
// the queue is empty, after threads are joined
void work_queue_destroy(WorkQueue* wq) {
  pthread_mutex_destroy(&wq->lock);
  pthread_cond_destroy(&wq->available);
  list_free(wq->tasks);
  free(wq);
}


//////// Metric Queue methods ///////////////

MetricQueue* metric_queue_init() {
  MetricQueue* mq = (MetricQueue*) malloc(sizeof(MetricQueue));
  if (mq == NULL) {
    return NULL;
  }
  mq->metrics = list_init();
  if (mq->metrics == NULL) {
    return NULL;
  }
  if (pthread_mutex_init(&mq->lock, NULL) != 0) {
    return NULL;
  }
  if (pthread_cond_init(&mq->available, NULL) != 0) {
    return NULL;
  }

  return mq;
}

int metric_queue_enqueue(MetricQueue* mq, TaskMetric* metric) {
  pthread_mutex_lock(&mq->lock);
  if (list_add_elem(mq->metrics, metric) != 0) {
    return -1;
  }
  pthread_cond_signal(&mq->available);
  pthread_mutex_unlock(&mq->lock);
  return 0;
}

TaskMetric* metric_queue_dequeue(MetricQueue* mq) {
  pthread_mutex_lock(&mq->lock);
  while(list_get_size(mq->metrics) == 0) {
    pthread_cond_wait(&mq->available, &mq->lock);
  }
  TaskMetric* metric = NULL;
  if (list_get_size(mq->metrics) > 0) {
    metric = list_remove_elem(mq->metrics);
  }
  pthread_mutex_unlock(&mq->lock);
  return metric;
}

void metric_queue_destroy(MetricQueue* mq) {
  pthread_mutex_destroy(&mq->lock);
  pthread_cond_destroy(&mq->available);
  list_free(mq->metrics);
  free(mq);
}

//////// Helper Functions //////////////////
void map_helper(Task* task){
  RDD *rdd = task->rdd;
  int pnum = task->pnum;
  Mapper mapper = (Mapper)rdd->fn;
  RDD *prev_rdd = rdd->dependencies[0];

  List* output_partition = (List*)list_get(rdd->partitions, pnum);
  if (output_partition == NULL) {
    printf("error, output partition %i for RDD %p is null(map output).\n", pnum, rdd);
    goto cleanup;
  }
  void* input_data = list_get(prev_rdd->partitions, pnum);
  if (input_data == NULL) {
    printf("error, input data for RDD %p partition %i is null(map input).\n", prev_rdd, pnum); 
    goto cleanup;
  }

  clock_gettime(CLOCK_MONOTONIC, &task->metric->scheduled);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);

  if (prev_rdd->numdependencies == 0) { // source RDD
    FILE* fp = (FILE*)input_data;
    void* item = NULL;
    // call mapper with FILE*
    while ((item = mapper(fp)) != NULL) {
      if (list_add_elem(output_partition, item) != 0) {
        printf("error adding element to output partition %i RDD %p", pnum, rdd);
        free(item);
        goto cleanup;
      }
    }
  } else {
    // input is a list of items from previous transformation
    List* input_partition = (List*)input_data;
    ListIterator* iter = list_iterator_begin(input_partition);
    if (iter != NULL) {
      while(list_iterator_has_next(iter)){
        void* element = list_iterator_next(iter);
        void* result = mapper(element);
        if (result != NULL) {
          if (list_add_elem(output_partition, result) != 0) {
            printf("error adding mapped element to output partition %i RDD %p\n", pnum, rdd);
            goto cleanup;
          }
        }
      }
      list_iterator_destroy(iter);
    } else {
      printf("error creating iterator\n");
      goto cleanup;
    }
  }

  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  task->metric->duration = TIME_DIFF_MICROS(start, end);

  cleanup: 
    return;
}

void filter_helper(Task* task){
  RDD *rdd = task->rdd;
  if (rdd->numdependencies != 1) {
    printf("incorrect # of dependencies for a Filter function!\n");
    goto cleanup;
  }
  int pnum = task->pnum;
  Filter filter = (Filter)rdd->fn;
  RDD *prev_rdd = rdd->dependencies[0];

  List *output_partition = (List*)list_get(rdd->partitions, pnum);
  if(output_partition == NULL){
    printf("error, output partition %i partition %p is null(filter output).\n", pnum, rdd);
    goto cleanup;
  }
  void *input_data = list_get(prev_rdd->partitions, pnum);
  if(input_data == NULL){
    printf("error, input data for RDD %p partition %i is null(filter input).\n", prev_rdd, pnum);
    goto cleanup;
  }

  clock_gettime(CLOCK_MONOTONIC, &task->metric->scheduled);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);

  // filter will ever deal with FILE* objects, only mapper does
  // so no need to check for case where numdependencies == 0, like in map_helper
  List *input_partition = (List*)input_data;
  ListIterator* iter = list_iterator_begin(input_partition);
  if(iter != NULL){
    while(list_iterator_has_next(iter)){
      void *element = list_iterator_next(iter);
      int result = filter(element, rdd->ctx); // filter returns 0 or 1, if 1, then keep element, if 0, don't keep element
      if (result == 1) {
        if(list_add_elem(output_partition, element) != 0){
          printf("error adding filtered element to output partition %i RDD %p.\n", pnum, rdd);
          goto cleanup;
        }
      }
    }
    list_iterator_destroy(iter);
  }else{
    printf("error creating iterator.\n");
    goto cleanup;
  }

  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  task->metric->duration = TIME_DIFF_MICROS(start,end);

  cleanup:
    return;
}

void join_helper(Task* task){
  RDD *rdd = task->rdd;
  int pnum = task->pnum;
  Joiner joiner = (Joiner)rdd->fn;
  void *ctx = rdd->ctx;
  RDD *prev_rdd1 = rdd->dependencies[0];
  RDD *prev_rdd2 = rdd->dependencies[1];

  List *output_partition = (List*)list_get(rdd->partitions, pnum);
  if(output_partition == NULL){
    printf("error, output partition %i for RDD %p is null(join output).\n", pnum, rdd);
    goto cleanup;
  }

  List *input_data1 = list_get(prev_rdd1->partitions, pnum);
  if(input_data1 == NULL){
    printf("error, output partition %i for RDD %p is null(join input 1).\n", pnum, prev_rdd1);
    goto cleanup;
  }

  List *input_data2 = list_get(prev_rdd2->partitions, pnum);
  if(input_data2 == NULL){
    printf("error, output partition %i for RDD %p is null(join input 2).\n", pnum, prev_rdd2);
    goto cleanup;
  }

  clock_gettime(CLOCK_MONOTONIC, &task->metric->scheduled);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);

  ListIterator *iter1 = list_iterator_begin(input_data1);
  if(iter1 == NULL){
    printf("error creating iter1");
    goto cleanup;
  }

  while(list_iterator_has_next(iter1)){
    void *row1 = (void*) list_iterator_next(iter1);
    if(row1 == NULL){
      continue;
    }

    ListIterator *iter2 = list_iterator_begin(input_data2);
    if(iter2 == NULL){
      printf("error creating iter2");
      goto cleanup;
    }

    while(list_iterator_has_next(iter2)){
      void *row2 = (void*) list_iterator_next(iter2);
      if(row2 == NULL){
        continue;
      }

      void *result;
      result = joiner(row1, row2, ctx);
      if(result != NULL){
        list_add_elem(output_partition, result);
      }
    }
    list_iterator_destroy(iter2);
  }
  list_iterator_destroy(iter1);

  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  task->metric->duration = TIME_DIFF_MICROS(start,end);
  cleanup:
    return;
}

void partition_helper(Task* task) {
  RDD *rdd = task->rdd;
  if (rdd->numdependencies != 1) {
    printf("incorrect # of dependencies for a Partition function!\n");
    goto cleanup;
  }
  int pnum = task->pnum;
  Partitioner partitioner = (Partitioner)rdd->fn;
  void* ctx = rdd->ctx;
  int numpartitions = rdd->numpartitions;
  RDD *prev_rdd = rdd->dependencies[0];

  List *output_partitions_list = rdd->partitions;
  if (output_partitions_list == NULL) {
    printf("error, output partition list %p is null(partition output).\n", rdd); 
    goto cleanup;
  }
  List *input_partition = (List*)list_get(prev_rdd->partitions, pnum);
  if (input_partition == NULL) {
    printf("error, output partition for RDD %p partition %i is null(partition input).\n", prev_rdd, pnum); 
    goto cleanup;
  }

  clock_gettime(CLOCK_MONOTONIC, &task->metric->scheduled);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);

  ListIterator* iter = list_iterator_begin(input_partition);
  if(iter != NULL){
    while(list_iterator_has_next(iter)){
      void *element = list_iterator_next(iter);
      unsigned long target = partitioner(element, numpartitions, ctx);
      if (target >= (unsigned long)numpartitions) {
        printf("error, partitioner returned invalid index %lu for RDD %p\n", target, rdd);
        pthread_mutex_unlock(&rdd->partition_locks[target]);
        continue;
      }
      // lock specific target pnum's mutex
      pthread_mutex_lock(&rdd->rdd_lock);
      List* output_partition = (List*)list_get(output_partitions_list, (int) target);
      pthread_mutex_unlock(&rdd->rdd_lock);
      pthread_mutex_lock(&rdd->partition_locks[target]);
      if (output_partition == NULL) {
        pthread_mutex_unlock(&rdd->partition_locks[target]);
        printf("error, target output partition %lu is NULL for RDD %p\n", target, rdd);
        continue;
      }
      if (list_add_elem(output_partition, element) != 0) {
        pthread_mutex_unlock(&rdd->partition_locks[target]);
        printf("error, failed to add element to target output partition %lu for RDD %p\n", target, rdd);
        goto cleanup;
      }
      pthread_mutex_unlock(&rdd->partition_locks[target]);
    }
    list_iterator_destroy(iter);
  }else{
    printf("error creating iterator.\n");
    goto cleanup;
  }

  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  task->metric->duration = TIME_DIFF_MICROS(start,end);
  
  cleanup:
    return;
}

// void file_backed_helper(Task* task){
//   (void)task;
//   return;
// }

//////// Worker Function ///////////////////

// processes tasks from the work queue until shutdown
// handles task execution and sync between threads
// 1. dequeues a task
// 2. calls appropriate helper function (which performs the actual data processing)
// 3. updates the completion status of the task's RDD
// 4. cleans up the task and metric resources.
// 5. updates thread pool's state.
void *worker_function(void *arg) {
  ThreadPool *tp = (ThreadPool *)arg;
  WorkQueue *wq = tp->wq;
  pthread_t self_id = pthread_self(); // Get worker's own ID for logging
  while (1) {
    int current_shutdown = 0; // shutdown status for worker_function

    pthread_mutex_lock(&wq->lock);
    // sleep til there's tasks available
    // check shutdown status before waiting to ensure no race condition
    while (list_get_size(wq->tasks) == 0) {
      pthread_mutex_lock(&tp->pool_lock);
      current_shutdown = tp->shutdown;
      pthread_mutex_unlock(&tp->pool_lock);

      if (current_shutdown) {
        break;
      }
      pthread_cond_wait(&wq->available, &wq->lock);
    }

    // check if wake up was due to shut down or not
    if (current_shutdown == 1 && list_get_size(wq->tasks) == 0) {
      pthread_mutex_unlock(&wq->lock);
      break; // exit worker loop since shut down
    }

    Task *task = NULL; // being safe rn
    if (list_get_size(wq->tasks) > 0) {
      task = (Task *)list_remove_elem(wq->tasks);
    }
    pthread_mutex_unlock(&wq->lock);
    if (task != NULL)
    {
      // execute task
      switch (task->rdd->trans)
      {
      case MAP:
        map_helper(task);
        break;
      case FILTER:
        filter_helper(task);
        break;
      case JOIN:
        join_helper(task);
        break;
      case PARTITIONBY:
        partition_helper(task);
        break;
      default: 
        printf("unknown worker type encountered %d\n", task->rdd->trans);
      }

      pthread_mutex_lock(&task->rdd->rdd_lock);
      if (!task->rdd->complete) {
        task->rdd->completed_partitions++;
        if (task->rdd->completed_partitions == task->rdd->completion_task_goal) {
          task->rdd->complete = 1;
          pthread_cond_broadcast(&task->rdd->completed_cv);
        } else if (task->rdd->completed_partitions > task->rdd->completion_task_goal) {
        }
      } else {
      }
      pthread_mutex_unlock(&task->rdd->rdd_lock);

      metric_queue_enqueue(global_metrics_queue, task->metric);
      task->metric = NULL;
      free(task);
      task = NULL;
      pthread_mutex_lock(&tp->pool_lock);
      tp->running_tasks--;
      if (tp->shutdown == 0 && tp->running_tasks == 0)
      {
        pthread_cond_signal(&tp->pool_idle_cv);
      }
      pthread_mutex_unlock(&tp->pool_lock);
    } else {
      printf("Worker [%lu]: Dequeued NULL task unexpectedly.\n", self_id);
    }
  }
  return NULL;
}

//////// Monitor Function ///////////////////
// entry point for monitor thread, for metrics
void *monitor_function(void *arg) {
  MetricQueue *mq = (MetricQueue*)arg;
  FILE *fp = fopen("metrics.log", "w");
  if (fp == NULL) {
    return NULL;
  }
  while(1) {
    pthread_mutex_lock(&mq->lock);
    while (list_get_size(mq->metrics) == 0 && global_shutdown_requested == 0) {
      pthread_cond_wait(&mq->available, &mq->lock);
    }
    if (global_shutdown_requested == 1 && list_get_size(mq->metrics) == 0) {
      pthread_mutex_unlock(&mq->lock);
      break;
    }

    TaskMetric *metric = (TaskMetric*) list_remove_elem(mq->metrics);
    pthread_mutex_unlock(&mq->lock);
    if (metric != NULL) {
      print_formatted_metric(metric, fp);
      free(metric);
    } else {
      printf("monitor dequeued NULL metric unexpectedly\n");
    }
    
  }
  fclose(fp);
  return NULL;
}

//////// Thread Pool methods ///////////////

ThreadPool* thread_pool_init(int num_threads){
  ThreadPool *tp = malloc(sizeof(ThreadPool));
  if (tp == NULL) {
    return NULL;
  }
  tp->num_threads = num_threads;
  tp->wq = work_queue_init();
  if (tp->wq == NULL) {
    return NULL;
  }
  tp->threads = malloc(num_threads * sizeof(pthread_t));
  if (tp->threads == NULL) {
    return NULL;
  }
  if (pthread_mutex_init(&tp->pool_lock, NULL) != 0) {
    return NULL;
  }
  if (pthread_cond_init(&tp->pool_idle_cv, NULL) != 0) {
    return NULL;
  }
  tp->shutdown = 0;
  tp->running_tasks = 0;
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&tp->threads[i], NULL, worker_function, tp) != 0) {
      return NULL; // error creating thread
    }
  }
  global_thread_pool = tp;
  return tp;
}


void thread_pool_destroy() {
  ThreadPool* tp = global_thread_pool;
  if (tp == NULL) {
    return;
  }
  pthread_mutex_lock(&tp->pool_lock);
  if (tp->shutdown) { // thread pool already in a shutdown state, no need to do anything else
    pthread_mutex_unlock(&tp->pool_lock);
    return;
  }
  tp->shutdown = 1;
  pthread_mutex_unlock(&tp->pool_lock);
  // wake all workers still waiting thru broadcast, make em shut down
  pthread_mutex_lock(&tp->wq->lock);
  pthread_cond_broadcast(&tp->wq->available);
  pthread_mutex_unlock(&tp->wq->lock);
  for (int i = 0; i < tp->num_threads; i++) {
    int ret = pthread_join(tp->threads[i], NULL);
    if (ret != 0) {
      fprintf(stderr, "Error joining worker thread %d: %s\n", i, strerror(ret));
    } else {
    }
 if (ret != 0) {
        fprintf(stderr, "Error joining worker thread %d: %s\n", i, strerror(ret));
    } else {
    }  }
  // cleanup remaining tasks in the queue first
  pthread_mutex_lock(&tp->wq->lock);
  while(list_get_size(tp->wq->tasks) > 0) {
    Task* task = (Task*)list_remove_elem(tp->wq->tasks);
    if (task != NULL) {
      if (task->metric != NULL) {
        free(task->metric);
      }
    }
    free(task);
  }
  pthread_mutex_unlock(&tp->wq->lock);
  // rest of cleanup
  free(tp->threads);
  pthread_mutex_destroy(&tp->pool_lock);
  pthread_cond_destroy(&tp->pool_idle_cv);
  work_queue_destroy(tp->wq);
  free(tp);
  global_thread_pool = NULL;
}

void thread_pool_wait() {
  ThreadPool* tp = global_thread_pool;
  if (tp == NULL) {
    return;
  }
  pthread_mutex_lock(&tp->pool_lock);
  while (tp->running_tasks > 0) { // wait while there's running tasks or queued
    pthread_cond_wait(&tp->pool_idle_cv, &tp->pool_lock);
  }
  pthread_mutex_unlock(&tp->pool_lock);
}

int thread_pool_submit(Task* task) {
  ThreadPool* tp = global_thread_pool;
  if (tp == NULL) {
    return -1;
  }
  pthread_mutex_lock(&tp->pool_lock);
  tp->running_tasks++;
  pthread_mutex_unlock(&tp->pool_lock);
  if (work_queue_enqueue(tp->wq, task) != 0) {
    return -1;
  }; // add given task to worker queue!
  return 0;
}


void execute(RDD *rdd) {
  if(rdd == NULL || rdd->complete){
    return;
  }
  // parent RDDs
  if (rdd->numdependencies == 0) {
    pthread_mutex_lock(&rdd->rdd_lock);
    if(!rdd->complete) {
      rdd->complete = 1;
      pthread_cond_broadcast(&rdd->completed_cv);
    }
    pthread_mutex_unlock(&rdd->rdd_lock);
    return;
  }
  // execute dependencies
  for (int i = 0; i < rdd->numdependencies; i++){
    execute(rdd->dependencies[i]);
  }
  // wait for dependency completion
  for (int i = 0; i < rdd->numdependencies; i++) {
    RDD* dep = rdd->dependencies[i];
    pthread_mutex_lock(&dep->rdd_lock);
    while (dep->complete == 0) {
      pthread_cond_wait(&dep->completed_cv, &dep->rdd_lock);
    }
    pthread_mutex_unlock(&dep->rdd_lock);
  }
  // dependencies should now be complete
  // set numpartitions
  pthread_mutex_lock(&rdd->rdd_lock);
  if (rdd->numpartitions == 0) {
    if (rdd->trans == MAP || rdd->trans == FILTER || rdd->trans == JOIN) {
      rdd->numpartitions = rdd->dependencies[0]->numpartitions;
    }
  }
  if (rdd->numpartitions <= 0) {
    printf("error, rdd %p has invalid number of partitions (%i) before init.\n", rdd, rdd->numpartitions);
    pthread_mutex_unlock(&rdd->rdd_lock);
    return;
}
  if(rdd->partitions == NULL) {
    rdd->partitions = list_init();
    if (rdd->partitions == NULL) { // list init failure
      pthread_mutex_unlock(&rdd->rdd_lock);
      printf("fatal error, failed to initialize partitions list for RDD %p\n", rdd);
      return;
    }
    int init_failed = 0;
    int i;
    for (i = 0; i < rdd->numpartitions; i++) {
      List* inner_list = list_init();
      if (inner_list == NULL) {
        printf("error, list_init failed for inner partition %i for rdd %p\n", i, rdd);
        init_failed = 1;
        break;
      }
      if (list_add_elem(rdd->partitions, inner_list) != 0) {
        printf("error, list_add_elem failed for inner partition %i for rdd %p\n", i, rdd);
        list_free(inner_list);
        init_failed = 1;
        break;
      }
    }
    
    if (init_failed) {
      printf("cleaning up partially initialized partition lists for rdd %p due to failure at index %i\n", rdd, i);
      for (int j = 0; j < i; j++) {
        List* error_list = (List*)list_get(rdd->partitions, j);
        if (error_list != NULL) { // Should generally not be NULL if added
          list_free(error_list);
        } else {
          printf("warning, Found null inner list during cleanup at index %i for RDD %p\n", j, rdd);
        }
      }
      list_free(rdd->partitions);
      rdd->partitions = NULL;
      pthread_mutex_unlock(&rdd->rdd_lock);
      return;
    }
  }
  // PARTITIONBY lock initliazation
  if (rdd->trans == PARTITIONBY && rdd->partition_locks == NULL) {
    rdd->partition_locks = malloc(rdd->numpartitions * sizeof(pthread_mutex_t));
    if (rdd->partition_locks == NULL) {
      printf("error creating list of partition locks for RDD %p\n", rdd);
      pthread_mutex_unlock(&rdd->rdd_lock);
      return;
    }
    int init_failed = 0; // needed for cleanup
    int i;
    for (i = 0; i < rdd->numpartitions; i++) {
      if (pthread_mutex_init(&rdd->partition_locks[i], NULL) != 0) {
        printf("error creating partition lock %i for RDD %p\n", i, rdd);
        init_failed = 1;
        break;
      };
    }

    if (init_failed) {
      printf("cleaning up partially initialized partition locks for RDD %p due to failure at index %i\n", rdd, i);
      for (int j = 0; j < i; j++) {
        if (pthread_mutex_destroy(&rdd->partition_locks[j]) != 0) {
          printf("warning, failed to destroy partition lock %i during cleanup for RDD %p\n", j, rdd);
        }
      }
      free(rdd->partition_locks);
      rdd->partition_locks = NULL; // need to try again
      pthread_mutex_unlock(&rdd->rdd_lock);
      return;
    }
  }


  bool already_complete = (rdd->complete == 1);
  pthread_mutex_unlock(&rdd->rdd_lock);

  if (already_complete) {
    return;
  }

  if(global_thread_pool == NULL){
    printf("error, thread pool not initalized before submitting task\n");
    return;
  }

  int num_tasks_to_submit = 0;
  int task_goal_for_completion = 0;
  RDD* source_rdd = NULL;

  if (rdd->trans == MAP || rdd->trans == FILTER || rdd->trans == PARTITIONBY) {
    if (rdd->numdependencies != 1) {
      printf("error, incorrect dependency count (%i) for RDD %p transform %i\n", rdd->numdependencies, rdd, rdd->trans);
      return;
    }
    source_rdd = rdd->dependencies[0];
    num_tasks_to_submit = source_rdd->numpartitions;
    task_goal_for_completion = num_tasks_to_submit;
  } else if (rdd->trans == JOIN) {
    if (rdd->numdependencies != 2) {
      printf("incorrect dependency count (%i) for JOIN rdd %p\n", rdd->numdependencies, rdd);
      return;
    }
    num_tasks_to_submit = rdd->numpartitions;
    task_goal_for_completion = num_tasks_to_submit;
  } else {
    printf("error, unexpected transform type %i in execute task submission\n", rdd->trans);
    return;
  }

  if (num_tasks_to_submit <= 0) {
    printf("error, RDD %p calculated %i tasks to submit. Check if dependencies are initialized", rdd, num_tasks_to_submit);
    return;
  }

  pthread_mutex_lock(&rdd->rdd_lock);
  rdd->completion_task_goal = task_goal_for_completion; // store the goal count
  pthread_mutex_unlock(&rdd->rdd_lock);


  for(int i = 0; i < num_tasks_to_submit; i++){//create task and task metric for each partition
    Task *task = malloc(sizeof(Task));
    if (!task) {
      printf("task malloc error");
      continue;
    }
    task->rdd = rdd;
    task->pnum = i;
    task->metric = malloc(sizeof(TaskMetric));
    if (!task->metric) {
      free(task);
      printf("task metric malloc error");
      continue;
    }
    clock_gettime(CLOCK_MONOTONIC, &task->metric->created);
    task->metric->pnum = i;
    task->metric->rdd = rdd;

    if (thread_pool_submit(task) != 0) {
      printf("failed to submit task for RDD %p, partition %i\n", rdd, i);
      free(task->metric);
      free(task);
    }
  }
}

void MS_Run() {
  cpu_set_t set;
  CPU_ZERO(&set);

  if(sched_getaffinity(0, sizeof(set), &set) == -1){
    perror("sched_getaffinity");
    exit(1);
  }

  int num_threads = CPU_COUNT(&set);

  global_thread_pool = thread_pool_init(num_threads);
  if (global_thread_pool == NULL) {
    printf("Failed to initialize thread pool\n");
    exit(1);
  }

  global_metrics_queue = metric_queue_init();
  if (global_metrics_queue == NULL) {
    printf("failed to initialize metrics queue\n");
    exit(1);
  }

  // monitor thread is a global var
  if (pthread_create(&monitor_thread, NULL, monitor_function, global_metrics_queue) != 0) {
    printf("failed to create monitor thread\n");
    metric_queue_destroy(global_metrics_queue);
    global_metrics_queue = NULL;
    return;
  }
  
  return;
}

void MS_TearDown() {
  if (global_thread_pool != NULL) {
    thread_pool_destroy();
  }


  if (global_metrics_queue != NULL) {
    pthread_mutex_lock(&global_metrics_queue->lock);
    global_shutdown_requested = 1; // to avoid race condition with monitor function
    pthread_cond_signal(&global_metrics_queue->available);
    pthread_mutex_unlock(&global_metrics_queue->lock);

    int ret = pthread_join(monitor_thread, NULL);
    if (ret != 0) {
      fprintf(stderr, "Error joining monitor thread: %s\n", strerror(ret));
    }
    else {
    }
    metric_queue_destroy(global_metrics_queue);
    global_metrics_queue = NULL;
  }
    
  return;
}

int count(RDD *rdd) {
  execute(rdd);
  thread_pool_wait(); // need to wait for rdd + dependencies to fully materialize

  int total_count = 0;
  if (rdd->partitions != NULL) {
    ListIterator* curr = list_iterator_begin(rdd->partitions);
    if (curr != NULL) {
      while (list_iterator_has_next(curr) != 0) {
        List* current_list = (List*)list_iterator_next(curr);
        if (current_list != NULL) {
          total_count += list_get_size(current_list);
        }
      }
      list_iterator_destroy(curr);
      
    }
  }
  return total_count;
}

void print(RDD *rdd, Printer p) {
  execute(rdd);
  thread_pool_wait();
  // print all the items in rdd
  // aka... `p(item)` for all items in rdd
  if (rdd->partitions != NULL) {
    ListIterator* curr = list_iterator_begin(rdd->partitions);
    if (curr != NULL) {
      while (list_iterator_has_next(curr) != 0) {
        List* current_list = (List*)list_iterator_next(curr);
        if (current_list != NULL) {
          ListIterator* inner_iter = list_iterator_begin(current_list);
          if (inner_iter != NULL) {
            while (list_iterator_has_next(inner_iter) != 0) {
              void* e = list_iterator_next(inner_iter);
              p(e); // use the given Printer function to print
            }
            list_iterator_destroy(inner_iter); 
          }
        }
      }
      list_iterator_destroy(curr);
    }
  }

}
