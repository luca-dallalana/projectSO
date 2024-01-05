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
int server_pipe;


// create pipes and connect to the server
int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  
  // unlinks the pipes to make sure there are no current connections
  unlink(req_pipe_path);
  unlink(resp_pipe_path);

  // opens the pipe that connects client to server 
  if((server_pipe = open(server_pipe_path,O_WRONLY))< 0) return 1;

  // defines the variables used to request and get an answer from the server
  char *request_message;
  char req_pipe_name[MAX_PIPE_PATH_NAME];
  char resp_pipe_name[MAX_PIPE_PATH_NAME];

  // initializes both pipe names with \0 
  memset(req_pipe_name,'\0',MAX_PIPE_PATH_NAME);
  memset(resp_pipe_name,'\0',MAX_PIPE_PATH_NAME);

  // passes the path given to the pipe name
  strcpy(req_pipe_name,req_pipe_path);
  strcpy(resp_pipe_name,resp_pipe_path);

  request_message = malloc(SETUP_REQUEST_LEN);

  // creates the request message
  memcpy(request_message,"OP_CODE=1",OP_CODE_LEN);
  memcpy(request_message + OP_CODE_LEN, req_pipe_name,MAX_PIPE_PATH_NAME);
  memcpy(request_message + OP_CODE_LEN + MAX_PIPE_PATH_NAME, resp_pipe_name, MAX_PIPE_PATH_NAME);

  // creates both pipes 
  if(mkfifo(req_pipe_path,0666) < 0 || mkfifo(resp_pipe_path,0666) < 0){
    free(request_message);
    return 1;
  }

  // requests session
  if(write(server_pipe,request_message,SETUP_REQUEST_LEN) < 0){
    free(request_message);
    return 1;

  } 

  free(request_message);

  //opens the request pipe and the response pipe 
  req_pipe = open(req_pipe_path, O_WRONLY);
  resp_pipe = open(resp_pipe_path, O_RDONLY);

  // if there are any errors on the pipe opening OTIMIZACAO POSSIVEL
  if(req_pipe < 0 || resp_pipe < 0){
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(server_pipe);
    return 1;
  }

  
  // reads the current session id from the response pipe
  if(read(resp_pipe, &cur_session_id, sizeof(int)) <= 0){
    close(req_pipe);
    close(resp_pipe);
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(server_pipe);
    return 1;
  }

  close(server_pipe);
  return 0;

}

// close pipes
int ems_quit(void) { 
  char *request_message;

  request_message = malloc(QUIT_REQUEST_LEN);

  // creates the request message
  memcpy(request_message,"OP_CODE=2",OP_CODE_LEN);

  // sends the request
  if(write(req_pipe,request_message,QUIT_REQUEST_LEN) < 0) {
    free(request_message);
    return 1;
  }
  free(request_message);
  close(req_pipe);
  close(resp_pipe);
  return 0;
}

// send create request to the server (through the request pipe) and wait for the response (through the response pipe)
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  char *request_message;

  request_message = malloc(CREATE_REQUEST_LEN);

  // creates the request message
  memcpy(request_message,"OP_CODE=3",OP_CODE_LEN);
  memcpy(request_message + OP_CODE_LEN,&event_id,EVENT_ID_LEN);
  memcpy(request_message + OP_CODE_LEN + EVENT_ID_LEN,&num_rows,ROW_COL_LEN);
  memcpy(request_message + OP_CODE_LEN + EVENT_ID_LEN + ROW_COL_LEN,&num_cols,ROW_COL_LEN);

  // sends the request message
  if(write(req_pipe,request_message,CREATE_REQUEST_LEN) <= 0){
    free(request_message);
    return 1;
  } 

  free(request_message);

  int success;
  // reads the result of the operation from the result pipe
  if(read(resp_pipe,&success,sizeof(int)) <= 0) return 1;

  return success;
}

