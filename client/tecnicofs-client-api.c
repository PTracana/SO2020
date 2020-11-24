#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

int tfsCreate(char *filename, char nodeType) {
  return -1;
}

int tfsDelete(char *path) {
  return -1;
}

int tfsMove(char *from, char *to) {
  return -1;
}

int tfsLookup(char *path) {
  return -1;
}

int tfsMount(char * sockPath) {
  int sockfd;
  struct sockaddr_un local;
  socklen_t len;
  if((sockfd = sock(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
      return -1;
  }
  local.sun_family = AF_UNIX;
  strcpy(local.sun_path, sockPath);
  unlink(local.sun_path);
  len = strlen(local.sun_path) + sizeof(local.sun_family);
  if (bind(sockfd, (struct sockaddr *) &local, len) < 0) {
      return -2;
  }  
  return 0;
}

int tfsUnmount() {
  return -1;
}
