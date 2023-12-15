#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>


#include "eventlist.h"
#include "constants.h"
#include "parser.h"
#include "operations.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;
pthread_rwlock_t global_lock;
int wait_id = -1;

void write_to_file(const char *message,const int output_fd){
  write(output_fd,message,strlen(message));
}

char* parse_file_name(char *fileName) {
  return strtok(fileName,".");
}

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int* get_seat_with_delay(struct Event* event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_ms) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  pthread_rwlock_init(&event_list -> list_lock_rw,NULL);
  pthread_rwlock_init(&global_lock,NULL);
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  free_list(event_list);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {

  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }
    

  if (get_event_with_delay(event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    return 1;
    
  }

  struct Event* event = malloc(sizeof(struct Event));
  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    return 1;
    
  }

  pthread_rwlock_init(&event->event_lock_rw,NULL);
  pthread_rwlock_wrlock(&event -> event_lock_rw);

  event->id =  event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));
 
  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    return 1;

  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }
  
  pthread_rwlock_unlock(&event -> event_lock_rw);

  if (append_to_list(event_list, event)) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    free(event);
    return 1;
  }
 
  return 0; 

}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {


  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
    
  }



  struct Event* event = get_event_with_delay(event_id);
  
  if (event == NULL) {
    write_to_file("Event not found\n",STDERR_FILENO);
    return 1;
   
  }


  pthread_rwlock_wrlock(&event -> event_lock_rw);
  unsigned int reservation_id = ++event->reservations;
  pthread_rwlock_unlock(&event -> event_lock_rw);

  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
          
      break;
    }


    pthread_rwlock_wrlock(&event -> event_lock_rw);

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;

    pthread_rwlock_unlock(&event -> event_lock_rw);

  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    pthread_rwlock_wrlock(&event -> event_lock_rw);
    event->reservations--;
    pthread_rwlock_unlock(&event -> event_lock_rw);

    for (size_t j = 0; j < i; j++) {

      pthread_rwlock_wrlock(&event -> event_lock_rw);

      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;

      pthread_rwlock_unlock(&event -> event_lock_rw);

    }
    return 1;
  }

  return 0; 

}

int ems_show(unsigned int event_id, const int output_fd) {
  
  if (event_list == NULL) {
    write_to_file("EMS state must be initialized\n",STDERR_FILENO);
    return 1;

  }
  

  struct Event* event = get_event_with_delay(event_id);


  if (event == NULL) {
    write_to_file("Event not found\n",STDERR_FILENO);
    return 1; 

  }


  pthread_rwlock_wrlock(&global_lock);
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {

      
      pthread_rwlock_rdlock(&event -> event_lock_rw);
      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));
      pthread_rwlock_unlock(&event -> event_lock_rw);
    

      char seat_str[16];  

      sprintf(seat_str, "%u", *seat);
      write_to_file(seat_str,output_fd);

      if (j < event->cols) {
        write_to_file(" ", output_fd);
  
      }
    }
    write_to_file("\n",output_fd);

  }
  pthread_rwlock_unlock(&global_lock);
  return 0; 

}

int ems_list_events(const int output_fd) {

  if (event_list == NULL) {
    write_to_file("EMS state must be initialized\n",output_fd);
    return 1;
     
  }

  pthread_rwlock_wrlock(&global_lock);

  pthread_rwlock_rdlock(&event_list -> list_lock_rw);
  if (event_list->head == NULL) {
    write_to_file("No events\n",output_fd);
    pthread_rwlock_unlock(&event_list -> list_lock_rw);
    return 1;
   
  }
  
  struct ListNode* current = event_list->head;

  while (current != NULL) {
    write_to_file("Event: ",output_fd);
    char id[16];  

    

    sprintf(id,"%u\n",(current->event)->id);
  
    write_to_file(id,output_fd);


    current = current->next;
    
  }
  pthread_rwlock_unlock(&global_lock);
  return 0; 

}

void ems_wait(unsigned int thread_id) {
  pthread_rwlock_wrlock(&global_lock);
  wait_id = (int)thread_id;
  pthread_rwlock_unlock(&global_lock);

}


// checks if the current thread sould execute the command 
int should_execute(int lines_read, int max_thread, int thread_index){
  if(lines_read % max_thread != thread_index){
    return 0;
  }
  return 1;
}

void* compute_file(void* args){


    int lines_read = 0, eoc = 0;
    struct FileArgs* arguments = (struct FileArgs*)args;
    int fd_output = arguments->fd_output;
    int fd_input = arguments->fd_input;
    int thread_index = arguments->thread_index;
    int max_thread = arguments ->max_threads;
    free(args);

    unsigned int delay_ms;
    unsigned int thread_id;

    

    while (1){
      
      // all threads wait
      if(wait_id == 0){

        struct timespec delay = delay_to_timespec(delay_ms);
        nanosleep(&delay, NULL);
      }

      // only a specific thread waits
      if(wait_id == thread_index){

        struct timespec delay = delay_to_timespec(delay_ms);
        nanosleep(&delay, NULL);
        wait_id = -1;
      }
     
 
      unsigned int event_id;
      size_t num_rows, num_columns, num_coords;
      size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];


      switch (get_next(fd_input)) {
        
        
        case CMD_CREATE:
          
          if (parse_create(fd_input, &event_id, &num_rows, &num_columns)) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            break;

          }
          if(should_execute(lines_read,max_thread,thread_index)){

            if (ems_create(event_id, num_rows, num_columns)) {
              fprintf(stderr, "Failed to create event\n");
              break;
        
            } 
        
            break;
          }
          break;

        case CMD_RESERVE:
          
          num_coords = parse_reserve(fd_input, MAX_RESERVATION_SIZE, &event_id, xs, ys);

          if (num_coords == 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            break;

          }
          if(should_execute(lines_read,max_thread,thread_index)){

            if (ems_reserve(event_id, num_coords, xs, ys)) {
              fprintf(stderr, "Failed to reserve seats\n");
              break;
            
            }
            break;
          }
          break;

        case CMD_SHOW:
          
          if (parse_show(fd_input, &event_id) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            break;

          }

          if(should_execute(lines_read,max_thread,thread_index)){

            if (ems_show(event_id,fd_output)) {
              fprintf(stderr, "Failed to show event\n");
              break;
            }
            
            break;
          }
          break;

        case CMD_LIST_EVENTS:
          if(should_execute(lines_read,max_thread,thread_index)){

            if (ems_list_events(fd_output)) {
              fprintf(stderr, "Failed to list events\n");
              break;
            
            }
            break;
          }
          break;

        case CMD_WAIT:
          if (parse_wait(fd_input, &delay_ms, &thread_id) == -1) { 
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          
          }
          if(should_execute(lines_read,max_thread,thread_index)){

            if (delay_ms > 0) {
              printf("Waiting...\n");
              ems_wait(thread_id);
              
              break;
            }

            break;
          }
          break;

        case CMD_INVALID:
          fprintf(stderr, "Invalid command. See HELP for usage\n");

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

        case CMD_BARRIER: 
          pthread_exit((void*)EXIT_FAILURE);
        case CMD_EMPTY:
          break;

        case EOC: 
          eoc = 1;
          close(fd_input);
          break;

          
      }
      if(eoc){
        pthread_exit((void*)EXIT_SUCCESS);

      }
      
      lines_read++;
      }
    


}