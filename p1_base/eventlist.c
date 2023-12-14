#include "eventlist.h"

#include <stdlib.h>

struct EventList* create_list() { // constructor that initializes an event list
  struct EventList* list = (struct EventList*)malloc(sizeof(struct EventList)); // assigns the size of an struct
  if (!list) return NULL;
  list->head = NULL;
  list->tail = NULL;
  return list;
}

int append_to_list(struct EventList* list, struct Event* event) {

  if (!list) return 1;

  pthread_rwlock_wrlock(&list -> list_lock_rw);
  struct ListNode* new_node = (struct ListNode*)malloc(sizeof(struct ListNode)); // node that will be added
  if (!new_node){
    pthread_rwlock_unlock(&list -> list_lock_rw); 
    return 1;
  } 

  new_node->event = event; // assigns the event to the new node created
  new_node->next = NULL; // assigns the next node as NULL

  if (list->head == NULL) { // if the given list is empty, assign the head and the tail since its the same
    list->head = new_node;
    list->tail = new_node;
  } else { // if its not empty, change the next of the last to point to the new node and add it
    list->tail->next = new_node;
    list->tail = new_node;
  }
  pthread_rwlock_unlock(&list -> list_lock_rw);
  return 0;
}

static void free_event(struct Event* event) {
  if (!event) return;

  free(event->data); // frees the space occupied by the matrix
  free(event);
}

void free_list(struct EventList* list) { 
  if (!list) return;

  struct ListNode* current = list->head;
  while (current) { // while there are nodes in the event list to be freed
    struct ListNode* temp = current; // assign a temporary variable
    current = current->next; // next node to free

    free_event(temp->event);
    free(temp);
  }

  free(list);
}

struct Event* get_event(struct EventList* list, unsigned int event_id) {
  if (!list) return NULL;

  pthread_rwlock_rdlock(&list -> list_lock_rw);
  struct ListNode* current = list->head; // access the first node of the list
  while (current) {
    struct Event* event = current->event; 
    pthread_rwlock_rdlock(&event -> event_lock_rw);
    if (event->id == event_id) {
      pthread_rwlock_unlock(&event -> event_lock_rw);
      pthread_rwlock_unlock(&list -> list_lock_rw);
      return event;
    }
    pthread_rwlock_unlock(&event -> event_lock_rw);
    current = current->next;
  }
  pthread_rwlock_unlock(&list -> list_lock_rw);
  return NULL;
}
