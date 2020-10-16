#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <sys/time.h>
#include <pthread.h>

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100
#define NOSYNC 0
#define MUTEX 1
#define RWLOCK 2

typedef struct lock {
    pthread_mutex_t mutex;
    pthread_rwlock_t rwlock;
} lock_t;

/*variaveis globais que guardam os parametros com que se inicializa o programa:
tecnicofs inputfile outputfile maxThreads synchstrategy*/

char* inputFilename = NULL;
char* outputFilename = NULL;
int maxThreads = 0;
int synchstrategy = NOSYNC;

pthread_mutex_t queue_lock;
lock_t fs_lock;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;

static void arguments(int argc, char* const argv[]) { //funcao que faz parse aos argumentos do tecnicofs
    if(argc != 5) { //a funcao so funciona se tiver os 5 argumentos necessarios para tal
        fprintf(stderr, "Wrong argument usage\n");
    }
    inputFilename = argv[1];    //ficheiro de onde se le
    outputFilename = argv[2];   //ficheiro para onde se escreve
    maxThreads = atoi(argv[3]);  //numero de tarefas
    if(strcmp(argv[4], "mutex")) {  //estrategia de sincronizacao (nosync, mutex ou rwlock)
        synchstrategy = MUTEX;
    }
    else if(strcmp(argv[4], "rwlock")) {
        synchstrategy = RWLOCK;
    }
        
    if(maxThreads <= 0) { //tem de existir um numero de threads valido
        fprintf(stderr, "Please use a valid number of threads\n");
    }
    else if(strcmp(argv[4], "nosync") != 0 && strcmp(argv[4], "mutex") != 0 && strcmp(argv[4], "rwlock") != 0) { //so existem tres metodos possiveis
        fprintf(stderr, "Please use a valid sync method from the following:\n mutex, rwlock, nosync\n");
    }
    else if(strcmp(argv[4], "nosync") == 0 && maxThreads != 1) {  //o metodo nosync so pode ter uma thread
        fprintf(stderr, "When using nosync the number of threads must be 1\n");
    }
}

int insertCommand(char* data) {
    if(numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        return 1;
    }
    return 0;
}

char* removeCommand() {
    if(numberCommands > 0){
        numberCommands--;
        return inputCommands[headQueue++];  
    }
    return NULL;
}

void errorParse(){
    fprintf(stderr, "Error: invalid command\n");
    exit(EXIT_FAILURE);
}

void mutex_lock(pthread_mutex_t* mutex) {
    if(pthread_mutex_lock(mutex) != 0) {
        fprintf(stderr, "Couldn't lock mutex\n");
        exit(EXIT_FAILURE);
    }
}

void mutex_unlock(pthread_mutex_t* mutex) {
    if(pthread_mutex_unlock(mutex) != 0) {
        fprintf(stderr, "Couldn't unlock mutex\n");
        exit(EXIT_FAILURE);
    }
}

void mutex_init(pthread_mutex_t* mutex) {
    if(pthread_mutex_init(mutex, NULL) != 0) {
        fprintf(stderr, "Couldn't initialize mutex\n");
        exit(EXIT_FAILURE);
    }
}

void init_lock(lock_t* lock) {
    switch(synchstrategy) {
        case MUTEX:
            mutex_init(&(lock->mutex));
            break;
        case RWLOCK:
            if(pthread_rwlock_init(&(lock->rwlock), NULL) != 0) {
                fprintf(stderr, "Couldn't initialize rwlock\n");
                exit(EXIT_FAILURE);
            }
            break;
    }
}

void readlock(lock_t* lock) {
    switch(synchstrategy) {
        case MUTEX:
            mutex_lock(&(lock->mutex));
            break;
        case RWLOCK:
            if(pthread_rwlock_rdlock(&(lock->rwlock)) != 0) {
                fprintf(stderr, "Couldn't lock rwlock\n");
                exit(EXIT_FAILURE);
            }
            break;
    }
}

