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
int eoc = 0;




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

void* ems_create(void* args) {

  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_exit((void*)EXIT_FAILURE); 
  }
  
  struct CreateArgs* create_args = (struct CreateArgs*)args;


  if (get_event_with_delay(create_args -> event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_exit((void*)EXIT_FAILURE); 
    
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    pthread_exit((void*)EXIT_FAILURE); 
    
  }

  pthread_rwlock_init(&event->event_lock_rw,NULL);
  pthread_rwlock_wrlock(&event -> event_lock_rw);

  event->id = create_args -> event_id;
  event->rows = create_args -> num_rows;
  event->cols = create_args -> num_cols;
  event->reservations = 0;
  event->data = malloc(create_args -> num_rows * create_args -> num_cols * sizeof(unsigned int));
 
  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    pthread_exit((void*)EXIT_FAILURE); 

  }

  for (size_t i = 0; i < event -> rows * event -> cols; i++) {
    event->data[i] = 0;
  }
  
  pthread_rwlock_unlock(&event -> event_lock_rw);
  
  if (append_to_list(event_list, event)) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    free(event);
    pthread_exit((void*)EXIT_FAILURE); 
  }
  
  pthread_exit((void*)EXIT_SUCCESS); 

}

void* ems_reserve(void* args) {


  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    
  }


  struct ReserveArgs* reserve_args = (struct ReserveArgs*)args;
  struct Event* event = get_event_with_delay(reserve_args ->event_id);
  
  if (event == NULL) {
    write_to_file("Event not found\n",STDERR_FILENO);
    pthread_exit((void*)EXIT_FAILURE); 
   
  }


  pthread_rwlock_wrlock(&event -> event_lock_rw);
  unsigned int reservation_id = ++event->reservations;
  pthread_rwlock_unlock(&event -> event_lock_rw);

  size_t i = 0;
  for (; i < reserve_args -> num_seats; i++) {
    size_t row = reserve_args -> xs[i];
    size_t col = reserve_args -> ys[i];

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
  if (i < reserve_args -> num_seats) {

    event->reservations--;

    for (size_t j = 0; j < i; j++) {

      pthread_rwlock_wrlock(&event -> event_lock_rw);

      *get_seat_with_delay(event, seat_index(event, reserve_args -> xs[j], reserve_args -> ys[j])) = 0;

      pthread_rwlock_unlock(&event -> event_lock_rw);

    }
    pthread_exit((void*)EXIT_FAILURE); 
  }


  
  pthread_exit((void*)EXIT_SUCCESS); 

}

void* ems_show(void* args) {

  struct ShowArgs* show_args = (struct ShowArgs*)args;
  
  if (event_list == NULL) {
    write_to_file("EMS state must be initialized\n",show_args -> output_fd);
    pthread_exit((void*)EXIT_FAILURE); 

  }
  

  struct Event* event = get_event_with_delay(show_args -> event_id);


  if (event == NULL) {
    write_to_file("Event not found\n",STDERR_FILENO);
    pthread_exit((void*)EXIT_FAILURE);  

  }


  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {

      
      //pthread_rwlock_rdlock(&event -> event_lock_rw);
      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));
      //pthread_rwlock_unlock(&event -> event_lock_rw);
    

      char seat_str[16];  

      sprintf(seat_str, "%u", *seat);
      write_to_file(seat_str,show_args -> output_fd);

      if (j < event->cols) {
        write_to_file(" ",show_args -> output_fd);
  
      }
    }
    write_to_file("\n",show_args -> output_fd);

  }

  pthread_exit((void*)EXIT_SUCCESS); 

}

void* ems_list_events(void* output_fd) {

  if (event_list == NULL) {
    write_to_file("EMS state must be initialized\n",*(const int*)output_fd);
    pthread_exit((void*)EXIT_FAILURE); 
     
  }

  pthread_rwlock_rdlock(&event_list -> list_lock_rw);
  if (event_list->head == NULL) {
    write_to_file("No events\n",*(const int*)output_fd);
    pthread_rwlock_unlock(&event_list -> list_lock_rw);
    pthread_exit((void*)EXIT_FAILURE); 
   
  }
  
  struct ListNode* current = event_list->head;

  while (current != NULL) {
    write_to_file("Event: ",*(const int*)output_fd);
    char id[16];  

    //pthread_rwlock_rdlock(&current -> event -> event_lock_rw);

    sprintf(id,"%u\n",(current->event)->id);
  
    write_to_file(id,*(const int*)output_fd);

    //pthread_rwlock_unlock(&current -> event -> event_lock_rw);

    current = current->next;
    
  }
  pthread_rwlock_unlock(&event_list -> list_lock_rw);
  pthread_exit((void*)EXIT_SUCCESS); 

}

