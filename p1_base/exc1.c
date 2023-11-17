#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#define MAX_PATH_SIZE 256
#define BUFFER_SIZE 1024

int main(){
    DIR *dir; // ponteiro para uma struct de diretorio
    struct dirent *entry; // ponteiro para a entrada do diretorio
    const char *dirPath = "/home/luca/SO/projectSO/p1_base/jobs"; // path para o diretorio

    dir = opendir(dirPath); // abre o diretorio e guarda o resultado na variavel

    if (dir == NULL)
    {
        const char *errorMessage = "Erro abrindo o diretorio\n"; // minha mensagem de erro
        write(2, errorMessage, strlen(errorMessage)); // 2 para escrever no output de erro, mensagem a escrever e tamanho
        write(2, strerror(errno), strlen(strerror(errno))); // mensagem de erro do sistema
        return 1;
    }

    int fileDescriptor; // resultado de uma operacao em ficheiros
    char buffer[BUFFER_SIZE]; // tamanho do buffer
    
    while ((entry = readdir(dir)) != NULL) { // enquanto a leitura do diretorio nao for nula 
        if (strstr(entry->d_name, ".jobs") != NULL) { // Se o tipo do diretorio for regular e a entrada conter .jobs
            char filePath[MAX_PATH_SIZE];
            snprintf(filePath, MAX_PATH_SIZE, "%s/%s", dirPath, entry->d_name);

            fileDescriptor = open(filePath, O_RDONLY); 

            if (fileDescriptor == -1) {
                const char *errorMessage = "Erro abrindo ficheiro ";
                write(2, errorMessage, strlen(errorMessage)); // 2 para escrever no output de erro, mensagem a escrever e tamanho
                write(2, strerror(errno), strlen(strerror(errno))); // mensagem de erro do sistema
                closedir(dir);
                return 1;
        }

            ssize_t bytesRead;
            while ((bytesRead = read(fileDescriptor, buffer, sizeof(buffer))) > 0) { // Enquanto houver bytes para ler
                if (write(1, buffer, bytesRead) == -1) {
                    const char *errorMessage = "Erro na escrita ";
                    write(2, errorMessage, strlen(errorMessage)); // 2 para escrever no output de erro, mensagem a escrever e tamanho
                    write(2, strerror(errno), strlen(strerror(errno))); // mensagem de erro do sistema
                    close(fileDescriptor);
                    closedir(dir);
                    return 1;
                }
            }

            if (bytesRead == -1) { // Se houver erros na leitura do ficheiro
                const char *errorMessage = "Erro na leitura do ficheiro ";
                write(2, errorMessage, strlen(errorMessage)); // 2 para escrever no output de erro, mensagem a escrever e tamanho
                write(2, strerror(errno), strlen(strerror(errno))); // mensagem de erro do sistema
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