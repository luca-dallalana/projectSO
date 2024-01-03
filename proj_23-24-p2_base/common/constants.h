#define MAX_RESERVATION_SIZE 256
#define STATE_ACCESS_DELAY_US 500000  // 500ms
#define MAX_JOB_FILE_NAME_SIZE 256
#define MAX_SESSION_COUNT 8
#define MAX_PIPE_PATH_NAME 40
#define SETUP_REQUEST_LEN  sizeof(int) + 2 * MAX_PIPE_PATH_NAME
#define QUIT_REQUEST_LEN sizeof(int)
#define CREATE_REQUEST_LEN sizeof(int) + sizeof(int) + sizeof(size_t) + sizeof(size_t)
#define RESERVE_REQUEST_LEN sizeof(int) + sizeof(int) + sizeof(size_t) + 2 * num_seats * sizeof(size_t)
#define SHOW_REQUEST_LEN sizeof(int) + sizeof(unsigned int)
