#include "utils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DOWNLOAD 1
#define UPLOAD 2
#define MAX_DATA_SIZE 50
#define MAX_PORT_BITS 32
#define QUEUE_LIMIT 10
#define SERVER_DOES_NOT_HAVE_THE_FILE 1
#define SEND_DELAY 1
#define NOT_AVAILABLE (-1)
#define FAILED (-1)
#define TIMEOUT 0
#define EQUAL 0
#define EMPTY 0
#define CLOSED 0
#define TRUE 1
#define WAITING_TIME 2
#define PORT_TOKEN_MIN_LENGTH 6
#define FILE_TOKEN_MIN_LENGTH 7
#define DOWNLOAD_TOKEN_MIN_LENGTH 10
#define UPLOAD_TOKEN_MIN_LENGTH 8

#define LOCALHOST "127.0.0.1"
#define EXIT "exit"
#define HELP "help"
#define OK "1"
#define DONE "sdone"
#define SPACE ' '
#define PORT_TOKEN "!port"
#define FILE_TOKEN "!file "

int server = NOT_AVAILABLE;
char *filenameToSend;
int broadcastSock;
struct sockaddr_in broadcastAddr;

void setOptions(int sockId) {
  int opt = 1, opt2 = 1, opt3 = 1;
  setsockopt(sockId, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
  setsockopt(sockId, SOL_SOCKET, SO_REUSEPORT, &opt2, sizeof(opt2));
  setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, &opt3, sizeof(opt3));
}

int serverExists(char *portNum) {
  int hbSockId = socket(PF_INET, SOCK_DGRAM, 0);
  int portNumI = atoi(portNum);
  setOptions(hbSockId);

  struct sockaddr_in addrPort;
  addrPort.sin_family = AF_INET;
  addrPort.sin_port = htons((uint16_t)portNumI);
  addrPort.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  socklen_t size = sizeof(addrPort);

  struct timeval selTimeOut;
  selTimeOut.tv_sec = WAITING_TIME;
  selTimeOut.tv_usec = 0;

  fd_set readFds;
  FD_ZERO(&readFds);
  FD_SET(hbSockId, &readFds);

  char HBMsg[MAX_DATA_SIZE];
  if (bind(hbSockId, (struct sockaddr *)&addrPort, sizeof(addrPort)) !=
      FAILED) {
    if (select(hbSockId + 1, &readFds, NULL, NULL, &selTimeOut) != TIMEOUT) {
      if (recvfrom(hbSockId, HBMsg, MAX_DATA_SIZE, 0,
                   (struct sockaddr *)&addrPort, &size) != strlen(HBMsg)) {
        printError("Receive Failed!\n");
      } else {
        server = atoi(HBMsg);
        printMsg("Server Was Discovered! You are in Server/Client Mode!\n");
      }
      close(hbSockId);
      return 1;
    } else {
      printMsg("Server Doesn't Exist! You are in Peer-to-Peer Mode!\n");
      return 0;
    }
  } else {
    printError("Bind Failed!\n");
  }
}

int isUpload(char *msg) {
  if (strlen(msg) >= UPLOAD_TOKEN_MIN_LENGTH && msg[0] == 'u' &&
      msg[1] == 'p' && msg[2] == 'l' && msg[3] == 'o' && msg[4] == 'a' &&
      msg[5] == 'd' && msg[6] == ' ') {
    return 1;
  } else {
    return 0;
  }
}

int isDownload(char *msg) {
  if (strlen(msg) >= DOWNLOAD_TOKEN_MIN_LENGTH && msg[0] == 'd' &&
      msg[1] == 'o' && msg[2] == 'w' && msg[3] == 'n' && msg[4] == 'l' &&
      msg[5] == 'o' && msg[6] == 'a' && msg[7] == 'd' && msg[8] == ' ') {
    return 1;
  } else {
    return 0;
  }
}

