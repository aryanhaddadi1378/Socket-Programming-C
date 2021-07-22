#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h> 


#define QUEUE_LIMIT 10
#define MAX_DATA_SIZE 50
#define PORT 8090
#define DELAY 1
#define EQUAL 0
#define FAILED -1
#define TRUE 1
#define CLOSED 0
#define EMPTY 0


#define LOCALHOST "127.0.0.1"
#define OK "1"
#define DONE "sdone"
#define FILE_DOES_NOT_EXIST "-1"

int hbSockid;
struct sockaddr_in addrportG;
char* hbport, *port = "8090";


int getHBPort(char* HBPort) {
    return atoi(HBPort);
}

void printError(char* msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
    exit(EXIT_FAILURE);
}

void handle(int sig) {
    if (sendto(hbSockid, port, strlen(port), 0, (struct sockaddr*)& addrportG, sizeof(addrportG)) != strlen(hbport)) {
            printError("Send Failed\n");
    } 
    alarm(DELAY);
}

void printmsg(char* msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
}


void sendHeartBeat(char* portNumS) {
    int portNumI = atoi(portNumS);
    hbSockid = socket(PF_INET, SOCK_DGRAM, 0);
    hbport = portNumS;
    int option = 1;
    setsockopt(hbSockid, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option));
    setsockopt(hbSockid, SOL_SOCKET, SO_BROADCAST, &option, sizeof(option));
    addrportG.sin_family = AF_INET;
    addrportG.sin_port = htons((uint16_t)portNumI);
    addrportG.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    signal(SIGALRM, handle);
    alarm(DELAY);

}

void clear(char* ptr) {
    for (int i = 0 ; i < MAX_DATA_SIZE ; i++) {
        ptr[i] = '\0'; 
    }
}

void getFileFromClient(int clientSocket, int fd, char* filename, char* text) {
    send(clientSocket, OK, MAX_DATA_SIZE, 0);
    recv(clientSocket, filename, MAX_DATA_SIZE, 0);
    unlink(filename);
    fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    send(clientSocket, OK, MAX_DATA_SIZE, 0);
    printmsg("Starting getting file from a client...\n");
    recv(clientSocket, text, MAX_DATA_SIZE, 0);
    while (strcmp(text, DONE) != EQUAL) {
        write(fd, text, strlen(text));
        clear(text);
        send(clientSocket, OK, MAX_DATA_SIZE, 0);
        recv(clientSocket, text, MAX_DATA_SIZE, 0);
    }
    close(fd);
    send(clientSocket, "Upload Was Successfull!", MAX_DATA_SIZE, 0);
    printmsg("\e[0;32m");
    printmsg(filename);
    printmsg(" was received successfully!");
    printmsg("\x1B[37m\n");
}

void sendFileToClient(int clientSocket, int fd, char* filename, char* response, char* text) {
    send(clientSocket, OK, MAX_DATA_SIZE, 0);
    recv(clientSocket, filename, MAX_DATA_SIZE, 0);
    fd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR);
    if (fd == FAILED) send(clientSocket, FILE_DOES_NOT_EXIST, MAX_DATA_SIZE, 0);
    else {
        send(clientSocket, OK, strlen(OK), 0);
        recv(clientSocket, response, MAX_DATA_SIZE, 0);
        if (strcmp(response, "ready") == EQUAL) {
            printmsg("Starting sending file to a client..\n");
            while (read(fd, text, MAX_DATA_SIZE) != 0) {
                send(clientSocket, text, strlen(text), 0);
                recv(clientSocket, response, 5, 0);
                clear(text);
            }
            send(clientSocket, DONE, strlen(DONE), 0);
            printmsg("\e[0;32mFile was Sent successfully!");
            printmsg("\x1B[37m\n");
        }
        close(fd);
    }   
}

int main(int argc, char* argv[]) {
    if (argc != 2) printError("Invalid Input for program to run!\n");
    
    int clients[QUEUE_LIMIT], maxfd, clientSocket, fd, option = 1;
    for (int i = 0 ; i < QUEUE_LIMIT ; i++) clients[i] = EMPTY;

    fd_set readfds;
    FD_ZERO(&readfds);

    sendHeartBeat(argv[1]);
    int sockid = socket(PF_INET, SOCK_STREAM, 0);
    if (sockid == FAILED) printError("Socket Failed!\n");
    clients[0] = sockid;
    setsockopt(sockid, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option));

    struct sockaddr_in addrport;
    addrport.sin_family = AF_INET;
    addrport.sin_port = htons(PORT);
    addrport.sin_addr.s_addr = inet_addr(LOCALHOST);


    char* filename = (char*) malloc(MAX_DATA_SIZE * sizeof(char)), 
        * text = (char*) malloc(MAX_DATA_SIZE * sizeof(char)),
        * command = (char*) malloc(MAX_DATA_SIZE * sizeof(char)),
        * response = (char*) malloc (MAX_DATA_SIZE * sizeof(char));

    struct sockaddr client;
    socklen_t size = sizeof(client);

    if (bind(sockid, (struct sockaddr*) &addrport, sizeof(addrport)) == FAILED) printError("Bind Failed!\n");
    if (listen(sockid, QUEUE_LIMIT) == FAILED) printError("Listen Failed!\n");

    printmsg("\e[1;33mListening.....");
    printmsg("\x1B[37m\n");

    while(TRUE) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        maxfd = STDIN_FILENO;
        for (int i = 0 ; i < QUEUE_LIMIT ; i++) {
            if (clients[i] != EMPTY) {
                FD_SET(clients[i], &readfds);
            }
            if (clients[i] > maxfd) maxfd = clients[i];
        }

        if (select(maxfd+1, &readfds, NULL, NULL, NULL) == FAILED) continue;
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            read(STDIN_FILENO, command, MAX_DATA_SIZE);
            if (strlen(command) > 1) command[strlen(command)-1] = '\0';
            if (strcmp(command, "exit") == EQUAL) {
                printmsg("Goodbye.See you!\n");
                break;
            }
            else printmsg("Wrong command!\n");
        }
        if (FD_ISSET(sockid, &readfds)) {
            if ((clientSocket = accept(sockid, (struct sockaddr*) &client, &size)) == FAILED) printError("Accept Error!\n");
            
            for (int i = 0 ; i < QUEUE_LIMIT ; i++) {
                if (clients[i] == EMPTY) {
                    printmsg("A new client got connected!\n");
                    clients[i] = clientSocket;
                    break;
                }
            }
            continue;
        }
        for (int i = 1 ; i < QUEUE_LIMIT ; i++) {
            clientSocket = clients[i];
            if (FD_ISSET(clientSocket, &readfds)) {
                if (recv(clientSocket, command, MAX_DATA_SIZE, 0) == CLOSED) {
                    clients[i] = CLOSED;
                    close(clientSocket);
                    printmsg("A client Closed its connection!\n");
                }
                else {
                    if (strcmp(command, "upload") == EQUAL) {
                        getFileFromClient(clientSocket, fd, filename, text);
                    }
                    else if (strcmp(command, "download") == EQUAL) {
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
    close(sockid);    
    return 0;
}