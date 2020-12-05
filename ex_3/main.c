#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_INPUT_SIZE 100

/*global variables that are used when initializing the program:
tecnicofs maxThreads nomeSocket*/

int maxThreads = 0;             //maximum number of threads is stored here
int isPrinting = 0;             //0 for not printing, 1 for printing, 2 for waiting state
int modThreads = 0;             //stores number of threads that have modifying behavior
char* nomeSocket = NULL;        //socket identification

//this condition variable and lock garantees that the program only prints if there are no other active operations that modify the tree
pthread_mutex_t printLock =  PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t printCond = PTHREAD_COND_INITIALIZER;

//this condition variable and lock garantees that while the program is printing, the other threads can't modify the tree
pthread_mutex_t opLock =  PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t opCond = PTHREAD_COND_INITIALIZER;

int sockfd;
struct sockaddr_un remote, local;
socklen_t servlen, clilen;
FILE* outputFile;


static void arguments(int argc, char* const argv[]) {   //this function parses the program's variables
    if(argc != 3) {                                     //the function only succeeds if you have exactly 3 arguments and if their typings are correct
        fprintf(stderr, "Wrong argument usage\n");
        exit(EXIT_FAILURE);
    }
    maxThreads = atoi(argv[1]);
    nomeSocket = argv[2]; 
        
    if(maxThreads <= 0) {   //there has to be a number of threads greater than 0
        fprintf(stderr, "Please use a valid number of threads\n");
        exit(EXIT_FAILURE);
    }
}

FILE* openOutput(char* filename) {        //the output file is opened for writing only
    FILE* outputFile = fopen(filename, "w");
    if(!outputFile) {
        fprintf(stderr, "Could not open/create requested output file");
        exit(EXIT_FAILURE);
    }
    return outputFile;
}

void* applyCommands() {     //this fuction receives a command from a client and executes the associated function
    while(1) {
        char* command = malloc(sizeof(char) * 100);
        int numTokens;
        char token, type;
        char name[MAX_INPUT_SIZE];
        char name2[MAX_INPUT_SIZE];
        if(recvfrom(sockfd, command, sizeof(char) * 100, 0, (struct sockaddr *) &local, &clilen) < 0) {
            perror("Receive Error");
            free(command);
            return NULL;
        }
        else if(command[0] == 'm'){
            numTokens = sscanf(command, "%c %s %s", &token, name, name2);
        }
        else if(command[0] == 'p') {
            numTokens = sscanf(command, "%c %s", &token, name);
        }
        else {
            numTokens = sscanf(command, "%c %s %c", &token, name, &type);
        }
        free(command);

        if (numTokens < 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }
        else if(token != 'l' && isPrinting == 1) {
            pthread_mutex_lock(&opLock);
            pthread_cond_wait(&opCond, &opLock);    //every thread with modifying behavior waits until the program finishes printing
            pthread_mutex_unlock(&opLock);
        }

        int result = 0;        //this variable saves the output of the applied command an it is sent back to the client as a reply
        switch (token) {      //there are 5 different types of commands: c (create), d (delete), l (lookup), m (move) and p (print)
            case 'c':
                switch (type) {
                    case 'f':
                        modThreads++;
                        printf("Create file: %s\n", name);
                        result = create(name, T_FILE);
                        break;
                    case 'd':
                        modThreads++;
                        printf("Create directory: %s\n", name);
                        result = create(name, T_DIRECTORY);
                        break;
                    default:
                        fprintf(stderr, "Error: invalid node type\n");
                        sendto(sockfd, "error", sizeof(char) * 100, 0, (struct sockaddr *) &local, clilen);
                        exit(EXIT_FAILURE);
                }
                modThreads--;
                break;
            case 'l':       
                result = lookup(name);
                if (result >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                break;
            case 'd':       
                modThreads++;
                printf("Delete: %s\n", name);
                result = delete(name);
                modThreads--;
                break;
            case 'm':
                modThreads++; 
                result = lookup(name);
                if (result >= 0){
                    printf("Search: %s found\n", name);
                    result = lookup(name2);
                    if (result >= 0){
                        printf("Search: %s found\n", name2);
                        result = move(name, name2);
                        modThreads--;
                    }          
                    else{
                        printf("Search: %s not found\n", name2);
                        modThreads--;
                        break;
                    }
                }
                else {
                    printf("Search: %s not found\n", name);
                    modThreads--;
                    break;
                }
            case 'p':
                //waits for other tasks to finish and prevents more tasks from being started (only applies to create, delete and move)
                isPrinting = 2;     //waiting state
                if(modThreads > 0) {
                    pthread_mutex_lock(&printLock);
                    pthread_cond_wait(&printCond, &printLock);
                    pthread_mutex_unlock(&printLock);
                }
                isPrinting = 1;     //printing state
                outputFile = openOutput(name);
                print_tecnicofs_tree(outputFile);
                fclose(outputFile);
                isPrinting = 0;
                pthread_cond_signal(&opCond);   //when the printing is done, the threads can freely apply all kinds of commands
                break;
            
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                sendto(sockfd, "error", sizeof(char) * 100, 0, (struct sockaddr *) &local, clilen);
                exit(EXIT_FAILURE);
            }
        }
        if(modThreads == 0 && isPrinting == 1) {
            pthread_cond_signal(&printCond); //the program can print if there are no active modifying commands
        }
        char* reply = malloc(sizeof(char) * 10);
        sprintf(reply, "%d", result);
        sendto(sockfd, reply, sizeof(char) * 100, 0, (struct sockaddr *) &local, clilen);
        free(reply);
    }
    return NULL;
}

void runThreads() {     //this function works as a thread creator and manager
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

int setSocket(char* sockPath) {  //this function creates a socket to establish a connection between the server and a client
    if((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("Sock Error");
        return -1;
    }
    printf("Socket created\n");
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, sockPath);
    clilen = sizeof(struct sockaddr_un);
    servlen = sizeof(struct sockaddr_un);
    if(bind(sockfd, (struct sockaddr *) &remote, servlen) < 0) {
        perror("Bind Error");
        return -2;
    }
    printf("Listening...\n");
    return 0;
}

int main(int argc, char* argv[]) {
    double elapsedTime;     //number of seconds the program ran for
    struct timeval startTime;
    struct timeval stopTime;

    arguments(argc, argv);

    if(setSocket(nomeSocket) < 0) {
        fprintf(stderr, "Failed to create socket\n");
        exit(EXIT_FAILURE);
    }
    
    init_fs();      // initializes filesystem
    if(gettimeofday(&startTime, NULL) != 0) {   //the program obtains its starting time from the pc's clock
        fprintf(stderr, "Couldn't get time\n");
        exit(EXIT_FAILURE);
    }

    runThreads();  //threads are created and begin receiving commands

    if(gettimeofday(&stopTime, NULL) != 0) {    //the program obtains its stopping time from the pc's clock
        fprintf(stderr, "Couldn't get time\n");
        exit(EXIT_FAILURE);
    }
    elapsedTime = (double) (stopTime.tv_usec - startTime.tv_usec) / CLOCKS_PER_SEC;
    printf("The program ended in %.4f seconds.\n", elapsedTime);

    /* release allocated memory */
    destroy_fs();
    exit(EXIT_SUCCESS);
}
