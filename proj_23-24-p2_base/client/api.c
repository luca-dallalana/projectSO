#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h>


#include "api.h"
#include "parser.h"
#include "common/constants.h"

int cur_session_id;
int req_pipe;
int resp_pipe;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {

  //TODO: create pipes and connect to the server
  printf("%s\n",server_pipe_path);
  unlink(req_pipe_path);
  unlink(resp_pipe_path);

  int server_pipe;
  if((server_pipe = open(server_pipe_path,O_WRONLY))< 0){
    
    printf("%s\n",server_pipe_path);
    printf("foi o server\n");
    unlink(server_pipe_path);
    return 1;
  }

  int op_code = 0;
  char request_message[MAX_REQUEST_MESSAGE];

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,&op_code,sizeof(int));
  memcpy(request_message + 1, req_pipe_path,MAX_PIPE_PATH_NAME);
  memcpy(request_message + 1 + MAX_PIPE_PATH_NAME, resp_pipe_path, MAX_PIPE_PATH_NAME);

  // requests session
  if(write(server_pipe,request_message,MAX_REQUEST_MESSAGE) < 0){
    printf("foi o write message\n");
    return 1;


  } 

  if(mkfifo(req_pipe_path,0666) < 0 || mkfifo(resp_pipe_path,0666) < 0){
    return 1;
  }

  req_pipe = open(req_pipe_path, O_WRONLY);
  resp_pipe = open(resp_pipe_path, O_RDONLY);

  if(req_pipe < 0 || resp_pipe < 0){
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(server_pipe);
    return 1;
  }
  
  int session_id;
  if(read(resp_pipe, &session_id, sizeof(int)) < 0){
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(server_pipe);
    close(req_pipe);
    close(resp_pipe);
    return 1;

  }

  cur_session_id = session_id;
  return 0;

}

int ems_quit(void) { 
  //TODO: close pipes
  char request_message[MAX_REQUEST_MESSAGE];
  int op_code = 2;

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,&op_code,sizeof(int));
  if(write(req_pipe,request_message,MAX_REQUEST_MESSAGE) < 0) return 1;

  close(req_pipe);
  close(resp_pipe);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  char request_message[MAX_REQUEST_MESSAGE];
  int op_code = 3;

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,&op_code,sizeof(int));
  memcpy(request_message + 1,&event_id,sizeof(int));
  memcpy(request_message + 1 + sizeof(int),&num_rows,sizeof(size_t));
  memcpy(request_message + 1 + sizeof(int) + sizeof(size_t),&num_cols,sizeof(size_t));

  if(write(req_pipe,request_message,MAX_REQUEST_MESSAGE) < 0) return 1;

  int success;
  if(read(resp_pipe,&success,sizeof(int)) <= 0) return 1;

  return success;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)

  char request_message[MAX_REQUEST_MESSAGE];
  int op_code = 4;

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,&op_code,sizeof(int));
  memcpy(request_message + 1,&event_id,sizeof(int));
  memcpy(request_message + 1 + sizeof(int),&num_seats,sizeof(size_t));
  memcpy(request_message + 1 + sizeof(int) + sizeof(size_t),&xs,sizeof(size_t*));
  memcpy(request_message + 1 + sizeof(int) + sizeof(size_t) + sizeof(size_t*),&ys,sizeof(size_t*));

  if(write(req_pipe,request_message,MAX_REQUEST_MESSAGE) < 0) return 1;

  int success;
  if(read(resp_pipe,&success,sizeof(int)) <= 0) return 1;

  return success;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)

  char request_message[MAX_REQUEST_MESSAGE];
  int op_code = 5;

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,&op_code,sizeof(int));
  memcpy(request_message + 1,&event_id,sizeof(unsigned int));

  if(write(req_pipe,request_message,MAX_REQUEST_MESSAGE) < 0) return 1;

  int success;
  size_t rows, cols;
  if(read(resp_pipe,&success,sizeof(int)) <= 0 || read(resp_pipe,&rows,sizeof(size_t)) <= 0 || read(resp_pipe,&cols,sizeof(size_t)) <= 0 ) return 1;

  size_t event_size = rows * cols;
  unsigned int *matrix = malloc(sizeof(unsigned int) * event_size);

  if(read(resp_pipe,matrix,event_size) < 0) return 1;


  for (size_t i = 1; i <= rows; i++) {
    for (size_t j = 1; j <= cols; j++) {
      char buffer[16];
      sprintf(buffer, "%u", matrix[i * j]);

      write(out_fd,&buffer,sizeof(unsigned int));
  

      if (j < cols) {
        write(out_fd," ",sizeof(char));
      }
    }
    write(out_fd,"\n",sizeof(char));
  }

  free(matrix);
  return success;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  char request_message[MAX_REQUEST_MESSAGE];
  int op_code = 6;

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,&op_code,sizeof(int));

  if(write(req_pipe,request_message,MAX_REQUEST_MESSAGE) < 0) return 1;

  int success;
  size_t n_events;
  if(read(resp_pipe,&success,sizeof(int)) <= 0 || read(resp_pipe,&n_events,sizeof(size_t)) <= 0) return 1;

  unsigned int id_list[n_events];

  if(read(resp_pipe,&id_list,n_events) <= 0) return 1;

  for(size_t i = 0; i < n_events; i++){
    char buff[] = "Event: ";
    write(out_fd,buff,sizeof(buff));

    char id[16];
    sprintf(id, "%u\n", id_list[i]);
    write(out_fd,id,sizeof(unsigned int) + sizeof(char));
  }

  return success;
}
