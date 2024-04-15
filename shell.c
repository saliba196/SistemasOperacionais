#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_ARGS 64
#define MAX_COMANDOS 16
#define MAX_TAMANHO_COMANDO 256

void executarComandoUnico(char *comando);
void executarComandoEncadeado(char *comandos[], int numComandos);
void executarComandoCondicional(char *comandos[], int numComandos, char *comandoFinal);
void executarComandoBackground(char *comando);

int main() {
    char entrada[MAX_TAMANHO_COMANDO];
    
    while (1) {
        printf("> ");
        fgets(entrada, sizeof(entrada), stdin);
        
        // Remove a nova linha do final da entrada
        entrada[strcspn(entrada, "\n")] = 0;
        
        // Verifica se o último caractere é '&', indicando um comando em background
        int background = 0;
        if (entrada[strlen(entrada) - 1] == '&') {
            background = 1;
            entrada[strlen(entrada) - 1] = '\0'; // Remove o '&'
        }
        
        // Verifica se há comandos condicionais (contém '||' ou '&&')
        if (strstr(entrada, "||") || strstr(entrada, "&&")) {
            char *comandos[MAX_COMANDOS];
            int numComandos = 0;
            
            // Divide a entrada em comandos separados
            char *todosComandosExcetoUltimo = strtok(entrada, "||");
            char *ultimoComando = strtok(NULL, "||");

            char *token = strtok(todosComandosExcetoUltimo, "&&");
            while (token != NULL) {
                comandos[numComandos++] = token;
                token = strtok(NULL, "&&");
            }
            
            executarComandoCondicional(comandos, numComandos, ultimoComando);
        } else {
            char *comandos[MAX_COMANDOS];
            int numComandos = 0;
            
            // Divide a entrada em comandos separados
            char *token = strtok(entrada, "|");
            while (token != NULL) {
                comandos[numComandos++] = token;
                token = strtok(NULL, "|");
            }
            
            if (numComandos == 1) {
                if (background) {
                    executarComandoBackground(comandos[0]);
                } else {
                    executarComandoUnico(comandos[0]);
                }
            } else {
                executarComandoEncadeado(comandos, numComandos);
            }
        }
    }
    
    return 0;
}

void executarComandoUnico(char *comando) {
    char *argumentos[MAX_ARGS];
    char *token;
    int i = 0;
    
    // Divide o comando em argumentos separados
    token = strtok(comando, " ");
    while (token != NULL) {
        argumentos[i++] = token;
        token = strtok(NULL, " ");
    }
    argumentos[i] = NULL;
    
    // Executa o comando
    pid_t pid = fork();
    if (pid == 0) {
        // Processo filho
        execvp(argumentos[0], argumentos);
        // Se execvp retornar, houve um erro
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Erro ao criar processo filho
        perror("fork");
    } else {
        // Processo pai
        waitpid(pid, NULL, 0);
    }
}

void executarComandoEncadeado(char *comandos[], int numComandos) {
    int pipes[numComandos - 1][2];
    int i;
    
    // Cria pipes para comunicação entre os comandos
    for (i = 0; i < numComandos - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }
    
    // Executa cada comando em um processo separado
    for (i = 0; i < numComandos; i++) {
        char *argumentos[MAX_ARGS];
        char *token;
        int j = 0;
        
        // Divide o comando em argumentos separados
        token = strtok(comandos[i], " ");
        while (token != NULL) {
            argumentos[j++] = token;
            token = strtok(NULL, " ");
        }
        argumentos[j] = NULL;
        
        pid_t pid = fork();
        if (pid == 0) {
            // Processo filho
            
            // Redireciona a entrada do primeiro comando
            if (i == 0) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Redireciona a saída do comando anterior para a entrada deste comando
            else if (i < numComandos - 1) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Redireciona a saída do último comando para stdout
            else {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            
            // Fecha todos os descritores de arquivo não necessários
            int k;
            for (k = 0; k < numComandos - 1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
            
            // Executa o comando
            execvp(argumentos[0], argumentos);
            // Se execvp retornar, houve um erro
            perror("execvp");
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            // Erro ao criar processo filho
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }
    
    // Fecha os descritores de arquivo nos pipes
    for (i = 0; i < numComandos - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Espera todos os processos filhos terminarem
    for (i = 0; i < numComandos; i++) {
        wait(NULL);
    }
}

void executarComandoCondicional(char *comandos[], int numComandos, char *comandoFinal) {
    for (int i = 0; i < numComandos; i++) {
      executarComandoUnico(comandos[i]);
    }

    if (numComandos == 1) {
      executarComandoUnico(comandoFinal);
    }
}

void executarComandoBackground(char *comando) {
    char *argumentos[MAX_ARGS];
    char *token;
    int i = 0;
    
    // Divide o comando em argumentos separados
    token = strtok(comando, " ");
    while (token != NULL) {
        argumentos[i++] = token;
        token = strtok(NULL, " ");
    }
    argumentos[i] = NULL;
    
    // Executa o comando em background
    pid_t pid = fork();
    if (pid == 0) {
        // Processo filho
        execvp(argumentos[0], argumentos);
        // Se execvp retornar, houve um erro
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Erro ao criar processo filho
        perror("fork");
    } else {
        // Processo pai
        printf("Comando em background iniciado com PID %d\n", pid);
    }
}
