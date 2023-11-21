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
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS; // delay that will not be negative
  if (argc > 1) {
    char *endptr;
    unsigned long int delay = strtoul(argv[1], &endptr, 10); // delay that will not be negative

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  dir = opendir(argv[2]); // opens the directory and stores the result in the variable

  if (dir == NULL){
        const char *errorMessage = "Error opening the directory\n"; // my error message
        write(2, errorMessage, strlen(errorMessage)); // 2 to write in the error output, the message and its size
        write(2, strerror(errno), strlen(strerror(errno))); // system error message
        return 1;
    }
  
  int fileDescriptor; // the result of a file operation

  while ((entry = readdir(dir)) != NULL) { // while there are directories to be read
    if (strstr(entry->d_name, ".jobs") != NULL) { // If the directory is regular and it contains .jobs files
      char filePath[MAX_PATH_SIZE]; // Max size for a path
      snprintf(filePath, MAX_PATH_SIZE, "%s/%s", argv[2], entry->d_name); // Concatenates the directory path wit the file name

      fileDescriptor = open(filePath, O_RDONLY); // Opens the file to read only mode

      if (fileDescriptor == -1) {
        const char *errorMessage = "Error opening the file\n"; 
        write(2, errorMessage, strlen(errorMessage)); 
        write(2, strerror(errno), strlen(strerror(errno))); 
        closedir(dir);
        return 1;
      }

    while (1) {
      unsigned int event_id, delay;
      size_t num_rows, num_columns, num_coords;
      size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

      fflush(stdout);

      switch (get_next(fileDescriptor)) {
        case CMD_CREATE:
          if (parse_create(fileDescriptor, &event_id, &num_rows, &num_columns) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (ems_create(event_id, num_rows, num_columns)) {
            fprintf(stderr, "Failed to create event\n");
          }

          break;

        case CMD_RESERVE:
          num_coords = parse_reserve(fileDescriptor, MAX_RESERVATION_SIZE, &event_id, xs, ys);

          if (num_coords == 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (ems_reserve(event_id, num_coords, xs, ys)) {
            fprintf(stderr, "Failed to reserve seats\n");
          }

          break;

        case CMD_SHOW:
          if (parse_show(fileDescriptor, &event_id) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (ems_show(event_id)) {
            fprintf(stderr, "Failed to show event\n");
          }

          break;

        case CMD_LIST_EVENTS:
          if (ems_list_events()) {
            fprintf(stderr, "Failed to list events\n");
          }

          break;

        case CMD_WAIT:
          if (parse_wait(fileDescriptor, &delay, NULL) == -1) {  // thread_id is not implemented
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (delay > 0) {
            printf("Waiting...\n");
            ems_wait(delay);
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

        case CMD_BARRIER:  // Not implemented
        case CMD_EMPTY:
          break;

        case EOC:
          ems_terminate();
          closedir(dir);
          // add um break
          return 0;
        }
     }
    close(fileDescriptor);
    }
  }
  return 0;
}