void* ems_wait(void* delay_ms) {
  struct timespec delay = delay_to_timespec(*(unsigned int*)delay_ms);
  nanosleep(&delay, NULL);
  pthread_exit((void*)EXIT_SUCCESS); 
}


void add_tid(pthread_t t_id[], pthread_t id, int max_thread){
  for(int i = 0; i < max_thread; i++){
      if(t_id[i] == 0){
        t_id[i] = id;
        break;
      }
    }
}



void compute_file(int fd_input, int fd_output, unsigned int delay, int max_thread){

    // array that will contain the id of each thread 
    pthread_t t_id[max_thread];
    int n_threads = 0;
    
    // initializes the array

    for(int i = 0; i < max_thread; i++){
      t_id[i] = 0;
    }
  

    while (1) {
          unsigned int event_id;
          size_t num_rows, num_columns, num_coords;
          size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
          
          pthread_t id;

          if(n_threads == max_thread){

            for(int i = 0; i < max_thread; i++){
              if(pthread_join(t_id[i],(void*)EXIT_SUCCESS) == 0){
                t_id[i] = 0;
                n_threads--;
                break;
              }
            }
          }
          
          switch (get_next(fd_input)) {

            
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
                free(create_args);
                break;
              }


              n_threads++;
              add_tid(t_id,id,max_thread);
            
              
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

              if(pthread_create(&id,NULL,ems_reserve,reserve_args) != 0){

                write_to_file("Failed to reserve seats\n",STDERR_FILENO);
                free(reserve_args);
                break;
              }

              n_threads++;
              add_tid(t_id,id,max_thread);
              
              break;


            case CMD_SHOW:
              if (parse_show(fd_input, &event_id) != 0) {
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }
              

              struct ShowArgs* show_args = malloc(sizeof(struct ShowArgs));

              show_args -> event_id = event_id;
              show_args -> output_fd = fd_output;

              if(pthread_create(&id,NULL,ems_show,show_args) != 0){
                  
                write_to_file("Failed to show event\n",STDERR_FILENO);
                free(show_args);
                break;
              
              }
                n_threads++;
                add_tid(t_id,id,max_thread);

              
                break;


            case CMD_LIST_EVENTS:
              if (pthread_create(&id,NULL,ems_list_events,&fd_output) != 0) {
                write_to_file("Failed to list events\n",STDERR_FILENO);
                break;
        
              }
      
                add_tid(t_id,id,max_thread);
                n_threads++;
                break;


            case CMD_WAIT:
              if (parse_wait(fd_input, &delay, NULL) == -1) {  // thread_id is not implemented
                printf("wait\n");
                write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
                continue;
              }

              if (delay > 0) {
                printf("Waiting...\n");
                pthread_create(&id,NULL,ems_wait,&delay);
                add_tid(t_id,id,max_thread);
                n_threads++;
                break;
      
              }
              break;
              

            case CMD_INVALID:
              printf("invalid\n");
              write_to_file("Invalid command. See HELP for usage\n",STDERR_FILENO);
              break;

            case CMD_HELP:
              printf(
                  "Available commands:\n"
                  "  CREATE <event_id> <num_rows> <num_columns>\n"
                  "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
                  "  SHOW <event_id>\n"
                  "  LIST\n"
                  "  WAIT <delay_ms> [thread_id]\n"  
                  "  BARRIER\n"                     
                  "  HELP\n");

              break;

            case CMD_BARRIER:
              for(int i = 0; i < max_thread; i++){

                if(pthread_join(t_id[i],(void*)EXIT_SUCCESS) == 0){
                  t_id[i] = 0;
                  n_threads--;
                }
              }
              break;
            case CMD_EMPTY:
              break;

            case EOC:
              eoc = 1;          
              break;
              
          }

          if(eoc){
            close(fd_input);
            close(fd_output);
            break;
          }
        
        }

}