#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include<sys/wait.h>
#include <pthread.h>
  
#include "constants.h"
#include "operations.h"
#include "parser.h"




#define BUFFER_SIZE 1024



int main(int argc, char *argv[]) {

  DIR *dir; // pointer for a directory struct 

  struct dirent *entry; // pointer for the entry of a directory 

  
  
  if(argc != 4){
    write_to_file("Wrong number of arguments\n",STDERR_FILENO);
    exit(EXIT_FAILURE);
  }

   
  unsigned int delay; 

  if(argv[4] != NULL){
    sscanf(argv[4],"%u",&delay);
  }
  else{
    delay = STATE_ACCESS_DELAY_MS;
  }

  if (delay > UINT_MAX) {
      write_to_file("Invalid delay value or value too large\n",STDERR_FILENO);
      exit(EXIT_FAILURE);
  }
  


  dir = opendir(argv[1]); // opens the directory and stores the result in the variable
  
  if (dir == NULL){
        write_to_file("Error opening the directory\n",STDERR_FILENO);
        exit(EXIT_FAILURE);
  }
  

  int fd_output;


  int max_proc,max_thread,status ,n_proc = 0;
  sscanf(argv[2],"%d",&max_proc);
  
  sscanf(argv[3],"%d",&max_thread);

  // array that will contain the pid of the child processes
  pid_t pid[max_proc];
  
  // initializes the array 
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


        if(max_proc <= 0){
          printf("Invalid max number of processes\n");
          exit(1);
        }

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
            // array that will contain the id of each thread 
            pthread_t t_id[max_thread];      
            ems_init(delay);
            

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
                    
            int fd_input;


            char outputFilePath[MAX_PATH_SIZE];
            snprintf(outputFilePath, MAX_PATH_SIZE, "%s/%s.out", argv[1], fileName); 
            
            // opens the file and erases its content if it already exists, creates a new one if it doesn't
            fd_output = open(outputFilePath,O_WRONLY | O_CREAT | O_TRUNC);  
            
            if(fd_output < 0){
                write_to_file("Error opening output file \n",STDERR_FILENO);
                exit(EXIT_FAILURE);
            }
            



            for(int i = 0; i < max_thread; i++){

              fd_input = open(inputFilePath, O_RDONLY); // Opens the file to read only mode

              if(fd_input < 0){
                
                write_to_file("Error opening inputfile\n",STDERR_FILENO);
                exit(EXIT_FAILURE);
              }
              struct FileArgs* args = malloc(sizeof(struct FileArgs));
      
              args->fd_input = fd_input;
              args->fd_output = fd_output;
              args->max_threads = max_thread;
              args -> thread_index = i;
              pthread_create(&t_id[i],NULL,compute_file,args);
                
            }

            for(int i = 0; i < max_thread; i++){
              if(pthread_join(t_id[i],1) == 0){
                
              }
              
            }
            
            ems_terminate();
            close(fd_output);
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
  exit(EXIT_SUCCESS);
}



