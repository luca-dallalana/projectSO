#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "eventlist.h"
#include "constants.h"
#include "parser.h"
#include "operations.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;


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

int ems_create(struct CreateArgs* args) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (get_event_with_delay(args -> event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    return 1;
  }

  event->id = args -> event_id;
  event->rows = args -> num_rows;
  event->cols = args -> num_cols;
  event->reservations = 0;
  event->data = malloc(args -> num_rows * args -> num_cols * sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    return 1;
  }

  for (size_t i = 0; i < args -> num_rows * args -> num_cols; i++) {
    event->data[i] = 0;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    free(event);
    return 1;
  }

  return 0;
}

int ems_reserve(struct ReserveArgs* args) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(args -> event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  for (; i < args -> num_seats; i++) {
    size_t row = args -> xs[i];
    size_t col = args -> ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < args -> num_seats) {
    event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, args -> xs[j], args -> ys[j])) = 0;
    }
    return 1;
  }

  return 0;
}

int ems_show(struct ShowArgs* args) {

  if (event_list == NULL) {
    write_to_file("EMS state must be initialized\n",args -> output_fd);
    return 1;
  }

  struct Event* event = get_event_with_delay(args -> event_id);

  if (event == NULL) {
    write_to_file("Event not found\n",args -> output_fd); 
    return 1;
  }

  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));

      char seat_str[16];  

      sprintf(seat_str, "%u", *seat);
      write_to_file(seat_str,args -> output_fd);

      if (j < event->cols) {
        write_to_file(" ",args -> output_fd);
  
      }
    }
    write_to_file("\n",args -> output_fd);

  }

  return 0;
}

int ems_list_events(const int output_fd) {
  if (event_list == NULL) {
    write_to_file("EMS state must be initialized\n",output_fd);
    return 1;
  }

  if (event_list->head == NULL) {
    write_to_file("No events\n",output_fd);
   
    return 0;
  }

  struct ListNode* current = event_list->head;
  while (current != NULL) {
    write_to_file("Event: ",output_fd);
    char id[16];  

    sprintf(id,"%u\n",(current->event)->id);
    write_to_file(id,output_fd);
  
    current = current->next;
  }

  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}


void add_tid(pthread_t t_id[], pthread_t id, int max_thread){
  for(int i = 0; i < max_thread; i++){
      if(t_id[i] != 0){
        t_id[i] = id;
        break;
      }
    }
}


void compute_file(int fd_input, int fd_output, unsigned int delay, int max_thread){

    // array that will contain the id of each thread 
    pthread_t t_id[max_thread];
    
    // initializes the array
    for(int i = 0; i < max_thread; i++){
      t_id[i] = 0;
    }

    while (1) {
          unsigned int event_id;
          size_t num_rows, num_columns, num_coords;
          size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
         
          int eoc = 0;

          switch (get_next(fd_input)) {
            
            pthread_t id;

            case CMD_CREATE:
              if (parse_create(fd_input, &event_id, &num_rows, &num_columns) != 0) {
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }
              struct CreateArgs* create_args = malloc(sizeof(struct CreateArgs));

              create_args->event_id = event_id;
              create_args->num_rows = num_rows;
              create_args->num_cols = num_columns;

              if(pthread_create(&id,NULL,ems_create,create_args) != 0){
                write_to_file("Failed to create event\n",STDERR_FILENO);
              }
              
              add_tid(t_id,id,max_thread);

              // future locks ?
        
              
              break;

            case CMD_RESERVE:
              num_coords = parse_reserve(fd_input, MAX_RESERVATION_SIZE, &event_id, xs, ys);

              if (num_coords == 0) {
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }

              struct ReserveArgs* reserve_args = malloc(sizeof(struct ReserveArgs));

              reserve_args->event_id = event_id;
              reserve_args->num_seats = num_coords;
              reserve_args->xs = xs;
              reserve_args->ys = ys;

              if(pthread_create(&id,NULL,ems_create,reserve_args) != 0){

                write_to_file("Failed to reserve seats\n",STDERR_FILENO);
              }

              add_tid(t_id,id,max_thread);
              
              // future locks?
                 

              break;

            case CMD_SHOW:
              if (parse_show(fd_input, &event_id) != 0) {
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }

              struct ShowArgs* show_args = malloc(sizeof(struct ShowArgs));

              if(pthread_create(&id,NULL,ems_show,show_args)){
                  
                write_to_file("Failed to show event\n",STDERR_FILENO);
              
              }
              
              add_tid(t_id,id,max_thread);

              // future locks ?
              
            
              break;

            case CMD_LIST_EVENTS:
              if (pthread_create(&id,NULL,ems_list_events,fd_output) != 0) {
                write_to_file("Failed to list events\n",STDERR_FILENO);
        
              }
              // future locks? 
              break;

            case CMD_WAIT:
              if (parse_wait(fd_input, &delay, NULL) == -1) {  // thread_id is not implemented
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }

              if (delay > 0) {
                printf("Waiting...\n");
                pthread_create(&id,NULL,ems_wait,delay);
      
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