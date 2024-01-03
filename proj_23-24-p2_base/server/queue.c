#include "queue.h"
#include "common/constants.h"

int create_queue(struct Queue* queue){
    void** pc_buffer = malloc(MAX_SESSION_COUNT * sizeof(void*));

    if(pc_buffer == NULL){
        printf("Failed to allocate memory for buffer\n");
        return 1;
    }

    queue->queue_buffer = pc_buffer;
    queue->queue_size = 0;

    if(pthread_mutex_init(&queue->size_lock,NULL) != 0 || 
    pthread_mutex_init(&queue->add_to_queue_lock,NULL) != 0 || 
    pthread_mutex_init(&queue->remove_from_queue_lock,NULL) != 0){

        free(pc_buffer);
        return 1;
    }

    if(pthread_cond_init(&queue->add_to_queue_condvar,NULL) != 0 || 
    pthread_cond_init(&queue->remove_from_queue_condvar,NULL) != 0){

        free(pc_buffer);
        return 1;
    }
    return 0;
}

int add_element(struct Queue* queue, void* element){

    pthread_mutex_lock(&queue->add_to_queue_lock);
    pthread_mutex_lock(&queue->size_lock);

    while(&queue->size_lock == MAX_SESSION_COUNT){
        pthread_mutex_unlock(&queue->size_lock);
        pthread_cond_wait(&queue->add_to_queue_condvar,&queue->add_to_queue_lock);
        pthread_mutex_lock(&queue->size_lock);
    }



    pthread_mutex_unlock(&queue->add_to_queue_lock);

    // Find an empty spot in the queue_buffer

    for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
        if (queue->queue_buffer[i] == NULL) {
            // Add the element to the found index
            queue->queue_buffer[i] = element;
            break;
        }
    }


    ++queue->queue_size;



    // Signal that an element has been added
    pthread_cond_broadcast(&queue->remove_from_queue_condvar);
    pthread_mutex_unlock(&queue->size_lock);

    return 0;
}

void* remove_element(struct Queue* queue){
    pthread_mutex_lock(&queue->remove_from_queue_lock);
    pthread_mutex_lock(&queue->size_lock);

    while(&queue->queue_size == 0){
        pthread_mutex_unlock(&queue->queue_size);
        pthread_cond_wait(&queue->remove_from_queue_condvar,&queue->remove_from_queue_lock);
        pthread_mutex_lock(&queue->size_lock);
    }

    pthread_mutex_unlock(&queue->remove_from_queue_lock);

    for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
        if (queue->queue_buffer[i] != NULL) {
            
            --queue->queue_size;
            pthread_cond_broadcast(&queue->add_to_queue_condvar);
            pthread_mutex_unlock(&queue->size_lock);
            return queue->queue_buffer[i];
            
        }
    }

    return NULL;

}

int destroy_queue(struct Queue* queue){

    free(queue -> queue_buffer);

    pthread_mutex_destroy(&queue->size_lock);

    pthread_mutex_destroy(&queue->add_to_queue_lock);
    pthread_cond_destroy(&queue->add_to_queue_condvar);

    pthread_mutex_destroy(&queue->remove_from_queue_lock);
    pthread_cond_destroy(&queue->remove_from_queue_condvar);

    return 0;
}