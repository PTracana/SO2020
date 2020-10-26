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

/*global variables that are used when initializing the program:
tecnicofs inputfile outputfile maxThreads synchstrategy*/

char* inputFilename = NULL;     //file from where you extract data for the file system
char* outputFilename = NULL;    //file to which you write the file system's output
int maxThreads = 0;             //maximum number of threads is stored here

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;

static void arguments(int argc, char* const argv[]) {   //this function parses the program's variables
    if(argc != 4) {                                     //the function only succeeds if you have exactly 5 arguments and if their typings are correct
        fprintf(stderr, "Wrong argument usage\n");
        exit(EXIT_FAILURE);
    }
    inputFilename = argv[1];    
    outputFilename = argv[2];   
    maxThreads = atoi(argv[3]);  
        
    if(maxThreads <= 0) {   //there has to be a number of threads greater than 0
        fprintf(stderr, "Please use a valid number of threads\n");
        exit(EXIT_FAILURE);
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

void init_lock(pthread_rwlock_t* lock) {      //initializes the mutex/rw lock
    if(pthread_rwlock_init(&lock, NULL) != 0) {
        fprintf(stderr, "Couldn't initialize rwlock\n");
        exit(EXIT_FAILURE);
    }
}

void readlock(pthread_rwlock_t* lock) {       //prevents other threads from reading from the locked content
    if(pthread_rwlock_rdlock(&lock) != 0) {
        fprintf(stderr, "Couldn't lock rwlock\n");
        exit(EXIT_FAILURE);
    }
}

void writelock(pthread_rwlock_t* lock) {      //prevents other threads from writing to the locked content
    if(pthread_rwlock_wrlock(&lock) != 0) {
        fprintf(stderr, "Couldn't lock rwlock\n");
        exit(EXIT_FAILURE);
    }
}

void unlock(pthread_rwlock_t* lock) {     //unlocks the mutex/rw lock
    if(pthread_rwlock_unlock(&lock) != 0) {
        fprintf(stderr, "Couldn't unlock rwlock\n");
        exit(EXIT_FAILURE);
    }
}
    

void processInput(){
    FILE* inputFile;
    inputFile = fopen(inputFilename, "r");  //input file is opened for reading only
    if(!inputFile) {                        //the program can't run without an input file
        fprintf(stderr, "Input file not found\n");
        exit(EXIT_FAILURE);
    }
    char line[MAX_INPUT_SIZE];

    /* break loop with ^Z or ^D */
    while (fgets(line, sizeof(line)/sizeof(char), inputFile)) { //the file is parsed line per line
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

FILE* openOutput() {        //the output file is opened for writing only
    FILE* outputFile = fopen(outputFilename, "w");
    if(!outputFile) {
        fprintf(stderr, "Could not open/create requested output file");
        exit(EXIT_FAILURE);
    }
    return outputFile;
}

/* 
applyCommands vai ser separado em 2: a primeira parte vai ter a extração do comando e criação de threads.
Um array de comandos que estão a ser usados por threads permite-nos detetar se o próximo comando é compatível.
Vamos ter também um int com o numero de comandos que estão a ser usados e um malloc para as threads = maxThreads.
A compatibilidade é verificada através de uma série de condições especificadas no enunciado do projeto.
No caso de haver compatibilidade, o comando é adicionado ao vetor de comandos atuais e é criada uma thread com o verdadeiro applyCommands.
No caso de não o ser, as threads atuais fazem join, o array de comandos usados é esvaziado e cria-se uma thread para o novo comando.
*/

void* applyCommands() {
    while (numberCommands > 0) {
        const char* command = removeCommand();
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
            case 'c':       //in case we want to create something, a writelock prevents other threads from doing the same before this one
                switch (type) {
                    case 'f':
                        printf("Create file: %s\n", name);
                        create(name, T_FILE);
                        break;
                    case 'd':
                        printf("Create directory: %s\n", name);
                        create(name, T_DIRECTORY);
                        break;
                    default:
                        fprintf(stderr, "Error: invalid node type\n");
                        exit(EXIT_FAILURE);
                }
                break;
            case 'l':       //in case we want to lookup something, a readlock prevents other threads from doing the same before this one
                searchResult = lookup(name);
                if (searchResult >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                break;
            case 'd':       //in case we want to delete something, a writelock prevents other threads from doing the same before this one
                printf("Delete: %s\n", name);
                delete(name);
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return NULL;
}

/*void runThreads(){      //this function is now useless
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
    else {      //if you chose the nosync strategy, the program will only have one thread and thus doesn't need to create more
        applyCommands();
    }
}*/

int main(int argc, char* argv[]) {
    double elapsedTime;     //number of seconds the program ran for
    struct timeval startTime;
    struct timeval stopTime;
    FILE* outputFile;
    arguments(argc, argv);
    outputFile = openOutput();
    /* init filesystem */
    init_fs();
    if(gettimeofday(&startTime, NULL) != 0) {   //the program obtains its starting time from the pc's clock
        fprintf(stderr, "Couldn't get time\n");
        exit(EXIT_FAILURE);
    }
    /*mutex_init(&queue_lock);
    init_lock(&fs_lock);*/

    /* process input and print tree */
    processInput();
    applyCommands();
    if(gettimeofday(&stopTime, NULL) != 0) {    //the program obtains its stopping time from the pc's clock
        fprintf(stderr, "Couldn't get time\n");
        exit(EXIT_FAILURE);
    }
    elapsedTime = (double) (stopTime.tv_usec - startTime.tv_usec) / CLOCKS_PER_SEC;
    printf("The program ended in %.4f seconds.\n", elapsedTime);
    print_tecnicofs_tree(outputFile);
    fclose(outputFile);

    /* release allocated memory */
    destroy_fs();
    exit(EXIT_SUCCESS);
}
