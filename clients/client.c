#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <fcntl.h> 
#include <signal.h>

#define NONE 0
#define DOWNLOAD 1
#define UPLOAD 2
#define MAX_DATA_SIZE 50
#define MAX_PORT_BITS 32
#define QUEUE_LIMIT 10
#define FILE_DOES_NOT_EXIST_FOR_UPLOAD -1
#define SERVER_DOES_NOT_HAVE_THE_FILE 1
#define SEND_DELAY 1
#define NOT_AVAILABLE -1
#define FAILED -1
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
char* filenameToSend;
int broadcastSock;
struct sockaddr_in broadcastaddr;


void printError(char* msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
    exit(EXIT_FAILURE);
}

void printmsg(char* msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
}

void setOptions(int sockid) {
    int opt = 1, opt2 = 1, opt3 = 1;
    setsockopt(sockid, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(sockid, SOL_SOCKET, SO_REUSEPORT, &opt2, sizeof(opt2));
    setsockopt(sockid, SOL_SOCKET, SO_REUSEADDR, &opt3, sizeof(opt3));
}

int serverExists(char* portnum) {
    int hbSockid = socket(PF_INET, SOCK_DGRAM, 0);
    int portNumI = atoi(portnum);
    setOptions(hbSockid);

    struct sockaddr_in addrport;
    addrport.sin_family = AF_INET;
    addrport.sin_port = htons((uint16_t)portNumI);
    addrport.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    socklen_t size = sizeof(addrport);

    struct timeval selTimeOut;
    selTimeOut.tv_sec = WAITING_TIME;
    selTimeOut.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(hbSockid, &readfds);

    char HBMsg[MAX_DATA_SIZE];
    if (bind(hbSockid, (struct sockaddr*) &addrport, sizeof(addrport)) != FAILED) {
        if (select(hbSockid+1, &readfds, NULL, NULL, &selTimeOut) != TIMEOUT) {
            if (recvfrom(hbSockid, HBMsg, MAX_DATA_SIZE, 0, (struct sockaddr*)& addrport, &size) != strlen(HBMsg)) {
                printError("Receive Failed\n");
            } 
            else {
                server = atoi(HBMsg);
                printmsg("Server Was Discovered! You are in Server/Client Mode!\n");
            }
            close(hbSockid);
            return 1;
        }
        else {
            printmsg("Server Doesn't Exist! You are in Peer-to-Peer Mode!\n");
            return 0;
        }   
    }
    else {
        printError("Bind Failed\n");
    }
}


int isUpload(char* msg) {
    if (strlen(msg) >= UPLOAD_TOKEN_MIN_LENGTH && msg[0] == 'u' && msg[1] == 'p' && msg[2] == 'l' && msg[3] == 'o' &&
        msg[4] == 'a' && msg[5] == 'd' && msg[6] == ' ') return 1;
    else return 0;
}

int isDownload(char* msg) {
    if (strlen(msg) >= DOWNLOAD_TOKEN_MIN_LENGTH && msg[0] == 'd' && msg[1] == 'o' && msg[2] == 'w' && msg[3] == 'n' &&
        msg[4] == 'l' && msg[5] == 'o' && msg[6] == 'a' && msg[7] == 'd' && msg[8] == ' ') return 1;
    else return 0;
}

int analyzeMsg(char* msg) {
    if (isUpload(msg)) {
        return UPLOAD;
    }
    else if (isDownload(msg)) {
        return DOWNLOAD;
    }
}

void clear(char* ptr) {
    for (int i = 0 ; i < MAX_DATA_SIZE ; i++) {
        ptr[i] = '\0';
    }
}

void sendFileToServer(int sockid, int fd, char* response, char* text) {
    recv(sockid, response, MAX_DATA_SIZE, 0);
    if (strcmp(response, OK) == EQUAL) {
        while (read(fd, text, MAX_DATA_SIZE) != 0) {
            if (send(sockid, text, strlen(text), 0) != strlen(text)) printError("Send Failed\n");
            recv(sockid, response, MAX_DATA_SIZE, 0);
            clear(text);
        }
        send(sockid, DONE, strlen(DONE), 0);
        recv(sockid, response, MAX_DATA_SIZE, 0);
        printmsg("\e[0;32m");
        printmsg(response);
        printmsg("\x1B[37m\n");
        close(fd);
    }
}

int getFileFromServer(int sockid, char* response, char* filenamePointer, char* text) {
    recv(sockid, response, MAX_DATA_SIZE, 0);
    if (strcmp(response, OK) == EQUAL) {
        if (send(sockid, filenamePointer, strlen(filenamePointer), 0) != strlen(filenamePointer)) printError("Send Failed\n");
        recv(sockid, response, MAX_DATA_SIZE, 0);

        if (atoi(response) == FAILED) {
            printmsg("Server doesn't have the file.Checking the other nodes for the file...\n");
            return SERVER_DOES_NOT_HAVE_THE_FILE;
        } 

        send(sockid, "ready", strlen("ready"), 0);
        recv(sockid, text, MAX_DATA_SIZE, 0);
        int fd = open(filenamePointer, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        while (strcmp(text, DONE) != EQUAL) {
            write(fd, text, strlen(text));
            clear(text);
            send(sockid, OK, strlen(OK), 0);
            recv(sockid, text, MAX_DATA_SIZE, 0);
        }
        close(fd);
        printmsg("\e[0;32mDownload Was Successfull!");
        printmsg("\x1B[37m\n");
    }
}

int serverCLientMode(int sockid, char* filename) {
    char* text = (char*) malloc(MAX_DATA_SIZE * sizeof(char)), 
        * response = (char*) malloc(MAX_DATA_SIZE * sizeof(char)),
        * filenamePointer;

    clear(text);
    clear(response);

    int status = analyzeMsg(filename);
    if (status == UPLOAD) {
        filenamePointer = &filename[UPLOAD_TOKEN_MIN_LENGTH-1];
        int fd = open(filenamePointer, O_RDONLY);

        if (fd != FAILED) {
            if (send(sockid, "upload", strlen("upload"), 0) != strlen("upload")) printError("Send Failed\n");
            else {
                recv(sockid, response, MAX_DATA_SIZE, 0);
                if (strcmp(response, OK) == EQUAL) {
                    if (send(sockid, filenamePointer, strlen(filenamePointer), 0) != strlen(filenamePointer)) printError("Send Failed\n");
                    else sendFileToServer(sockid, fd, response, text);
                }
            }
        }
        else {
            printmsg("\e[0;31mYou don't have the file you want to upload!\n");
            printmsg("\x1B[37m\n");
        }
        
    }

    else if (status == DOWNLOAD) {
        filenamePointer = &filename[DOWNLOAD_TOKEN_MIN_LENGTH-1];
        if (send(sockid, "download", strlen("download"), 0) != strlen("download")) printError("Send Failed\n");
        else {
            if (getFileFromServer(sockid, response, filenamePointer, text) == SERVER_DOES_NOT_HAVE_THE_FILE) {
                return SERVER_DOES_NOT_HAVE_THE_FILE;
            }
        }
    }
    free(text);
    free(response);
    return 0;
}


int isPort(char* txt) {
    if (strlen(txt) >= PORT_TOKEN_MIN_LENGTH && txt[0] == '!' && txt[1] == 'p' && txt[2] == 'o' && txt[3] == 'r' 
        && txt[4] == 't') return 1;
    else return 0; 
}

int isFileRequest(char* txt) {
    if (strlen(txt) >= FILE_TOKEN_MIN_LENGTH && txt[0] == '!' && txt[1] == 'f' && txt[2] == 'i' && txt[3] == 'l' 
        && txt[4] == 'e' && txt[5] == ' ') return 1;
    else return 0;
}


char* extractFilename(char* txt) {
    int i = 5;
    while (txt[i] != SPACE) i++;
    i++;
    return &(txt[i]);

}

int extractPort(char* txt) {
    int i = 5;
    char* temp = (char*) malloc(MAX_PORT_BITS * sizeof(char));
    while (txt[i] != SPACE) {
        temp[i-5] = txt[i];
        i++;
    }
    i = atoi(temp);
    free(temp);
    return i;
}

void sendFileToAnotherNode(int sockid, char*text, char*data)  {
    clear(text);
    int fd = open(data, O_RDONLY);
    while(read(fd, text, MAX_DATA_SIZE) != EMPTY) {
        if (send(sockid, text, strlen(text), 0) != strlen(text)) printError("Send Failed\n");
        recv(sockid, data, MAX_DATA_SIZE, 0);
        clear(text);
    }

    send(sockid, DONE, strlen(DONE), 0);
    close(fd);
    printmsg("\e[0;32m");
    printmsg("Transfer Completed!");
    printmsg("\x1B[37m\n");
}  

void getFileFromAnotherNode(int fd, int connectionSock, char* response, char* text, char* filename) {
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
    printmsg("\e[0;32m");
    printmsg("File got received successfully!");
    printmsg("\x1B[37m\n");
    close(fd);
}

void desiredFileBroadcastHandler(int sig) {
    if (strlen(filenameToSend) != 0) {
        if (sendto(broadcastSock, filenameToSend, strlen(filenameToSend), 0, (struct sockaddr *)&broadcastaddr, sizeof(broadcastaddr)) != strlen(filenameToSend)) printError("Filename Send Failed!\n"); 
        alarm(SEND_DELAY);
    }
} 


void printHelp() {
    printmsg("\e[0;36m\nHelp:\n");
    printmsg("Welcome! Here are the possible commands:\n");
    printmsg("1- If you want to download a file enter \"download <filename>\"\n");
    printmsg("2- If you want to upload a file enter \"upload <filename>\"\n");
    printmsg("3- If you want to quit the system enter \"exit\"\nNow enter your command:\n");
    printmsg("\x1B[37m\n");
}


void handlePortTokenInput(char* clientPort, char* text, char* filename, char* data, char* response, struct sockaddr_in* clientaddr) {
    int fd, portnumber, connectionSock;
    clear(text);
    strcat(text, extractFilename(data));
    if (strcmp(filename, text) == EQUAL) {
        portnumber = extractPort(data);
        if (portnumber != atoi(clientPort)) {
            printmsg("Connected to get desired file...\n");
            clientaddr->sin_port = htons((uint16_t)portnumber);

            connectionSock = socket(PF_INET, SOCK_STREAM, 0);
            if (connectionSock == FAILED) printError("Creating Socket Failed! \n");
            setOptions(connectionSock);
            if (connect(connectionSock, (struct sockaddr*) clientaddr, sizeof(*clientaddr)) == FAILED) printError("Connection Failed\n");
            
            fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
            clear(text);
            clear(response);
            if (fd != FAILED) {
                getFileFromAnotherNode(fd, connectionSock, response, text, filename);
                clear(filenameToSend);
                clear(filename);
            }
            clientaddr->sin_port = htons((uint16_t)atoi(clientPort));
            close(connectionSock);
        }                    
    }
}



void startCommunication(char* broadcastPort, char* clientPort) {
    int fd, connectionSock, portnumber, maxfd, tempsock, clientServerSock;
    fd_set readfds;

    int clients[QUEUE_LIMIT];
    for (int i = 0 ; i < QUEUE_LIMIT ; i++) clients[i] = EMPTY;

    broadcastSock = socket(PF_INET, SOCK_DGRAM, 0);
    setOptions(broadcastSock);

    broadcastaddr.sin_family = AF_INET;
    broadcastaddr.sin_port = htons((uint16_t)atoi(broadcastPort));
    broadcastaddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    socklen_t broadcastSize = sizeof(broadcastaddr);

    if (bind(broadcastSock, (struct sockaddr*) &broadcastaddr, broadcastSize) == FAILED) printError("Bind Failed\n");

    int clientSock = socket(PF_INET, SOCK_STREAM, 0);
    setOptions(clientSock);
    clients[0] = clientSock;

    struct sockaddr_in clientaddr;
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_port = htons((uint16_t)atoi(clientPort));
    clientaddr.sin_addr.s_addr = inet_addr(LOCALHOST);
    
    struct sockaddr_in * receiveraddr;


    if (bind(clientSock, (struct sockaddr*) &clientaddr, sizeof(clientaddr)) == FAILED) printError("Bind Failed\n");
    if (listen( clientSock, QUEUE_LIMIT) == FAILED) printError("Listen Failed\n");
    
    if (server != NOT_AVAILABLE) {
        clientServerSock = socket(PF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serveraddr;
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons((uint16_t)server);
        serveraddr.sin_addr.s_addr = inet_addr(LOCALHOST);
        if (connect(clientServerSock, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) == FAILED) printError("Connection To Server Failed\n");
    }

    char* filename = (char*) malloc(MAX_DATA_SIZE * sizeof(char)), 
        * text = (char*) malloc(MAX_DATA_SIZE * sizeof(char)),
        * data = (char*) malloc(MAX_DATA_SIZE * sizeof(char)), 
        * response = (char*) malloc(MAX_DATA_SIZE * sizeof(char)),
        * filenamePointer = (char*) malloc(MAX_DATA_SIZE * sizeof(char));

    printmsg("\e[1;33mEnter your command. Enter \"help\" for help!");    
    printmsg("\x1B[37m\n");
    
    while(TRUE) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(broadcastSock, &readfds);
        maxfd = broadcastSock;
        for (int i = 0 ; i < QUEUE_LIMIT ; i++) {
            if (clients[i] != EMPTY) {
                FD_SET(clients[i], &readfds);
                if (clients[i] > maxfd) maxfd = clients[i];
            }
        }

        if (select(maxfd+1, &readfds, NULL, NULL, NULL) == FAILED) continue;
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            clear(text);
            read(STDIN_FILENO, text, MAX_DATA_SIZE);
            text[strlen(text)-1] = '\0';
            if (strcmp(text, EXIT) == EQUAL) {
                printmsg("Goodbye.See you!\n");
                return;
            }
            if (strlen(filenameToSend) != 0) {
                printmsg("\e[0;31mYou haven't received the file you asked last time.Wait until it gets finished");
                printmsg("\x1B[37m\n");
                continue;
            }
            else if (isUpload(text)) {
                if (server == NOT_AVAILABLE) {
                    printmsg("\e[0;31mUpload feature is not possible due to server unavailability");
                    printmsg("\x1B[37m\n");
                }
                else serverCLientMode(clientServerSock, text);
                continue;
            }
            else if (isDownload(text)) {
                if (server != NOT_AVAILABLE && serverCLientMode(clientServerSock, text) != SERVER_DOES_NOT_HAVE_THE_FILE) continue;
                clear(filenameToSend);
                strcat(filenameToSend, FILE_TOKEN);
                strcat(filenameToSend, &text[DOWNLOAD_TOKEN_MIN_LENGTH-1]);
                clear(filename);
                strcat(filename, &filenameToSend[FILE_TOKEN_MIN_LENGTH-1]);
                signal(SIGALRM, desiredFileBroadcastHandler);
                desiredFileBroadcastHandler(1);
                printmsg("File request was sent to other nodes.\n");
            }
            else if(strcmp(text, HELP) == EQUAL) {
                printHelp();
                continue;
            }
            else {
                printmsg("\e[0;31mInvalid Input.If you want to know the rules enter \"help\"");
                printmsg("\x1B[37m\n");
                continue;
            }
        }

        if(FD_ISSET(broadcastSock, &readfds)) {
            clear(data);
            recvfrom(broadcastSock, data, MAX_DATA_SIZE, 0, (struct sockaddr*)& broadcastaddr, &broadcastSize);
            broadcastaddr.sin_addr.s_addr = INADDR_BROADCAST;
            if (isPort(data)) {
                handlePortTokenInput(clientPort, text, filename, data, response, &clientaddr);
            }
            else if (isFileRequest(data) && strcmp(filename, &data[FILE_TOKEN_MIN_LENGTH-1]) != EQUAL) {
                clear(filenamePointer);
                strcat(filenamePointer, &data[FILE_TOKEN_MIN_LENGTH-1]);  
                fd = open(filenamePointer, O_RDONLY);
                if (fd != FAILED) {
                    clear(response);
                    strcat(response, PORT_TOKEN);
                    strcat(response, clientPort);
                    strcat(response, " ");
                    strcat(response, filenamePointer);
                    if (sendto(broadcastSock, response, strlen(response), 0,(struct sockaddr*)& broadcastaddr, broadcastSize) != strlen(response)) printError("Send Failed\n");
                    close(fd);   
                }
            }
        }

        for (int i = 1 ; i < QUEUE_LIMIT ; i++) {
            if  (clients[i] != EMPTY) {
                tempsock = clients[i];
                if (FD_ISSET(tempsock, &readfds)) {
                    if (recv(tempsock, data, MAX_DATA_SIZE, 0) == CLOSED) {
                        clients[i] = EMPTY;
                        close(tempsock);
                        free(receiveraddr);
                    }
                    else {
                        send(tempsock, OK, strlen(OK), 0);
                        clear(data);
                        recv(tempsock, data, MAX_DATA_SIZE, 0);
                        printmsg("A user got connected to receive a file...\n");
                        sendFileToAnotherNode(tempsock, text, data);
                    }
                }
            }
        }

        if (FD_ISSET(clientSock, &readfds)) {
            receiveraddr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
            socklen_t receiverSize = sizeof(*receiveraddr);
            if ((tempsock = accept(clientSock, (struct sockaddr*) receiveraddr, &receiverSize)) == FAILED) printError("Accept Error!\n");
            for (int i = 1 ; i < QUEUE_LIMIT ; i++) {
                if (clients[i] == EMPTY) {
                    clients[i] = tempsock;
                    break;
                }
            }
        }
        

    }

}


int main(int argc, char* argv[]) {
    if (argc != 4) printError("Invalid Input for program to run!\n");
    printmsg("Looking for server...\n");
    serverExists(argv[1]);
    filenameToSend = (char*) malloc(MAX_DATA_SIZE * sizeof(char));
    clear(filenameToSend);
    startCommunication(argv[2], argv[3]);
    return 0;
}
