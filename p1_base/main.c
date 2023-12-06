#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include<sys/wait.h>
  
#include "constants.h"
#include "operations.h"
#include "parser.h"


#define MAX_PATH_SIZE 256 + 2
#define BUFFER_SIZE 1024


void compute_file(int fd_input, int fd_output, unsigned int delay){
    while (1) {
          unsigned int event_id;
          size_t num_rows, num_columns, num_coords;
          size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
         
          int eoc = 0;

          switch (get_next(fd_input)) {
            case CMD_CREATE:
              if (parse_create(fd_input, &event_id, &num_rows, &num_columns) != 0) {
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }

              if (ems_create(event_id, num_rows, num_columns)) {
                write_to_file("Failed to create event\n",STDERR_FILENO);
              }

              break;

            case CMD_RESERVE:
              num_coords = parse_reserve(fd_input, MAX_RESERVATION_SIZE, &event_id, xs, ys);

              if (num_coords == 0) {
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }

              if (ems_reserve(event_id, num_coords, xs, ys)) {
                write_to_file("Failed to reserve seats\n",STDERR_FILENO);
        
              }

              break;

            case CMD_SHOW:
              if (parse_show(fd_input, &event_id) != 0) {
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }

              if (ems_show(event_id, fd_output)) {
                write_to_file("Failed to show event\n",STDERR_FILENO);
              }
              
            
              break;

            case CMD_LIST_EVENTS:
              if (ems_list_events(fd_output)) {
                write_to_file("Failed to list events\n",STDERR_FILENO);
        
              }
            
              break;

            case CMD_WAIT:
              if (parse_wait(fd_input, &delay, NULL) == -1) {  // thread_id is not implemented
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }

              if (delay > 0) {
                printf("Waiting...\n");
                ems_wait(delay);
              }

              break;

            case CMD_INVALID:
              write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
              break;

            case CMD_HELP:
              printf(
                  "Available commands:\n"
                  "  CREATE <event_id> <num_rows> <num_columns>\n"
                  "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
                  "  SHOW <event_id>\n"
                  "  LIST\n"
                  "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
                  "  BARRIER\n"                      // Not implemented
                  "  HELP\n");

              break;

            case CMD_BARRIER:  // Not implemented
              break;
            case CMD_EMPTY:
              break;

            case EOC:
              eoc = 1;
              close(fd_input);
              close(fd_output);
        
              break;
          }

          if(eoc){
            break;
          }
        
        }
}

int main(int argc, char *argv[]) {

  DIR *dir; // pointer for a directory struct 

  struct dirent *entry; // pointer for the entry of a directory 

  
  
  if(argc != 4){
    write_to_file("Wrong number of arguments\n",STDERR_FILENO);
    exit(EXIT_FAILURE);
  }

   
  unsigned int delay; 
  sscanf(argv[2],"%u",&delay);

  if ( delay > UINT_MAX) {
      write_to_file("Invalid delay value or value too large\n",STDERR_FILENO);
      exit(EXIT_FAILURE);
  }

   ems_init(delay);

  dir = opendir(argv[1]); // opens the directory and stores the result in the variable
  
  if (dir == NULL){
        write_to_file("Error opening the directory\n",STDERR_FILENO);
        exit(EXIT_FAILURE);
  }
  
  int fd_input; 
  int fd_output;

  int max_proc,status ,n_proc = 0;
  sscanf(argv[3],"%i",&max_proc);
  int pid[max_proc];
  
  // initializes the array that will contain the pid of the child processes
  for(int i = 0; i < max_proc ; i++){
    pid[i] = 0;
  }

  while ((entry = readdir(dir))) { // while there are directories to be read

    // skips "invisible" files
    if(strcmp(entry->d_name,"..") == 0 || strcmp(entry->d_name,".") == 0 ){
      continue;
    }

    // skips .out files
    if (strstr(entry->d_name, ".out") != NULL){
      continue;
    }


    if (strstr(entry->d_name, ".jobs") != NULL) { // If the directory is regular and it contains .jobs files

        // if the number of parallel processes reaches max it waits until one of the existing processes exits to create another    
        if(n_proc == max_proc){
            
          for(int i = 0; i < n_proc; i++){

            pid_t cpid = waitpid(pid[i],&status,0);
            
            if (cpid != -1 && WIFEXITED(status)){
                // removes the process from the array of pid
                pid[i] = 0;
                n_proc--;
                printf("Child %d terminated with status: %d\n", cpid, WEXITSTATUS(status));
                break;                  
            }
          
          }       
        }

        int cur_pid;

        // creates a new process
        cur_pid = fork();
        n_proc++;

        // child process code
        if(cur_pid == 0){
          // looks for an empty space in the array of pid to add the new process
          for(int i = 0; i < max_proc; i++){

            if(pid[i] == 0){
              pid[i] = getpid();
              break;
            }

          }

          char *fileName;
          fileName = parse_file_name(entry->d_name);

          char inputFilePath[MAX_PATH_SIZE]; // Max size for a path
          snprintf(inputFilePath, MAX_PATH_SIZE, "%s/%s.jobs", argv[1], fileName); // Concatenates the directory path with the file name

          fd_input = open(inputFilePath, O_RDONLY); // Opens the file to read only mode
      
          if(fd_input < 0){
              
              write_to_file("Error opening inputfile\n",STDERR_FILENO);
              exit(EXIT_FAILURE);
          }
          
      
          char outputFilePath[MAX_PATH_SIZE];
          snprintf(outputFilePath, MAX_PATH_SIZE, "%s/%s.out", argv[1], fileName); 
          
          // opens the file and erases its content if it already exists, creates a new one if it doesn't
          fd_output = open(outputFilePath,O_WRONLY | O_CREAT | O_TRUNC);  
          
          if(fd_output < 0){
              write_to_file("Error opening output file \n",STDERR_FILENO);
              exit(EXIT_FAILURE);
          }
          
          // if nothing failed it analises the input file and writes to the .out file its content
          compute_file(fd_input,fd_output,delay);

          // terminates the process
          exit(EXIT_SUCCESS);
        }

        else if(cur_pid < 0){
          exit(EXIT_FAILURE);
        }        
        
    }

    // parent process waits for all the child processes
    for(int i = 0; i < max_proc; i++){

      pid_t cpid = waitpid(pid[i],&status,0);

      if (cpid != -1 && WIFEXITED(status)){
          printf("Child %d terminated with status: %d\n", cpid, WEXITSTATUS(status));

      }
  
    }

  }
  closedir(dir);
  ems_terminate();
  exit(EXIT_SUCCESS);
}



