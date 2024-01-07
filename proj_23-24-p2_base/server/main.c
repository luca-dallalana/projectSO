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



// Struct that represents producer/consumer queue
struct Queue{

    // Lock to change the queue
    pthread_mutex_t buffer_lock;
    void** queue_buffer;

    pthread_mutex_t size_lock;
    int queue_size;

    // To add to the queue
    pthread_mutex_t add_to_queue_lock;
    pthread_cond_t add_to_queue_condvar;

    // To remove to the queue
    pthread_mutex_t remove_from_queue_lock;
    pthread_cond_t remove_from_queue_condvar;

    
};

// Struct that represents a client session
struct Session {
  // session id and pipe paths used to communicate with server 
  int session_id;
  char req_pipe_path[MAX_PIPE_PATH_NAME];
  char resp_pipe_path[MAX_PIPE_PATH_NAME];
};

struct Queue pc_buffer;

// Function that creates a queue of client requests
int create_queue(struct Queue* queue){
    // Allocates memory for the actual queue of clients
    void** buffer = malloc(MAX_SESSION_COUNT * sizeof(void*));

    if(buffer == NULL){
        printf("Failed to allocate memory for buffer\n");
        return 1;
    }
   
    queue->queue_buffer = buffer;
    queue->queue_size = 0;

    // Initializes the locks
    if(pthread_mutex_init(&queue->buffer_lock,NULL) != 0 || pthread_mutex_init(&queue->size_lock,NULL) != 0 || 
    pthread_mutex_init(&queue->add_to_queue_lock,NULL) != 0 || 
    pthread_mutex_init(&queue->remove_from_queue_lock,NULL) != 0){

        free(queue->queue_buffer);
        return 1;
    }

    // Initializes the condition variables
    if(pthread_cond_init(&queue->add_to_queue_condvar,NULL) != 0 || 
    pthread_cond_init(&queue->remove_from_queue_condvar,NULL) != 0){

        free(queue->queue_buffer);
        return 1;
    }
    return 0;
}

// Function that adds a client to the queue
int add_element(struct Queue* queue, void* element){

    // Locks adding condition and the number of clients in queue
    pthread_mutex_lock(&queue->add_to_queue_lock);
    pthread_mutex_lock(&queue->size_lock);

    // While the queue is full
    while(queue->queue_size == MAX_SESSION_COUNT){
        pthread_mutex_unlock(&queue->size_lock);
        pthread_cond_wait(&queue->add_to_queue_condvar,&queue->add_to_queue_lock);
        pthread_mutex_lock(&queue->size_lock);
    }

    // Locks the queue buffer
    pthread_mutex_lock(&queue->buffer_lock);
    // Unlocks the adding condition so other threads can try to add elements
    pthread_mutex_unlock(&queue->add_to_queue_lock);

    // Find an empty spot in the queue

    for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
        if (queue->queue_buffer[i] == NULL) {
            // Add the element to the found index
            queue->queue_buffer[i] = element;
            ++queue->queue_size;
            break;
        }
    }



    // Signal to all threads that a client has been added
    pthread_cond_broadcast(&queue->remove_from_queue_condvar);
    pthread_mutex_unlock(&queue->size_lock);
    pthread_mutex_unlock(&queue->buffer_lock);

    return 0;
}

// Function to remove a client from the list
void* remove_element(struct Queue* queue){

    // Locks removing condition and the number of clients in queue
    pthread_mutex_lock(&queue->remove_from_queue_lock);
    pthread_mutex_lock(&queue->size_lock);

    // While its empty
    while(queue->queue_size == 0){
        pthread_mutex_unlock(&queue->size_lock);
        pthread_cond_wait(&queue->remove_from_queue_condvar,&queue->remove_from_queue_lock);
        pthread_mutex_lock(&queue->size_lock);
    }

    // Locks the queue buffer 
    pthread_mutex_lock(&queue->buffer_lock);

    // Unlocks the removing condition so other threads can try to remove elements
    pthread_mutex_unlock(&queue->remove_from_queue_lock);
    void* element;
    // Searches for a client request in queue
    for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
        if (queue->queue_buffer[i] != NULL) {
            element = queue->queue_buffer[i];
            // Removes client from queue
            queue->queue_buffer[i] = NULL;
            --queue->queue_size;
            break;
            
        }
    }
    // Signal to all threads that a client has been removed from queue
    pthread_cond_broadcast(&queue->add_to_queue_condvar);
    pthread_mutex_unlock(&queue->size_lock);
    pthread_mutex_unlock(&queue->buffer_lock);

    // Returns the client
    return element;

}
// Function that destroys the queue
void destroy_queue(struct Queue* queue){
    pthread_mutex_destroy(&queue->buffer_lock);
    free(queue -> queue_buffer);

    pthread_mutex_destroy(&queue->size_lock);

    pthread_mutex_destroy(&queue->add_to_queue_lock);
    pthread_cond_destroy(&queue->add_to_queue_condvar);

    pthread_mutex_destroy(&queue->remove_from_queue_lock);
    pthread_cond_destroy(&queue->remove_from_queue_condvar);

}

