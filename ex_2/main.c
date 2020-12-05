#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <sys/time.h>
#include <pthread.h>

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100

struct Buffer {
    char data[MAX_INPUT_SIZE];
    struct Buffer *next;
};

/*global variables that are used when initializing the program:
tecnicofs inputfile outputfile maxThreads synchstrategy*/

char* inputFilename = NULL;     //file from where you extract data for the file system
char* outputFilename = NULL;    //file to which you write the file system's output
int maxThreads = 0;             //maximum number of threads is stored here

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE]; //erase later
int numberCommands = 0;
int headQueue = 0;
struct Buffer buffer;
int isDone = 0; //0 = false, 1 = true

pthread_mutex_t commandLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t insertLock =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t removeLock =  PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t condInsert = PTHREAD_COND_INITIALIZER;
pthread_cond_t condRemove = PTHREAD_COND_INITIALIZER;

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
    pthread_mutex_lock(&commandLock);
    struct Buffer *temp;
    temp = &buffer;
    if(numberCommands != MAX_COMMANDS) {
        for(int i = 0; i < MAX_COMMANDS; i++) {
            if(temp->data[0] == '\0') {
                numberCommands++;
                strcpy(temp->data, data);
                break;
            }
            temp = temp->next;
        }
        pthread_mutex_unlock(&commandLock);
        pthread_cond_signal(&condInsert);
        return 1;
    }
    pthread_mutex_unlock(&commandLock);
    return 0;
}

char* removeCommand() {
    struct Buffer *temp;
    char *tempCommand = malloc(sizeof(char)*MAX_INPUT_SIZE);
    temp = &buffer;
    if(numberCommands > 0) {
        for(int i = 0; i < MAX_COMMANDS; i++) {
            if(temp->data[0] == '\0') {
                temp = temp->next;
            }
            else {
                strcpy(tempCommand, temp->data);
                temp->data[0] = '\0';
                break;
            }
        }
        numberCommands--;
        return tempCommand; 
    }
    return NULL;
}

void errorParse(){
    fprintf(stderr, "Error: invalid command\n");
    exit(EXIT_FAILURE);
}
    

void* processInput(){
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

        if(numberCommands == MAX_COMMANDS) {
            pthread_mutex_lock(&removeLock);
            pthread_cond_wait(&condRemove, &removeLock);
            pthread_mutex_unlock(&removeLock);
        }

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
                return NULL;
            
            case 'l':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return NULL;
            
            case 'd':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return NULL;

            case 'm':
                if(numTokens != 3)
                    errorParse();
                if(insertCommand(line))
                    break;
                return NULL;
            
            case '#':
                break;
            
            default: { /* error */
                errorParse();
            }
        }
    }
    isDone = 1;
    pthread_cond_signal(&condInsert);
    fclose(inputFile);
    return NULL;
}

FILE* openOutput() {        //the output file is opened for writing only
    FILE* outputFile = fopen(outputFilename, "w");
    if(!outputFile) {
        fprintf(stderr, "Could not open/create requested output file");
        exit(EXIT_FAILURE);
    }
    return outputFile;
}

void* applyCommands() {
    while(isDone == 0 || numberCommands != 0){
        if(numberCommands == 0) {
            pthread_mutex_lock(&insertLock);
            pthread_cond_wait(&condInsert, &insertLock);
            pthread_mutex_unlock(&insertLock);
        }
        pthread_mutex_lock(&commandLock);
        char* command = removeCommand();
        pthread_cond_signal(&condRemove);
        pthread_mutex_unlock(&commandLock);

        if (command == NULL){
            continue;
        }
        int numTokens;
        char token, type;
        char name[MAX_INPUT_SIZE];
        char name2[MAX_INPUT_SIZE];
        if(command[0] == 'm'){
            numTokens = sscanf(command, "%c %s %s", &token, name, name2);
        }
        else{
            numTokens = sscanf(command, "%c %s %c", &token, name, &type);
        }
        free(command);
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
            case 'm': 
                searchResult = lookup(name);
                if (searchResult >= 0){
                    printf("Search: %s found\n", name);
                    searchResult = lookup(name2);
                    if (searchResult >= 0){
                        printf("Search: %s found\n", name);
                        move(name, name2);
                    }          
                    else{
                        printf("Search: %s not found\n", name);
                        break;
                    }
                }
                else{
                    printf("Search: %s not found\n", name);
                    break;
                }
            
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    pthread_cond_signal(&condInsert);
    return NULL;
}

void runThreads() {
    pthread_t processThread;
    pthread_t* thread_list = malloc(maxThreads * sizeof(pthread_t));
    pthread_create(&processThread, NULL, processInput, NULL);
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
    pthread_join(processThread, NULL);
    free(thread_list);

}

void initBuffer() {
    struct Buffer *temp;
    temp = &buffer;
    for(int i = 1; i < MAX_COMMANDS; i++) {
        temp->next = malloc(sizeof(struct Buffer));
        temp->data[0] = '\0';
        temp = temp->next;
    }
    temp->data[0] = '\0';
    temp->next = &buffer;
}

int main(int argc, char* argv[]) {
    double elapsedTime;     //number of seconds the program ran for
    struct timeval startTime;
    struct timeval stopTime;
    FILE* outputFile;
    initBuffer();
    arguments(argc, argv);
    outputFile = openOutput();
    /* init filesystem */
    init_fs();
    if(gettimeofday(&startTime, NULL) != 0) {   //the program obtains its starting time from the pc's clock
        fprintf(stderr, "Couldn't get time\n");
        exit(EXIT_FAILURE);
    }

    /* process input and print tree */
    runThreads();
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
