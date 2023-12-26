#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "operations.c"



struct Session {
  int active;
  int session_id;
  char req_pipe_path[40];
  char resp_pipe_path[40];
};

struct Session session;

int session_request(int pipe){

    if(read(pipe,&session.req_pipe_path,MAX_PIPE_PATH_NAME) < 0 || read(pipe,&session.resp_pipe_path,MAX_PIPE_PATH_NAME) < 0){
      return 1;
    }
    session.session_id = 1;
    session.active = 1;

    return 0;
  

}


int process_request(enum Command* code, int request_pipe, int response_pipe){
  if(code == 0){
    unsigned int event_id;
    size_t num_rows;
    size_t num_cols;

    if(read(request_pipe,&event_id,sizeof(unsigned int)) < 0 || 
    read(request_pipe,&num_rows,sizeof(size_t)) < 0 || 
    read(request_pipe,&num_cols,sizeof(size_t)) < 0) return 1;


    int data = ems_create(event_id,num_rows,num_cols);
    if(write(response_pipe,&data,sizeof(int)) < 0) return 1;

    return 0;
  }
  else if(code == 1){
    unsigned int event_id;
    size_t num_seats;
    size_t* xs;
    size_t* ys;

    if(read(request_pipe,&event_id,sizeof(unsigned int)) < 0 || 
    read(request_pipe,&num_seats,sizeof(size_t)) < 0 || 
    read(request_pipe,&xs,sizeof(size_t*)) < 0 || read(request_pipe,&ys,sizeof(size_t*)) < 0)  return 1;

    int data = ems_reserve(event_id,num_seats,xs,ys);
    
    if(write(response_pipe,&data,sizeof(int)) < 0) return 1;

    return 0;

  }
  else if(code == 2){
    int out_fd;
    unsigned int event_id;

    if(read(request_pipe,&out_fd,sizeof(int)) < 0 || read(request_pipe,&event_id,sizeof(unsigned int)) < 0) return 1;

    int show_value = ems_show(out_fd,event_id);

    struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

    size_t rows = event->rows;
    size_t cols = event->cols;
    size_t event_size = rows * cols;
    
    char response_message[sizeof(int) + sizeof(size_t) + sizeof(size_t) + event_size];

    memset(response_message,'\0', sizeof(response_message));

    memcpy(response_message,show_value,sizeof(int));
    memcpy(response_message + sizeof(int),rows,sizeof(size_t));
    memcpy(response_message + sizeof(int) + sizeof(size_t),cols,sizeof(size_t));
    memcpy(response_message + sizeof(int) + sizeof(size_t) + sizeof(size_t),event->data,event_size);

    if(write(response_pipe,response_message,sizeof(response_message)) < 0) return 1;

    return 0;
  }

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

  if(mkfifo(argv[1],0777) < 0){
    return 1;
  }



  int register_pipe;
  if(register_pipe = open(argv[1],O_RDONLY) < 0){
    return 1;
  }

  while (1) {
    //TODO: Read from pipe
    //TODO: Write new client to the producer-consumer buffer
    enum Command* code;
    int resp_pipe, req_pipe;
    int bytesread;
    
    bytesread = read(register_pipe,&code,1);

    if(bytesread = read(register_pipe,&code,1)) break;

    if(code == 9){   
      if(session_request(register_pipe)){
        fprintf(stderr,"Failed session request\n");
        return 1;
      }
      resp_pipe = open(session.resp_pipe_path,O_WRONLY);
      req_pipe = open(session.req_pipe_path, O_RDONLY);

      if(resp_pipe < 0 || req_pipe < 0){
        return 1;
      }
      write(resp_pipe,session.session_id,sizeof(int));
    }

    if(bytesread = read(session.req_pipe_path,&code,sizeof(code)) <= 0) break;

    

    


  }

  //TODO: Close Server
  close(register_pipe);
  ems_terminate();
}