// Function that retrieves a client from the queue and processes its request
void* read_session_request(){
  sigset_t set;

  
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  // Threads that are not the main ignore this signal
  if(pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) return (void*)1;

  while(1){
    int resp_pipe, req_pipe;
    struct Session* session = (struct Session*)remove_element(&pc_buffer);

    // Opens the request and response pipes
    req_pipe = open(session->req_pipe_path, O_RDONLY);
    resp_pipe = open(session->resp_pipe_path,O_WRONLY);


    if(resp_pipe < 0 || req_pipe < 0){
      return (void*)1;
    }

    char *response_message;


    response_message = malloc(sizeof(int));
    memcpy(response_message,&session->session_id,sizeof(int));

    // Sends the session id to the client
    if(write(resp_pipe,response_message,sizeof(int)) < 0){
      close(resp_pipe);
      close(req_pipe);
      unlink(session->req_pipe_path);
      unlink(session->resp_pipe_path);
      return (void*)1;
    }

    free(response_message);


    while (1) {
      char op_code[OP_CODE_LEN];

      // Reads the op code sent from the client
      if(read(req_pipe,&op_code,OP_CODE_LEN) <= 0) break;

      int code = get_code(op_code);
      // process the request from the client
      if(process_request(code,req_pipe,resp_pipe)){
        close(resp_pipe);
        close(req_pipe);
        unlink(session->req_pipe_path);
        unlink(session->resp_pipe_path);
        return (void*)1;
      } 

    }
    // destroy pipes used to communicate with the client
    unlink(session->req_pipe_path);
    unlink(session->resp_pipe_path);

  }
  return (void*)0;
}



// Function that handles the received signal
void sigusr1_handler(int signal){
  ems_list_events(STDOUT_FILENO);

}


int main(int argc, char* argv[]) {
  // Checks for insuficient arguments
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

  // creates the server pipe
  if(mkfifo(argv[1],0666) < 0) return 1;

  // creates the producer/consumer buffer
  if(create_queue(&pc_buffer)) return 1;


  pthread_t thread_list[MAX_SESSION_COUNT];

  // changes the handling for the SIGUSR1 signal
  signal(SIGUSR1, sigusr1_handler);

  // Creates all worker threads
  for(int i = 0; i < MAX_SESSION_COUNT; i++){
    if(pthread_create(&thread_list[i],NULL,read_session_request,NULL) != 0){
      destroy_queue(&pc_buffer);
      ems_terminate();
      unlink(argv[1]);
      return 1;
    }
  }
  // starts the session counter to give clients their session id numbers
  int session_counter = 0;
  

  while (1) {

    int register_pipe;

    // Opens the server pipe 
    if((register_pipe = open(argv[1],O_RDONLY)) < 0){
      break;
    }

    char op_code[OP_CODE_LEN];

    // Reads the op code sent by the client
    if(read(register_pipe,&op_code,OP_CODE_LEN) < 0) break;

    int code = get_code(op_code);
   
    // If it's the session request code, begin a session
    if(code == 1){   
  
      struct Session* session = malloc(sizeof(struct Session));

      if(session == NULL){
        printf("Failed to allocate memory for session\n");
        break;
      }
      
      // register the request and response pipe names
      read(register_pipe,&session->req_pipe_path,MAX_PIPE_PATH_NAME);
      read(register_pipe,&session->resp_pipe_path,MAX_PIPE_PATH_NAME);

      session -> session_id = session_counter++;

      // add this client to the buffer
      add_element(&pc_buffer,session);
        
    }
   
    close(register_pipe);

  }

  //TODO: Close Server
  destroy_queue(&pc_buffer);
  ems_terminate();
  unlink(argv[1]);
  return 0;
}