void writelock(lock_t* lock) {
    switch(synchstrategy) {
        case MUTEX:
            mutex_lock(&(lock->mutex));
            break;
        case RWLOCK:
            if(pthread_rwlock_wrlock(&(lock->rwlock)) != 0) {
                fprintf(stderr, "Couldn't lock rwlock\n");
                exit(EXIT_FAILURE);
            }
            break;
    }
}

void unlock(lock_t* lock) {
    switch(synchstrategy) {
        case MUTEX:
            mutex_unlock(&(lock->mutex));
            break;
        case RWLOCK:
            if(pthread_rwlock_unlock(&(lock->rwlock)) != 0) {
                fprintf(stderr, "Couldn't unlock rwlock\n");
                exit(EXIT_FAILURE);
            }
            break;
    }
}

void processInput(){
    FILE* inputFile;
    inputFile = fopen(inputFilename, "r"); //leitura do ficheiro de input
    if(!inputFile) {
        fprintf(stderr, "Input file not found\n");
    }
    char line[MAX_INPUT_SIZE];

    /* break loop with ^Z or ^D */
    while (fgets(line, sizeof(line)/sizeof(char), inputFile)) { //parsing do ficheiro linha a linha
        char token, type;
        char name[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s %c", &token, name, &type);

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
                if(numTokens != 3)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'l':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'd':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case '#':
                break;
            
            default: { /* error */
                errorParse();
            }
        }
    }
    fclose(inputFile);
}

FILE* openOutput() {
    FILE* outputFile = fopen(outputFilename, "w");
    if(!outputFile) {
        fprintf(stderr, "Could not open/create requested output file");
    }
    return outputFile;
}

void* applyCommands() {
    while (numberCommands > 0) {
        mutex_lock(&queue_lock);
        const char* command = removeCommand();
        mutex_unlock(&queue_lock);
        if (command == NULL){
            continue;
        }
        char token, type;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s %c", &token, name, &type);
        if (numTokens < 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }

        int searchResult;
        switch (token) {
            case 'c':
                switch (type) {
                    case 'f':
                        writelock(&fs_lock);
                        printf("Create file: %s\n", name);
                        create(name, T_FILE);
                        unlock(&fs_lock);
                        break;
                    case 'd':
                        writelock(&fs_lock);
                        printf("Create directory: %s\n", name);
                        create(name, T_DIRECTORY);
                        unlock(&fs_lock);
                        break;
                    default:
                        fprintf(stderr, "Error: invalid node type\n");
                        exit(EXIT_FAILURE);
                }
                break;
            case 'l':
                readlock(&fs_lock);
                searchResult = lookup(name);
                unlock(&fs_lock);
                if (searchResult >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                break;
            case 'd':
                writelock(&fs_lock);
                printf("Delete: %s\n", name);
                delete(name);
                unlock(&fs_lock);
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return NULL;
}

void runThreads(){
    if(synchstrategy != NOSYNC) {
        pthread_t* thread_list = malloc(maxThreads * sizeof(pthread_t));
        for(int i = 0; i < maxThreads; i++) {
            if(pthread_create(&thread_list[i], NULL, applyCommands, NULL) != 0) {
                printf("Couldn't create thread\n");
                exit(EXIT_FAILURE);
            }
        }
        for(int i = 0; i < maxThreads; i++) {
            if(pthread_join(thread_list[i], NULL) != 0) {
                printf("Couldn't join thread\n");
                exit(EXIT_FAILURE);
            }
        }
        free(thread_list);
    }
    else {
        applyCommands();
    }
}

int main(int argc, char* argv[]) {
    FILE* outputFile;
    arguments(argc, argv); //o programa obtem os parametros que foram introduzidos na linha de comandos quando este foi invocado
    outputFile = openOutput();
    /* init filesystem */
    init_fs();
    mutex_init(&queue_lock);
    init_lock(&fs_lock);

    /* process input and print tree */
    processInput();
    applyCommands();
    print_tecnicofs_tree(outputFile);
    fclose(outputFile);

    /* release allocated memory */
    destroy_fs();
    exit(EXIT_SUCCESS);
}
