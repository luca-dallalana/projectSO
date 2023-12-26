#include <fcntl.h>
#include <string.h>
#include "api.h"
#include "parser.h"
#include "common/constants.h"

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  int server_pipe;
  if(server_pipe = open(server_pipe_path,O_WRONLY) < 0){
    return 1;
  }

  if(mkfifo(req_pipe_path,0777) < 0 || mkfifo(resp_pipe_path,0777) < 0){
    return 1;
  }

  char request_message[MAX_REQUEST_MESSAGE];

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,CMD_REQUEST,1);
  memcpy(request_message + 1, req_pipe_path,MAX_PIPE_PATH_NAME);
  memcpy(request_message + 1 + MAX_PIPE_PATH_NAME, resp_pipe_path, MAX_PIPE_PATH_NAME);

  // requests session
  if(write(server_pipe,request_message,sizeof(MAX_REQUEST_MESSAGE)) < 0) return 1;


  req_pipe = open(req_pipe_path, O_WRONLY);
  resp_pipe = open(resp_pipe_path, O_RDONLY);

  if(req_pipe < 0 || resp_pipe < 0){
      return 1;
  }
  
  char resp_buffer[16];
  int bytesRead = read(resp_pipe, resp_buffer, sizeof(resp_buffer));

  if(bytesRead > 0){
    sscanf(resp_buffer,"%d",&cur_session_id);
    return 0;
  }

  close(req_pipe);
  close(resp_pipe);
  return 1;
}

int ems_quit(void) { 
  //TODO: close pipes
  close(req_pipe);
  close(resp_pipe);
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  char request_message[MAX_REQUEST_MESSAGE];

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,CMD_CREATE,1);
  memcpy(request_message + 1,event_id,sizeof(int));
  memcpy(request_message + 1 + sizeof(int),num_rows,sizeof(size_t));
  memcpy(request_message + 1 + sizeof(int) + sizeof(size_t),num_cols,sizeof(size_t));

  if(write(req_pipe,request_message,MAX_REQUEST_MESSAGE) < 0) return 1;

  // falta response
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)

  char request_message[MAX_REQUEST_MESSAGE];

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,CMD_RESERVE,1);
  memcpy(request_message + 1,event_id,sizeof(int));
  memcpy(request_message + 1 + sizeof(int),num_seats,sizeof(size_t));
  memcpy(request_message + 1 + sizeof(int) + sizeof(size_t),xs,sizeof(size_t*));
  memcpy(request_message + 1 + sizeof(int) + sizeof(size_t) + sizeof(size_t*),ys,sizeof(size_t*));

  if(write(req_pipe,request_message,MAX_REQUEST_MESSAGE) < 0) return 1;

  // falta response; 
  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)

  char request_message[MAX_REQUEST_MESSAGE];

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,CMD_SHOW,1);
  memcpy(request_message + 1,out_fd,sizeof(int));
  memcpy(request_message + 1 + sizeof(int),event_id,sizeof(unsigned int));

  if(write(req_pipe,request_message,MAX_REQUEST_MESSAGE) < 0) return 1;

  // falta response
  return 0;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  char request_message[MAX_REQUEST_MESSAGE];

  memset(request_message,'\0',MAX_REQUEST_MESSAGE);

  memcpy(request_message,CMD_LIST_EVENTS,1);
  memcpy(request_message + 1,out_fd,sizeof(int));

  if(write(req_pipe,request_message,MAX_REQUEST_MESSAGE) < 0) return 1;

  // falta response
  return 0;
}
