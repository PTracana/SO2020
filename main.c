#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#define MAX_INPUT_SIZE 100

/*global variables that are used when initializing the program:
tecnicofs inputfile outputfile maxThreads synchstrategy*/

int maxThreads = 0;             //maximum number of threads is stored here
char* nomeSocket = NULL;        //socket identification


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

void* applyCommands() {
    while(1) {
        char* command = removeCommand();
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
        else if(command[0] == 'p') {
            numTokens = sscanf(command, "%c %s %s", &token, name);
        }
        else {
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
            case 'p':
                //wait for other tasks to finish and prevent more tasks from being started
                FILE* outputFile = openOutput(name);
                print_tecnicofs_tree(outputFile);
                fclose(outputFile);
                break;
            
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    return NULL;
    }
}

void runThreads() {
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

int setSocket(char* sockPath) {
    int sockfd;
    struct sockaddr_un remote;
    socklen_t len;
    if((sockfd = sock(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        return -1;
    }
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, sockPath);
    unlink(remote.sun_path);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (bind(sockfd, (struct sockaddr *) &remote, len) < 0) {
        return -2;
    }  
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
    
    init_fs();      // init filesystem
    if(gettimeofday(&startTime, NULL) != 0) {   //the program obtains its starting time from the pc's clock
        fprintf(stderr, "Couldn't get time\n");
        exit(EXIT_FAILURE);
    }

    runThreads();

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
