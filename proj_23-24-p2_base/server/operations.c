#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "common/io.h"
#include "eventlist.h"
#include "common/constants.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_us = 0;


/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @param from First node to be searched.
/// @param to Last node to be searched.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id, struct ListNode* from, struct ListNode* to) {
  struct timespec delay = {0, state_access_delay_us * 1000};
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id, from, to);
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_us) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_us = delay_us;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  free_list(event_list);
  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  if (get_event_with_delay(event_id, event_list->head, event_list->tail) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  if (pthread_mutex_init(&event->mutex, NULL) != 0) {
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }
  event->data = calloc(num_rows * num_cols, sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event->data);
    free(event);
    return 1;
  }

  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    return 1;
  }

  for (size_t i = 0; i < num_seats; i++) {
    if (xs[i] <= 0 || xs[i] > event->rows || ys[i] <= 0 || ys[i] > event->cols) {
      fprintf(stderr, "Seat out of bounds\n");
      pthread_mutex_unlock(&event->mutex);
      return 1;
    }
  }

  for (size_t i = 0; i < event->rows * event->cols; i++) {
    for (size_t j = 0; j < num_seats; j++) {
      if (seat_index(event, xs[j], ys[j]) != i) {
        continue;
      }

      if (event->data[i] != 0) {
        fprintf(stderr, "Seat already reserved\n");
        pthread_mutex_unlock(&event->mutex);
        return 1;
      }

      break;
    }
  }

  unsigned int reservation_id = ++event->reservations;

  for (size_t i = 0; i < num_seats; i++) {
    event->data[seat_index(event, xs[i], ys[i])] = reservation_id;
  }

  pthread_mutex_unlock(&event->mutex);
  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    return 1;
  }

  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      char buffer[16];
      sprintf(buffer, "%u", event->data[seat_index(event, i, j)]);

      if (print_str(out_fd, buffer)) {
        perror("Error writing to file descriptor");
        pthread_mutex_unlock(&event->mutex);
        return 1;
      }

      if (j < event->cols) {
        if (print_str(out_fd, " ")) {
          perror("Error writing to file descriptor");
          pthread_mutex_unlock(&event->mutex);
          return 1;
        }
      }
    }

    if (print_str(out_fd, "\n")) {
      perror("Error writing to file descriptor");
      pthread_mutex_unlock(&event->mutex);
      return 1;
    }
  }

  pthread_mutex_unlock(&event->mutex);
  return 0;
}

int ems_list_events(int out_fd) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct ListNode* to = event_list->tail;
  struct ListNode* current = event_list->head;

  if (current == NULL) {
    char buff[] = "No events\n";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      pthread_rwlock_unlock(&event_list->rwl);
      return 1;
    }

    pthread_rwlock_unlock(&event_list->rwl);
    return 0;
  }

  while (1) {
    char buff[] = "Event: ";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      pthread_rwlock_unlock(&event_list->rwl);
      return 1;
    }

    char id[16];
    sprintf(id, "%u\n", (current->event)->id);
    if (print_str(out_fd, id)) {
      perror("Error writing to file descriptor");
      pthread_rwlock_unlock(&event_list->rwl);
      return 1;
    }

    if (current == to) {
      break;
    }

    current = current->next;
  }

  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}



int process_request(int code, int request_pipe, int response_pipe){
  char *response_message;
  unsigned int event_id;

  switch (code){

    // create
    case 3:
      size_t num_rows;
      size_t num_cols;

      if(read(request_pipe,&event_id,sizeof(unsigned int)) <= 0 || 
      read(request_pipe,&num_rows,sizeof(size_t)) <= 0 || 
      read(request_pipe,&num_cols,sizeof(size_t)) <= 0) return 1;


      int create_value = ems_create(event_id,num_rows,num_cols);
      if(write(response_pipe,&create_value,sizeof(int)) <= 0) return 1;

      return 0;
    
    // reserve
    case 4:
      

      size_t num_seats;
      size_t* xs;
      size_t* ys;

      if (read(request_pipe, &event_id, sizeof(unsigned int)) <= 0 || 
      read(request_pipe, &num_seats, sizeof(size_t)) <= 0)  
      {
        return 1;
      }

      // Allocate memory for xs and ys
      xs = malloc(num_seats * sizeof(size_t));
      ys = malloc(num_seats * sizeof(size_t));

      // Check if memory allocation is successful
      if (xs == NULL || ys == NULL) {

        free(xs);
        free(ys);
        return 1;
      }

      // Read data into allocated memory
      if (read(request_pipe, xs, num_seats * sizeof(size_t)) <= 0 || 
          read(request_pipe, ys, num_seats * sizeof(size_t)) <= 0){

        free(xs);
        free(ys);
        return 1;
      }

      int reserve_value = ems_reserve(event_id,num_seats,xs,ys);

      free(xs);
      free(ys);
      
      if(write(response_pipe,&reserve_value,sizeof(int)) <= 0) return 1;


      return 0;

    // show
    case 5:


      if (read(request_pipe, &event_id, sizeof(unsigned int)) <= 0) {
        return 1;
      }

      int show_value = 0;
      struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

      if (event == NULL) {
          show_value = 1;
      }

      size_t rows = event->rows;
      size_t cols = event->cols;
      size_t event_size = rows * cols;

    
      size_t response_size = sizeof(int) + sizeof(size_t) + sizeof(size_t) + event_size * sizeof(unsigned int);

      response_message = malloc(response_size);

      // Check if memory allocation is successful
      if (response_message == NULL) {
          return 1;
      }

      memcpy(response_message, &show_value, sizeof(int));
      memcpy(response_message + sizeof(int), &rows, sizeof(size_t));
      memcpy(response_message + sizeof(int) + sizeof(size_t), &cols, sizeof(size_t));
      memcpy(response_message + sizeof(int) + sizeof(size_t) + sizeof(size_t), event->data, event_size);

      // Write the response message to the pipe
      if (write(response_pipe, response_message, response_size) <= 0) {
          free(response_message);
          return 1;
      }

      // Clean up allocated memory
      free(response_message);
      return 0;


    // list
    case 6:
      int list_value = 0;
      size_t n_events = 0;

      if (event_list == NULL) {
          return 1;
      }

      struct ListNode* to = event_list->tail;
      struct ListNode* current = event_list->head;

      unsigned int* id = NULL;
      while (1) {
          id = realloc(id, (n_events + 1) * sizeof(unsigned int));

          if (id == NULL) {
              fprintf(stderr, "Memory allocation failed\n");
              return 1;
          }

          id[n_events] = (current->event)->id;
          n_events++;

          if (current == to) {
              break;
          }

          current = current->next;
      }

      response_message = malloc(sizeof(int) + sizeof(int) + n_events * sizeof(unsigned int));

      if (response_message == NULL) {
          // Handle memory allocation failure
          free(id);
          return 1;
      }

      memcpy(response_message, &list_value, sizeof(int));
      memcpy(response_message + sizeof(int), &n_events, sizeof(int));
      memcpy(response_message + sizeof(int) + sizeof(int), id, n_events * sizeof(unsigned int));

      // Use the correct size when writing to the response pipe
      if (write(response_pipe, response_message, sizeof(int) + sizeof(int) + n_events * sizeof(unsigned int)) <= 0) {
          free(response_message);
          free(id);
          return 1;
      }


      // Clean up allocated memory
      free(response_message);
      free(id);

      return 0;

  }
  return 1;
}


