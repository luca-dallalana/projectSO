#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>

  
#include "constants.h"
#include "operations.h"
#include "parser.h"

#define MAX_PATH_SIZE 256 + 2
#define BUFFER_SIZE 1024




int main(int argc, char *argv[]) {

  DIR *dir; // pointer for a directory struct 

  struct dirent *entry; // pointer for the entry of a directory 



  if(argc != 3){
    write_to_file("Wrong number of arguments\n",STDERR_FILENO);
    return 1;
  }

   
  unsigned int delay; 
  sscanf(argv[2],"%u",&delay);

  if ( delay > UINT_MAX) {
      write_to_file("Invalid delay value or value too large\n",STDERR_FILENO);
    
      return 1;
  }

   ems_init(delay);

  dir = opendir(argv[1]); // opens the directory and stores the result in the variable
  
  if (dir == NULL){
        write_to_file("Error opening the directory\n",STDERR_FILENO);
        return 1;
  }
  
  int fd_input; // the result of a file operation
  int fd_output;

  while ((entry = readdir(dir))) { // while there are directories to be read

    if(strcmp(entry->d_name,"..") == 0 || strcmp(entry->d_name,".") == 0 ){
      continue;
    }

    if (strstr(entry->d_name, ".out") != NULL){
      continue;
    }

    if (strstr(entry->d_name, ".jobs") != NULL) { // If the directory is regular and it contains .jobs files

        char *fileName;
        fileName = parse_file_name(entry->d_name);

        char inputFilePath[MAX_PATH_SIZE]; // Max size for a path
        snprintf(inputFilePath, MAX_PATH_SIZE, "%s/%s.jobs", argv[1], fileName); // Concatenates the directory path with the file name

        fd_input = open(inputFilePath, O_RDONLY); // Opens the file to read only mode
       
        if(fd_input < 0){
            
            write_to_file("Error opening file\n",STDERR_FILENO);
            return 1;
        }
        
     
        char outputFilePath[MAX_PATH_SIZE];
        snprintf(outputFilePath, MAX_PATH_SIZE, "%s/%s.out", argv[1], fileName);
      
        
        fd_output = open(outputFilePath,O_WRONLY | O_CREAT);
        
        if(fd_output < 0){
            write_to_file("Error opening file 2\n",STDERR_FILENO);
            return 1;
        }

       
        
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

  }
  closedir(dir);
  ems_terminate();
  return 0;
}