// send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {

  char *request_message;


  request_message = malloc(RESERVE_REQUEST_LEN);

  // creates the request message
  memcpy(request_message,"OP_CODE=4",OP_CODE_LEN);
  memcpy(request_message + OP_CODE_LEN,&event_id,EVENT_ID_LEN);
  memcpy(request_message + OP_CODE_LEN + EVENT_ID_LEN,&num_seats,SEATS_LEN);
  memcpy(request_message + OP_CODE_LEN + EVENT_ID_LEN+ SEATS_LEN,xs,num_seats * SEATS_LEN);
  memcpy(request_message + OP_CODE_LEN + EVENT_ID_LEN + SEATS_LEN + num_seats * SEATS_LEN,ys,num_seats * SEATS_LEN);

  // sends the request message
  if(write(req_pipe,request_message,RESERVE_REQUEST_LEN) < 0){
    free(request_message);
    return 1;
  } 

  free(request_message);
  int success;
  // reads the result of the operation from the result pipe
  if(read(resp_pipe,&success,sizeof(int)) <= 0) return 1;

  return success;
}

// send show request to the server (through the request pipe) and wait for the response (through the response pipe)
int ems_show(int out_fd, unsigned int event_id) {
  char *request_message;

  // Allocate memory for the request message
  request_message = malloc(SHOW_REQUEST_LEN);

  // Check if memory allocation is successful
  if (request_message == NULL) { // NECESSARIO?
    return 1;
  }

  // creates the request message
  memcpy(request_message, "OP_CODE=5", OP_CODE_LEN);
  memcpy(request_message + OP_CODE_LEN, &event_id, EVENT_ID_LEN);

  // sends the request message
  if (write(req_pipe, request_message, SHOW_REQUEST_LEN) < 0) {
    free(request_message);
    return 1;
  }

  // Clean up allocated memory
  free(request_message);

  int success;
  size_t rows, cols;

  // Read the response from the pipe
  if (read(resp_pipe, &success, sizeof(int)) <= 0 || 
      read(resp_pipe, &rows, ROW_COL_LEN) <= 0 || 
      read(resp_pipe, &cols, ROW_COL_LEN) <= 0) {
    return 1;
  }

  size_t event_size = rows * cols;
  unsigned int *matrix = malloc(sizeof(unsigned int) * event_size);

  // Check if memory allocation is successful
  if (matrix == NULL) { // NECESSARIO?
    return 1;
  }

  // Read the matrix data from the pipe
  if (read(resp_pipe, matrix, sizeof(unsigned int) * event_size) < 0) {
    free(matrix);
    return 1;
  }

  // Write the matrix to the specified output file descriptor
  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      char buffer[16];
      sprintf(buffer, "%u", matrix[i * cols + j]);

      write(out_fd, buffer, strlen(buffer));

      if (j < cols - 1) {
        write(out_fd, " ", sizeof(char));
      }
    }
    write(out_fd, "\n", sizeof(char));
  }

  // Clean up allocated memory
  free(matrix);
  return success;
}

// send list request to the server (through the request pipe) and wait for the response (through the response pipe)
int ems_list_events(int out_fd) {
  char *request_message;

  // Allocate memory for the request message
  request_message = malloc(LIST_REQUEST_LEN);

  // Check if memory allocation is successful
  if (request_message == NULL) {
    return 1;
  }

  // creates the request message
  memcpy(request_message, "OP_CODE=6", OP_CODE_LEN);

  // Write the request message to the pipe
  if (write(req_pipe, request_message, LIST_REQUEST_LEN) < 0) {
    free(request_message);
    return 1;
  }

  // Clean up allocated memory
  free(request_message);

  int success;
  size_t n_events;

  // Read the response from the pipe
  if (read(resp_pipe, &success, sizeof(int)) <= 0 || 
      read(resp_pipe, &n_events, sizeof(size_t)) <= 0) {
    return 1;
  }

  unsigned int id_list[n_events];

  // Read the event IDs from the pipe
  if (read(resp_pipe, &id_list, sizeof(unsigned int) * n_events) <= 0) {
    return 1;
  }
  // write the event list to the output fd
  for (size_t i = 0; i < n_events; i++) {
    char buff[] = "Event: ";
    write(out_fd, buff, sizeof(buff) - 1);  // sizeof(buff) includes the null terminator, subtract 1

    char id[16];
    sprintf(id, "%u\n", id_list[i]);
    write(out_fd, id, strlen(id));
  }

  return success;
}

