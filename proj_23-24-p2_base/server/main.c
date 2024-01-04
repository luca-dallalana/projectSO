#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "eventlist.h"




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

  int session_id;
  char req_pipe_path[MAX_PIPE_PATH_NAME];
  char resp_pipe_path[MAX_PIPE_PATH_NAME];
};

struct Queue pc_buffer;

int create_queue(struct Queue* queue){
    void** buffer = malloc(MAX_SESSION_COUNT * sizeof(void*));

    if(buffer == NULL){
        printf("Failed to allocate memory for buffer\n");
        return 1;
    }

    queue->queue_buffer = buffer;
    queue->queue_size = 0;

    if(pthread_mutex_init(&queue->size_lock,NULL) != 0 || 
    pthread_mutex_init(&queue->add_to_queue_lock,NULL) != 0 || 
    pthread_mutex_init(&queue->remove_from_queue_lock,NULL) != 0){

        free(queue->queue_buffer);
        return 1;
    }

    if(pthread_cond_init(&queue->add_to_queue_condvar,NULL) != 0 || 
    pthread_cond_init(&queue->remove_from_queue_condvar,NULL) != 0){

        free(queue->queue_buffer);
        return 1;
    }
    return 0;
}

int add_element(struct Queue* queue, void* element){

    pthread_mutex_lock(&queue->add_to_queue_lock);
    pthread_mutex_lock(&queue->size_lock);

    while(queue->queue_size == MAX_SESSION_COUNT){
        pthread_mutex_unlock(&queue->size_lock);
        pthread_cond_wait(&queue->add_to_queue_condvar,&queue->add_to_queue_lock);
        pthread_mutex_lock(&queue->size_lock);
    }



    pthread_mutex_unlock(&queue->add_to_queue_lock);

    // Find an empty spot in the queue_buffer

    for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
        if (queue->queue_buffer[i] == NULL) {
            // Add the element to the found index
            queue->queue_buffer[i] = element;
            break;
        }
    }


    ++queue->queue_size;

    // Signal that an element has been added
    pthread_cond_broadcast(&queue->remove_from_queue_condvar);
    pthread_mutex_unlock(&queue->size_lock);

    return 0;
}

void* remove_element(struct Queue* queue){
    pthread_mutex_lock(&queue->remove_from_queue_lock);
    pthread_mutex_lock(&queue->size_lock);

    while(queue->queue_size == 0){
        pthread_mutex_unlock(&queue->size_lock);
        pthread_cond_wait(&queue->remove_from_queue_condvar,&queue->remove_from_queue_lock);
        pthread_mutex_lock(&queue->size_lock);
    }

    pthread_mutex_unlock(&queue->remove_from_queue_lock);
    void* element;
    for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
        if (queue->queue_buffer[i] != NULL) {
            element = queue->queue_buffer[i];
            queue->queue_buffer[i] = NULL;
            --queue->queue_size;
            break;
            
        }
    }

    pthread_cond_broadcast(&queue->add_to_queue_condvar);
    pthread_mutex_unlock(&queue->size_lock);
    return element;

}

void destroy_queue(struct Queue* queue){

    free(queue -> queue_buffer);

    pthread_mutex_destroy(&queue->size_lock);

    pthread_mutex_destroy(&queue->add_to_queue_lock);
    pthread_cond_destroy(&queue->add_to_queue_condvar);

    pthread_mutex_destroy(&queue->remove_from_queue_lock);
    pthread_cond_destroy(&queue->remove_from_queue_condvar);

}

void* read_session_request(){
  sigset_t set;

  
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);

  if(pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) return (void*)1;

  while(1){
    int resp_pipe, req_pipe;
    struct Session* session = (struct Session*)remove_element(&pc_buffer);

    req_pipe = open(session->req_pipe_path, O_RDONLY);
    resp_pipe = open(session->resp_pipe_path,O_WRONLY);


    if(resp_pipe < 0 || req_pipe < 0){
      return (void*)1;
    }

    char *response_message;

    response_message = malloc(sizeof(int));
    memcpy(response_message,&session->session_id,sizeof(int));

    if(write(resp_pipe,response_message,sizeof(int)) <= 0){
      return (void*)1;
    }

    free(response_message);

    while (1) {
      int op_code;

      if(read(req_pipe,&op_code,sizeof(int)) <= 0) break;

      if(process_request(op_code,req_pipe,resp_pipe)) break;

    }
    close(resp_pipe);
    close(req_pipe);
  }
  return (void*)0;
}




void sigusr1_handler(int sig){

  ems_list_events(STDOUT_FILENO);

}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  unlink(argv[1]);
  //TODO: Intialize server, create worker threads

  if(mkfifo(argv[1],0666) < 0) return 1;

  int register_pipe;
  if((register_pipe = open(argv[1],O_RDONLY)) < 0){
    unlink(argv[1]);
    return 1;
  }



  if(create_queue(&pc_buffer)) return 1;

  pthread_t thread_list[MAX_SESSION_COUNT];

  for(int i = 0; i < MAX_SESSION_COUNT; i++){
    if(pthread_create(&thread_list[i],NULL,read_session_request,NULL) != 0){
      return 1;
    }
  }
  int session_counter = 0;
  
  signal(SIGUSR1, sigusr1_handler);

  while (1) {
    //TODO: Read from pipe
    //TODO: Write new client to the producer-consumer buffer



    int code;
    if(read(register_pipe,&code,sizeof(int)) < 0) break;


    if(code == 0){   

      struct Session* session = malloc(sizeof(struct Session));
      
      read(register_pipe,&session->req_pipe_path,MAX_PIPE_PATH_NAME);
      read(register_pipe,&session->resp_pipe_path,MAX_PIPE_PATH_NAME);

      session -> session_id = session_counter++;

      if(add_element(&pc_buffer,session)){
        free(session);
        break;
      } 

    }

    
  }

  //TODO: Close Server
  destroy_queue(&pc_buffer);
  close(register_pipe);
  unlink(argv[1]);
  ems_terminate();
  return 0;
}