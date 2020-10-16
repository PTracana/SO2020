#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <time.h>
#include<pthread.h>

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100


/*variaveis globais que guardam os parametros com que se inicializa o programa:
tecnicofs inputfile outputfile maxThreads synchstrategy*/

char* inputFilename = NULL;
char* outputFilename = NULL;
int maxThreads = 0;
char* synchstrategy = NULL;

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
    synchstrategy = argv[4];    //estrategia de sincronizacao (nosync, mutex ou rwlock)

    if(maxThreads <= 0) { //tem de existir um numero de threads valido
        fprintf(stderr, "Please use a valid number of threads\n");
    }
    else if(strcmp(synchstrategy, "nosync") != 0 && strcmp(synchstrategy, "mutex") != 0 && strcmp(synchstrategy, "rwlock") != 0) { //so existem tres metodos possiveis
        fprintf(stderr, "Please use a valid sync method from the following:\n mutex, rwlock, nosync\n");
    }
    else if(strcmp(synchstrategy, "nosync") == 0 && maxThreads != 1) {  //o metodo nosync so pode ter uma thread
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

void *applyCommand(char *command){
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
        case 'l': 
            searchResult = lookup(name);
            if (searchResult >= 0)
                printf("Search: %s found\n", name);
            else
                printf("Search: %s not found\n", name);
            break;
        case 'd':
            printf("Delete: %s\n", name);
            delete(name);
            break;
        default: { /* error */
            fprintf(stderr, "Error: command to apply\n");
            exit(EXIT_FAILURE);
        }
    }
}



void applyCommands(){
    pthread_t *thread_id;
    int numberThreads;
    thread_id = malloc(sizeof(thread_id));
    while (numberCommands > 0){
        const char* command = removeCommand();
        if (command == NULL){
            continue;
        }
        for (int i = 0 ; i< maxThreads; i++){
            pthread_create(&thread_id[i], NULL, applyCommand , (void *)command);
             numberThreads++;
        }
    }
}

int main(int argc, char* argv[]) {
    FILE* outputFile;
    arguments(argc, argv); //o programa obtem os parametros que foram introduzidos na linha de comandos quando este foi invocado
    outputFile = openOutput();
    /* init filesystem */
    init_fs();

    /* process input and print tree */
    processInput();
    applyCommands();
    print_tecnicofs_tree(outputFile);
    fclose(outputFile);

    /* release allocated memory */
    destroy_fs();
    exit(EXIT_SUCCESS);
}