int analyzeMsg(char *msg) {
  if (isUpload(msg)) {
    return UPLOAD;
  } else if (isDownload(msg)) {
    return DOWNLOAD;
  }
}

void sendFileToServer(int sockId, int fd, char *response, char *text) {
  recv(sockId, response, MAX_DATA_SIZE, 0);
  if (strcmp(response, OK) == EQUAL) {
    while (read(fd, text, MAX_DATA_SIZE) != 0) {
      if (send(sockId, text, strlen(text), 0) != strlen(text)) {
        printError("Send Failed\n");
      }
      recv(sockId, response, MAX_DATA_SIZE, 0);
      clear(text);
    }
    send(sockId, DONE, strlen(DONE), 0);
    recv(sockId, response, MAX_DATA_SIZE, 0);
    printMsg("\e[0;32m");
    printMsg(response);
    printMsg("\x1B[37m\n");
    close(fd);
  }
}

int getFileFromServer(int sockId, char *response, char *filenamePointer,
                      char *text) {
  recv(sockId, response, MAX_DATA_SIZE, 0);
  if (strcmp(response, OK) == EQUAL) {
    if (send(sockId, filenamePointer, strlen(filenamePointer), 0) !=
        strlen(filenamePointer)) {
      printError("Send Failed\n");
    }
    recv(sockId, response, MAX_DATA_SIZE, 0);

    if (atoi(response) == FAILED) {
      printMsg("Server doesn't have the file.Checking the other nodes for the "
               "file...\n");
      return SERVER_DOES_NOT_HAVE_THE_FILE;
    }

    send(sockId, "ready", strlen("ready"), 0);
    recv(sockId, text, MAX_DATA_SIZE, 0);
    int fd = open(filenamePointer, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    while (strcmp(text, DONE) != EQUAL) {
      write(fd, text, strlen(text));
      clear(text);
      send(sockId, OK, strlen(OK), 0);
      recv(sockId, text, MAX_DATA_SIZE, 0);
    }
    close(fd);
    printMsg("\e[0;32mDownload Was Successful!");
    printMsg("\x1B[37m\n");
  }
}

int serverClientMode(int sockId, char *filename) {
  char *text = (char *)malloc(MAX_DATA_SIZE * sizeof(char)),
       *response = (char *)malloc(MAX_DATA_SIZE * sizeof(char)),
       *filenamePointer;

  clear(text);
  clear(response);

  int status = analyzeMsg(filename);
  if (status == UPLOAD) {
    filenamePointer = &filename[UPLOAD_TOKEN_MIN_LENGTH - 1];
    int fd = open(filenamePointer, O_RDONLY);

    if (fd != FAILED) {
      if (send(sockId, "upload", strlen("upload"), 0) != strlen("upload")) {
        printError("Send Failed!\n");
      } else {
        recv(sockId, response, MAX_DATA_SIZE, 0);
        if (strcmp(response, OK) == EQUAL) {
          if (send(sockId, filenamePointer, strlen(filenamePointer), 0) !=
              strlen(filenamePointer)) {
            printError("Send Failed!\n");
          } else {
            sendFileToServer(sockId, fd, response, text);
          }
        }
      }
    } else {
      printMsg("\e[0;31mYou don't have the file you want to upload!\n");
      printMsg("\x1B[37m\n");
    }
  }

  else if (status == DOWNLOAD) {
    filenamePointer = &filename[DOWNLOAD_TOKEN_MIN_LENGTH - 1];
    if (send(sockId, "download", strlen("download"), 0) != strlen("download"))
      printError("Send Failed!\n");
    else if (getFileFromServer(sockId, response, filenamePointer, text) ==
             SERVER_DOES_NOT_HAVE_THE_FILE) {
      return SERVER_DOES_NOT_HAVE_THE_FILE;
    }
  }
  free(text);
  free(response);
  return 0;
}

int isPort(char *txt) {
  if (strlen(txt) >= PORT_TOKEN_MIN_LENGTH && txt[0] == '!' && txt[1] == 'p' &&
      txt[2] == 'o' && txt[3] == 'r' && txt[4] == 't') {
    return 1;
  } else {
    return 0;
  }
}

int isFileRequest(char *txt) {
  if (strlen(txt) >= FILE_TOKEN_MIN_LENGTH && txt[0] == '!' && txt[1] == 'f' &&
      txt[2] == 'i' && txt[3] == 'l' && txt[4] == 'e' && txt[5] == ' ') {
    return 1;
  } else {
    return 0;
  }
}

char *extractFilename(char *txt) {
  int i = 5;
  while (txt[i] != SPACE) {
    i++;
  }
  i++;
  return &(txt[i]);
}

int extractPort(char *txt) {
  int i = 5;
  char *temp = (char *)malloc(MAX_PORT_BITS * sizeof(char));
  while (txt[i] != SPACE) {
    temp[i - 5] = txt[i];
    i++;
  }
  i = atoi(temp);
  free(temp);
  return i;
}

void sendFileToAnotherNode(int sockId, char *text, char *data) {
  clear(text);
  int fd = open(data, O_RDONLY);
  while (read(fd, text, MAX_DATA_SIZE) != EMPTY) {
    if (send(sockId, text, strlen(text), 0) != strlen(text)) {
      printError("Send Failed\n");
    }
    recv(sockId, data, MAX_DATA_SIZE, 0);
    clear(text);
  }

  send(sockId, DONE, strlen(DONE), 0);
  close(fd);
  printMsg("\e[0;32m");
  printMsg("Transfer Completed!");
  printMsg("\x1B[37m\n");
}

void getFileFromAnotherNode(int fd, int connectionSock, char *response,
                            char *text, char *filename) {
  send(connectionSock, "ready", strlen("ready"), 0);
  recv(connectionSock, response, MAX_DATA_SIZE, 0);
  send(connectionSock, filename, strlen(filename), 0);
  recv(connectionSock, text, MAX_DATA_SIZE, 0);
  while (strcmp(text, DONE) != EQUAL) {
    write(fd, text, strlen(text));
    send(connectionSock, OK, strlen(OK), 0);
    clear(text);
    recv(connectionSock, text, MAX_DATA_SIZE, 0);
  }
  printMsg("\e[0;32m");
  printMsg("File got received successfully!");
  printMsg("\x1B[37m\n");
  close(fd);
}

void desiredFileBroadcastHandler() {
  if (strlen(filenameToSend) != 0) {
    if (sendto(broadcastSock, filenameToSend, strlen(filenameToSend), 0,
               (struct sockaddr *)&broadcastAddr,
               sizeof(broadcastAddr)) != strlen(filenameToSend)) {
      printError("Filename Send Failed!\n");
    }
    alarm(SEND_DELAY);
  }
}

void printHelp() {
  printMsg("\e[0;36m\nHelp:\n");
  printMsg("Welcome! Here are the possible commands:\n");
  printMsg("1- If you want to download a file enter \"download <filename>\"\n");
  printMsg("2- If you want to upload a file enter \"upload <filename>\"\n");
  printMsg("3- If you want to quit the system enter \"exit\"\nNow enter your "
           "command:\n");
  printMsg("\x1B[37m\n");
}

void handlePortTokenInput(char *clientPort, char *text, char *filename,
                          char *data, char *response,
                          struct sockaddr_in *clientAddr) {
  int fd, portNumber, connectionSock;
  clear(text);
  strcat(text, extractFilename(data));
  if (strcmp(filename, text) == EQUAL) {
    portNumber = extractPort(data);
    if (portNumber != atoi(clientPort)) {
      printMsg("Connected to get desired file...\n");
      clientAddr->sin_port = htons((uint16_t)portNumber);

      connectionSock = socket(PF_INET, SOCK_STREAM, 0);
      if (connectionSock == FAILED) {
        printError("Creating Socket Failed! \n");
      }
      setOptions(connectionSock);
      if (connect(connectionSock, (struct sockaddr *)clientAddr,
                  sizeof(*clientAddr)) == FAILED) {
        printError("Connection Failed!\n");
      }

      fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
      clear(text);
      clear(response);
      if (fd != FAILED) {
        getFileFromAnotherNode(fd, connectionSock, response, text, filename);
        clear(filenameToSend);
        clear(filename);
      }
      clientAddr->sin_port = htons((uint16_t)atoi(clientPort));
      close(connectionSock);
    }
  }
}

void startCommunication(char *broadcastPort, char *clientPort) {
  int fd, maxfd, tempSock, clientServerSock;
  fd_set readFds;

  int clients[QUEUE_LIMIT];
  for (int i = 0; i < QUEUE_LIMIT; i++) {
    clients[i] = EMPTY;
  }

  broadcastSock = socket(PF_INET, SOCK_DGRAM, 0);
  setOptions(broadcastSock);

  broadcastAddr.sin_family = AF_INET;
  broadcastAddr.sin_port = htons((uint16_t)atoi(broadcastPort));
  broadcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  socklen_t broadcastSize = sizeof(broadcastAddr);

  if (bind(broadcastSock, (struct sockaddr *)&broadcastAddr, broadcastSize) ==
      FAILED) {
    printError("Bind Failed!\n");
  }

  int clientSock = socket(PF_INET, SOCK_STREAM, 0);
  setOptions(clientSock);
  clients[0] = clientSock;

  struct sockaddr_in clientAddr;
  clientAddr.sin_family = AF_INET;
  clientAddr.sin_port = htons((uint16_t)atoi(clientPort));
  clientAddr.sin_addr.s_addr = inet_addr(LOCALHOST);

  struct sockaddr_in *receiverAddr;

  if (bind(clientSock, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) ==
      FAILED) {
    printError("Bind Failed!\n");
  }
  if (listen(clientSock, QUEUE_LIMIT) == FAILED) {
    printError("Listen Failed!\n");
  }

  if (server != NOT_AVAILABLE) {
    clientServerSock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((uint16_t)server);
    serverAddr.sin_addr.s_addr = inet_addr(LOCALHOST);
    if (connect(clientServerSock, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == FAILED) {
      printError("Connection To Server Failed!\n");
    }
  }

  char *filename = (char *)malloc(MAX_DATA_SIZE * sizeof(char)),
       *text = (char *)malloc(MAX_DATA_SIZE * sizeof(char)),
       *data = (char *)malloc(MAX_DATA_SIZE * sizeof(char)),
       *response = (char *)malloc(MAX_DATA_SIZE * sizeof(char)),
       *filenamePointer = (char *)malloc(MAX_DATA_SIZE * sizeof(char));

  printMsg("\e[1;33mEnter your command. Enter \"help\" for help!");
  printMsg("\x1B[37m\n");

  while (TRUE) {
    FD_ZERO(&readFds);
    FD_SET(STDIN_FILENO, &readFds);
    FD_SET(broadcastSock, &readFds);
    maxfd = broadcastSock;
    for (int i = 0; i < QUEUE_LIMIT; i++) {
      if (clients[i] != EMPTY) {
        FD_SET(clients[i], &readFds);
        if (clients[i] > maxfd)
          maxfd = clients[i];
      }
    }

    if (select(maxfd + 1, &readFds, NULL, NULL, NULL) == FAILED) {
      continue;
    }
    if (FD_ISSET(STDIN_FILENO, &readFds)) {
      clear(text);
      read(STDIN_FILENO, text, MAX_DATA_SIZE);
      text[strlen(text) - 1] = '\0';
      if (strcmp(text, EXIT) == EQUAL) {
        printMsg("Goodbye.See you!\n");
        return;
      }
      if (strlen(filenameToSend) != 0) {
        printMsg("\e[0;31mYou haven't received the file you asked last "
                 "time.Wait until it gets finished");
        printMsg("\x1B[37m\n");
        continue;
      } else if (isUpload(text)) {
        if (server == NOT_AVAILABLE) {
          printMsg("\e[0;31mUpload feature is not possible due to server "
                   "unavailability");
          printMsg("\x1B[37m\n");
        } else {
          serverClientMode(clientServerSock, text);
        }
        continue;
      } else if (isDownload(text)) {
        if (server != NOT_AVAILABLE &&
            serverClientMode(clientServerSock, text) !=
                SERVER_DOES_NOT_HAVE_THE_FILE) {
          continue;
        }
        clear(filenameToSend);
        strcat(filenameToSend, FILE_TOKEN);
        strcat(filenameToSend, &text[DOWNLOAD_TOKEN_MIN_LENGTH - 1]);
        clear(filename);
        strcat(filename, &filenameToSend[FILE_TOKEN_MIN_LENGTH - 1]);
        signal(SIGALRM, desiredFileBroadcastHandler);
        desiredFileBroadcastHandler();
        printMsg("File request was sent to other nodes.\n");
      } else if (strcmp(text, HELP) == EQUAL) {
        printHelp();
        continue;
      } else {
        printMsg("\e[0;31mInvalid Input.If you want to know the rules enter "
                 "\"help\"");
        printMsg("\x1B[37m\n");
        continue;
      }
    }

    if (FD_ISSET(broadcastSock, &readFds)) {
      clear(data);
      recvfrom(broadcastSock, data, MAX_DATA_SIZE, 0,
               (struct sockaddr *)&broadcastAddr, &broadcastSize);
      broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
      if (isPort(data)) {
        handlePortTokenInput(clientPort, text, filename, data, response,
                             &clientAddr);
      } else if (isFileRequest(data) &&
                 strcmp(filename, &data[FILE_TOKEN_MIN_LENGTH - 1]) != EQUAL) {
        clear(filenamePointer);
        strcat(filenamePointer, &data[FILE_TOKEN_MIN_LENGTH - 1]);
        fd = open(filenamePointer, O_RDONLY);
        if (fd != FAILED) {
          clear(response);
          strcat(response, PORT_TOKEN);
          strcat(response, clientPort);
          strcat(response, " ");
          strcat(response, filenamePointer);
          if (sendto(broadcastSock, response, strlen(response), 0,
                     (struct sockaddr *)&broadcastAddr,
                     broadcastSize) != strlen(response)) {
            printError("Send Failed!\n");
          }
          close(fd);
        }
      }
    }

    for (int i = 1; i < QUEUE_LIMIT; i++) {
      if (clients[i] != EMPTY) {
        tempSock = clients[i];
        if (FD_ISSET(tempSock, &readFds)) {
          if (recv(tempSock, data, MAX_DATA_SIZE, 0) == CLOSED) {
            clients[i] = EMPTY;
            close(tempSock);
            free(receiverAddr);
          } else {
            send(tempSock, OK, strlen(OK), 0);
            clear(data);
            recv(tempSock, data, MAX_DATA_SIZE, 0);
            printMsg("A user got connected to receive a file...\n");
            sendFileToAnotherNode(tempSock, text, data);
          }
        }
      }
    }

    if (FD_ISSET(clientSock, &readFds)) {
      receiverAddr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
      socklen_t receiverSize = sizeof(*receiverAddr);
      if ((tempSock = accept(clientSock, (struct sockaddr *)receiverAddr,
                             &receiverSize)) == FAILED) {
        printError("Accept Error!\n");
      }
      for (int i = 1; i < QUEUE_LIMIT; i++) {
        if (clients[i] == EMPTY) {
          clients[i] = tempSock;
          break;
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printError("Invalid Input for program to run!\n");
  }
  printMsg("Looking for server...\n");
  serverExists(argv[1]);
  filenameToSend = (char *)malloc(MAX_DATA_SIZE * sizeof(char));
  clear(filenameToSend);
  startCommunication(argv[2], argv[3]);
  return 0;
}
