#include "utils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define QUEUE_LIMIT 10
#define MAX_DATA_SIZE 50
#define PORT 8090
#define DELAY 1
#define EQUAL 0
#define FAILED (-1)
#define TRUE 1
#define CLOSED 0
#define EMPTY 0

#define LOCALHOST "127.0.0.1"
#define OK "1"
#define DONE "sdone"
#define FILE_DOES_NOT_EXIST "-1"

int hbSockId;
struct sockaddr_in addrPortG;
char *hbPort, *port = "8090";

void handle() {
  if (sendto(hbSockId, port, strlen(port), 0, (struct sockaddr *)&addrPortG,
             sizeof(addrPortG)) != strlen(hbPort)) {
    printError("Send Failed!\n");
  }
  alarm(DELAY);
}

void sendHeartBeat(char *portNumS) {
  int portNumI = atoi(portNumS);
  hbSockId = socket(PF_INET, SOCK_DGRAM, 0);
  hbPort = portNumS;
  int option = 1;
  setsockopt(hbSockId, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option));
  setsockopt(hbSockId, SOL_SOCKET, SO_BROADCAST, &option, sizeof(option));
  addrPortG.sin_family = AF_INET;
  addrPortG.sin_port = htons((uint16_t)portNumI);
  addrPortG.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  signal(SIGALRM, handle);
  alarm(DELAY);
}

void getFileFromClient(int clientSocket, int fd, char *filename, char *text) {
  send(clientSocket, OK, MAX_DATA_SIZE, 0);
  recv(clientSocket, filename, MAX_DATA_SIZE, 0);
  unlink(filename);
  fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  send(clientSocket, OK, MAX_DATA_SIZE, 0);
  printMsg("Starting getting file from a client...\n");
  recv(clientSocket, text, MAX_DATA_SIZE, 0);
  while (strcmp(text, DONE) != EQUAL) {
    write(fd, text, strlen(text));
    clear(text);
    send(clientSocket, OK, MAX_DATA_SIZE, 0);
    recv(clientSocket, text, MAX_DATA_SIZE, 0);
  }
  close(fd);
  send(clientSocket, "Upload Was Successful!", MAX_DATA_SIZE, 0);
  printMsg("\e[0;32m");
  printMsg(filename);
  printMsg(" was received successfully!");
  printMsg("\x1B[37m\n");
}

void sendFileToClient(int clientSocket, int fd, char *filename, char *response,
                      char *text) {
  send(clientSocket, OK, MAX_DATA_SIZE, 0);
  recv(clientSocket, filename, MAX_DATA_SIZE, 0);
  fd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR);
  if (fd == FAILED)
    send(clientSocket, FILE_DOES_NOT_EXIST, MAX_DATA_SIZE, 0);
  else {
    send(clientSocket, OK, strlen(OK), 0);
    recv(clientSocket, response, MAX_DATA_SIZE, 0);
    if (strcmp(response, "ready") == EQUAL) {
      printMsg("Starting sending file to a client..\n");
      while (read(fd, text, MAX_DATA_SIZE) != 0) {
        send(clientSocket, text, strlen(text), 0);
        recv(clientSocket, response, 5, 0);
        clear(text);
      }
      send(clientSocket, DONE, strlen(DONE), 0);
      printMsg("\e[0;32mFile was Sent successfully!");
      printMsg("\x1B[37m\n");
    }
    close(fd);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2)
    printError("Invalid Input for program to run!\n");

  int clients[QUEUE_LIMIT], maxfd, clientSocket, fd, option = 1;
  for (int i = 0; i < QUEUE_LIMIT; i++) {
    clients[i] = EMPTY;
  }

  fd_set readFds;
  FD_ZERO(&readFds);

  sendHeartBeat(argv[1]);
  int sockId = socket(PF_INET, SOCK_STREAM, 0);
  if (sockId == FAILED) {
    printError("Socket Failed!\n");
  }
  clients[0] = sockId;
  setsockopt(sockId, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option));

  struct sockaddr_in addrPort;
  addrPort.sin_family = AF_INET;
  addrPort.sin_port = htons(PORT);
  addrPort.sin_addr.s_addr = inet_addr(LOCALHOST);

  char *filename = (char *)malloc(MAX_DATA_SIZE * sizeof(char)),
       *text = (char *)malloc(MAX_DATA_SIZE * sizeof(char)),
       *command = (char *)malloc(MAX_DATA_SIZE * sizeof(char)),
       *response = (char *)malloc(MAX_DATA_SIZE * sizeof(char));

  struct sockaddr client;
  socklen_t size = sizeof(client);

  if (bind(sockId, (struct sockaddr *)&addrPort, sizeof(addrPort)) == FAILED) {
    printError("Bind Failed!\n");
  }
  if (listen(sockId, QUEUE_LIMIT) == FAILED) {
    printError("Listen Failed!\n");
  }

  printMsg("\e[1;33mListening.....");
  printMsg("\x1B[37m\n");

  while (TRUE) {
    FD_ZERO(&readFds);
    FD_SET(STDIN_FILENO, &readFds);
    maxfd = STDIN_FILENO;
    for (int i = 0; i < QUEUE_LIMIT; i++) {
      if (clients[i] != EMPTY) {
        FD_SET(clients[i], &readFds);
      }
      if (clients[i] > maxfd) {
        maxfd = clients[i];
      }
    }

    if (select(maxfd + 1, &readFds, NULL, NULL, NULL) == FAILED) {
      continue;
    }
    if (FD_ISSET(STDIN_FILENO, &readFds)) {
      read(STDIN_FILENO, command, MAX_DATA_SIZE);
      if (strlen(command) > 1) {
        command[strlen(command) - 1] = '\0';
      }
      if (strcmp(command, "exit") == EQUAL) {
        printMsg("Goodbye.See you!\n");
        break;
      } else {
        printMsg("Wrong command!\n");
      }
    }
    if (FD_ISSET(sockId, &readFds)) {
      if ((clientSocket = accept(sockId, (struct sockaddr *)&client, &size)) ==
          FAILED) {
        printError("Accept Error!\n");
      }

      for (int i = 0; i < QUEUE_LIMIT; i++) {
        if (clients[i] == EMPTY) {
          printMsg("A new client got connected!\n");
          clients[i] = clientSocket;
          break;
        }
      }
      continue;
    }
    for (int i = 1; i < QUEUE_LIMIT; i++) {
      clientSocket = clients[i];
      if (FD_ISSET(clientSocket, &readFds)) {
        if (recv(clientSocket, command, MAX_DATA_SIZE, 0) == CLOSED) {
          clients[i] = CLOSED;
          close(clientSocket);
          printMsg("A client Closed its connection!\n");
        } else {
          if (strcmp(command, "upload") == EQUAL) {
            getFileFromClient(clientSocket, fd, filename, text);
          } else if (strcmp(command, "download") == EQUAL) {
            sendFileToClient(clientSocket, fd, filename, response, text);
          }
        }
      }
    }
    clear(filename);
    clear(text);
    clear(command);
    clear(response);
  }
  close(sockId);
  return 0;
}