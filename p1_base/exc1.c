#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define BUFFER_SIZE 1024

int main(){
    int fileDescriptor; // resultado de uma operacao em ficheiros
    char buffer[BUFFER_SIZE]; // tamanho do buffer
    const char *filePath = "/home/luca/SO/p1_base/jobs"; // path para o ficheiro

    fileDescriptor = open(filePath, O_RDONLY); // abre o file e guarda o resultado da operacao na variavel

    if (fileDescriptor == -1)
    {
        const char *errorMessage = "Erro abrindo o diretorio\n";
        write(2, errorMessage, strlen(errorMessage)); // 2 para escrever no output de erro, mensagem a escrever e tamanho
        return 1;
    }

    ssize_t bytesRead; // numero de bytes lidos
    while ((bytesRead = read(fileDescriptor, buffer, sizeof(buffer))) > 0) { // enquanto nao ocorrerem erros na leitura 
        if (write(1, buffer, bytesRead) == -1) { // Se ocorrerem erros na escrita
            const char *errorMessage = "Erro na escrita\n";
            write(2, errorMessage, strlen(errorMessage));
            close(fileDescriptor); // fecha o ficheiro
            return 1; 
        }
    }

    if (bytesRead == -1)
    {
        const char *errorMessage = "Erro na leitura\n";
        write(2, errorMessage, strlen(errorMessage));
        close(fileDescriptor); // fecha o ficheiro
        return 1; 
    }
    return 0;
}