#include <fcntl.h>

#include "api.h"

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  int server_pipe;
  if(server_pipe = open(server_pipe_path,O_WRONLY) < 0){
    return 1;
  }

  if(mkfifo(req_pipe_path,0777) < 0 || mkfifo(resp_pipe_path,0777) < 0){
    return 1;
  }

  char session_request_data[256];
  snprintf(session_request_data, sizeof(session_request_data), "%s %s", req_pipe_path, resp_pipe_path);

  // requests session
  write(server_pipe,session_request_data,sizeof(session_request_data));


  req_pipe = open(req_pipe_path, O_WRONLY);
  resp_pipe = open(resp_pipe_path, O_RDONLY);

  if(req_pipe < 0 || resp_pipe < 0){
      return 1;
  }
  
  char resp_buffer[256];
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
  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}
