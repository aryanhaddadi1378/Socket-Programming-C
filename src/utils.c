#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_DATA_SIZE 50

void printError(char *msg) {
  write(STDERR_FILENO, msg, strlen(msg));
  exit(EXIT_FAILURE);
}

void printMsg(char *msg) { write(STDOUT_FILENO, msg, strlen(msg)); }

void clear(char *ptr) {
  for (int i = 0; i < MAX_DATA_SIZE; i++) {
    ptr[i] = '\0';
  }
}