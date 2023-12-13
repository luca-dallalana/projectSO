#ifndef EMS_OPERATIONS_H
#define EMS_OPERATIONS_H


#include <stddef.h>



struct CreateArgs{
    unsigned int event_id;
    size_t num_rows;
    size_t num_cols;
};

struct ReserveArgs{
    unsigned int event_id;
    size_t num_seats;
    size_t* xs;
    size_t* ys;
};

struct ShowArgs{
    unsigned int event_id;
    const int output_fd;
};

void compute_file(int fd_input, int fd_output, unsigned int delay, int max_thread);

void write_to_file(const char *message,const int output_fd);

char* parse_file_name( char *fileName);

/// Initializes the EMS state.
/// @param delay_ms State access delay in milliseconds.
/// @return 0 if the EMS state was initialized successfully, 1 otherwise.
int ems_init(unsigned int delay_ms);

/// Destroys the EMS state.
int ems_terminate();

/// Creates a new event with the given id and dimensions.
/// @param event_id Id of the event to be created.
/// @param num_rows Number of rows of the event to be created.
/// @param num_cols Number of columns of the event to be created.
/// @return 0 if the event was created successfully, 1 otherwise.
void* ems_create(void* args);

/// Creates a new reservation for the given event.
/// @param event_id Id of the event to create a reservation for.
/// @param num_seats Number of seats to reserve.
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
/// @return 0 if the reservation was created successfully, 1 otherwise.
void* ems_reserve(void* args);

/// Prints the given event.
/// @param event_id Id of the event to print.
/// @return 0 if the event was printed successfully, 1 otherwise.
void* ems_show(void* args);

/// Prints all the events.
/// @return 0 if the events were printed successfully, 1 otherwise.
void* ems_list_events(void* output_fd);

/// Waits for a given amount of time.
/// @param delay_us Delay in milliseconds.
void* ems_wait(void* delay_ms);

void add_tid(pthread_t t_id[], pthread_t id, int max_thread);

#endif  // EMS_OPERATIONS_H
