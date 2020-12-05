#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>

int sockfd;
int messageSize = sizeof(char) * 100; 
struct sockaddr_un local, remote;
socklen_t clilen, servlen;

int tfsCreate(char *filename, char nodeType) {    //sends request to the server to create a file or a directory in the specified path
  char* message = malloc(messageSize);
  char* node = malloc(sizeof(char));
  node[0] = nodeType;
  strcpy(message, "c ");
  strcat(message, filename);
  strcat(message, " ");
  strcat(message, node);
  if(sendto(sockfd, message, messageSize, 0, (struct sockaddr *) &remote, servlen) < 0) {
    perror("Send Error");
    free(message);
    free(node);
    return -1;
  }
  else if(recvfrom(sockfd, message, messageSize, 0, NULL, NULL) < 0) {
    perror("Receive Error");
    free(message);
    free(node);
    return -2;
  }
  else if(strcmp(message, "error") == 0) {
    perror("Server Error");
    free(message);
    free(node);
    return -3;
  }
  free(message);
  free(node);
  return 0;
}

int tfsDelete(char *path) {   //sends a request to the server to delete a specific file
  char* message = malloc(messageSize);
  strcpy(message, "d ");
  strcat(message, path);
  if(sendto(sockfd, message, messageSize, 0, (struct sockaddr *) &remote, servlen) < 0) {
    perror("Send Error");
    free(message);
    return -1;
  }
  else if(recvfrom(sockfd, message, messageSize, 0, NULL, NULL) < 0) {
    perror("Receive Error");
    free(message);
    return -2;
  }
  else if(strcmp(message, "error") == 0) {
    perror("Server Error");
    free(message);
    return -3;
  }
  free(message);
  return 0;
}

int tfsMove(char *from, char *to) {   //sends a request to the server to move a file from one directory to another
  char* message = malloc(messageSize);
  strcpy(message, "m ");
  strcat(message, from);
  strcat(message, " ");
  strcat(message, to);
  if(sendto(sockfd, message, messageSize, 0, (struct sockaddr *) &remote, servlen) < 0) {
    perror("Send Error");
    free(message);
    return -1;
  }
  else if(recvfrom(sockfd, message, messageSize, 0, NULL, NULL) < 0) {
    perror("Receive Error");
    free(message);
    return -2;
  }
  else if(strcmp(message, "error") == 0) {
    perror("Server Error");
    free(message);
    return -3;
  }
  free(message);
  return 0;
}

int tfsLookup(char *path) {   //sends to the server a request to look for a specific filepath
  char* message = malloc(messageSize);
  strcpy(message, "l ");
  strcat(message, path);
  if(sendto(sockfd, message, messageSize, 0, (struct sockaddr *) &remote, servlen) < 0) {
    perror("Send Error");
    free(message);
    return -1;
  }
  else if(recvfrom(sockfd, message, messageSize, 0, NULL, NULL) < 0) {
    perror("Receive Error");
    free(message);
    return -2;
  }
  else if(strcmp(message, "error") == 0) {
    perror("Server Error");
    free(message);
    return -3;
  }
  free(message);
  return 0;
}

int tfsPrint(char* filename) {  //sends to the server a request to print to a specific file
  char* message = malloc(messageSize);
  strcpy(message, "p ");
  strcat(message, filename);
  if(sendto(sockfd, message, messageSize, 0, (struct sockaddr *) &remote, servlen) < 0) {
    perror("Send Error");
    free(message);
    return -1;
  }
  else if(recvfrom(sockfd, message, messageSize, 0, NULL, NULL) < 0) {
    perror("Receive Error");
    free(message);
    return -2;
  }
  else if(strcmp(message, "error") == 0) {
    perror("Server Error");
    free(message);
    return -3;
  }
  free(message);
  return 0;
}

int tfsMount(char * sockPath) { //server path is recieved and the function creates a socket
  if((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
      perror("Socket Error");
      return -1;
  }
  local.sun_family = AF_UNIX;
  remote.sun_family = AF_UNIX;
  strcpy(local.sun_path, "/tmp/client");
  strcpy(remote.sun_path, sockPath);
  clilen = sizeof(struct sockaddr_un);
  servlen = sizeof(struct sockaddr_un);
  if(bind(sockfd, (struct sockaddr *) &local, clilen) < 0) {
    perror("Bind Error");
    return -2;
  }
  return 0;
}

int tfsUnmount() {  //closes the socket and unlinks the associated path
  unlink(local.sun_path);
  close(sockfd);
  return 0;
}
