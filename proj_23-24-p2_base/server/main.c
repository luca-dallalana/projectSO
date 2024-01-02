#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <errno.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "eventlist.h"

struct Session session;

int session_request(int pipe){


    if(read(pipe,&session.req_pipe_path,MAX_PIPE_PATH_NAME) < 0 || read(pipe,&session.resp_pipe_path,MAX_PIPE_PATH_NAME) < 0){
      return 1;
    }
    session.session_id = 1;
    session.active = 1;

    return 0;
  

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

  if(mkfifo(argv[1],0666) < 0){
    printf("%s\n",strerror(errno));
    return 1;
  }



  int register_pipe;
  if((register_pipe = open(argv[1],O_RDONLY)) < 0){
    unlink(argv[1]);
    return 1;
  }



  while (1) {
    //TODO: Read from pipe
    //TODO: Write new client to the producer-consumer buffer
    int code;
    int resp_pipe, req_pipe;

    if(read(register_pipe,&code,sizeof(int))) break;

    if(code == 0){   
      if(session_request(register_pipe)){
        fprintf(stderr,"Failed session request\n");
        return 1;
      }

      resp_pipe = open(session.resp_pipe_path,O_WRONLY);
      req_pipe = open(session.req_pipe_path, O_RDONLY);

      if(resp_pipe < 0 || req_pipe < 0){
        return 1;
      }
      write(resp_pipe,&session.session_id,sizeof(int));
    }

    if(read(req_pipe,&code,sizeof(int)) <= 0) break;

    process_request(code,req_pipe,resp_pipe);

  }

  //TODO: Close Server
  close(register_pipe);
  unlink(argv[1]);
  ems_terminate();
}