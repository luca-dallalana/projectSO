#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#define MAX_PATH_SIZE 256
#define BUFFER_SIZE 1024

int main(){
    DIR *dir; // pointer for a directory struct 
    struct dirent *entry; // pointer for the entry of a directory 
    const char *dirPath = "/home/luca/SO/projectSO/p1_base/jobs"; // path to the directory

    dir = opendir(dirPath); // opens the directory and stores the result in the variable

    if (dir == NULL)
    {
        const char *errorMessage = "Error opening the directory\n"; // my error message
        write(2, errorMessage, strlen(errorMessage)); // 2 to write in the error output, the message and its size
        write(2, strerror(errno), strlen(strerror(errno))); // system error message
        return 1;
    }

    int fileDescriptor; // the result of a file operation
    char buffer[BUFFER_SIZE]; 
    
    while ((entry = readdir(dir)) != NULL) { // while there are directories to be read
        if (strstr(entry->d_name, ".jobs") != NULL) { // If the directory is regular and it contains .jobs files
            char filePath[MAX_PATH_SIZE]; // Max size for a path
            snprintf(filePath, MAX_PATH_SIZE, "%s/%s", dirPath, entry->d_name); // Concatenates the directory path wit the file name

            fileDescriptor = open(filePath, O_RDONLY); // Opens the file to read only mode

            if (fileDescriptor == -1) {
                const char *errorMessage = "Error opening the file\n"; 
                write(2, errorMessage, strlen(errorMessage)); 
                write(2, strerror(errno), strlen(strerror(errno))); 
                closedir(dir);
                return 1;
        }

            ssize_t bytesRead;
            while ((bytesRead = read(fileDescriptor, buffer, sizeof(buffer))) > 0) { // while there bytes to be read
                if (write(1, buffer, bytesRead) == -1) { // If there is an error in the writing process
                    const char *errorMessage = "Error writing in the file\n";
                    write(2, errorMessage, strlen(errorMessage)); 
                    write(2, strerror(errno), strlen(strerror(errno))); 
                    close(fileDescriptor);
                    closedir(dir);
                    return 1;
                }
            }

            if (bytesRead == -1) { // If there are errors reading the file
                const char *errorMessage = "Error reading the file\n";
                write(2, errorMessage, strlen(errorMessage)); 
                write(2, strerror(errno), strlen(strerror(errno))); 
                close(fileDescriptor);
                closedir(dir);
                return 1;
            }
            close(fileDescriptor);
        }         
    }
    closedir(dir);
    return 0;
}