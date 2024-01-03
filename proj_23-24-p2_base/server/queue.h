#include <pthread.h>
#include "common/constants.h"

struct Queue{
    void** queue_buffer;

    pthread_mutex_t size_lock;
    int queue_size;

    pthread_mutex_t add_to_queue_lock;
    pthread_cond_t add_to_queue_condvar;

    pthread_mutex_t remove_from_queue_lock;
    pthread_cond_t remove_from_queue_condvar;

    
};

struct Session {
  int active;
  int session_id;
  char req_pipe_path[MAX_PIPE_PATH_NAME];
  char resp_pipe_path[MAX_PIPE_PATH_NAME];
};

int create_queue(struct Queue* queue);

int add_element(struct Queue* queue, void* element);

void* remove_element(struct Queue* queue);