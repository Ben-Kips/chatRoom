// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "proto.h"
#include "string.h"
#include "server.h"

// Global variables
volatile sig_atomic_t flag = 0;  // Flag to indicate program termination
int sockfd = 0;  // Socket file descriptor
char nickname[LENGTH_NAME] = {};  // User's chosen nickname

// Function prototypes
void catch_ctrl_c_and_exit(int sig);  // Signal handler for Ctrl+C
void recv_msg_handler();  // Thread function to handle incoming messages
void send_msg_handler();  // Thread function to handle outgoing messages

int main() {
    // Shared memory initialization for client list
    int shmid = shmget(IPC_PRIVATE, sizeof(ClientList), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment
    ClientList *root = (ClientList *)shmat(shmid, (void *)0, 0);
    if (root == (ClientList *)(-1)) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Signal handling for Ctrl+C
    signal(SIGINT, catch_ctrl_c_and_exit);

    // User naming
    printf("Please enter your name: ");
    if (fgets(nickname, LENGTH_NAME, stdin) != NULL) {
        str_trim_lf(nickname, LENGTH_NAME);
    }
    if (strlen(nickname) < 2 || strlen(nickname) >= LENGTH_NAME - 1) {
        printf("\nName must be more than one and less than thirty characters.\n");
        exit(EXIT_FAILURE);
    }

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Fail to create a socket.");
        exit(EXIT_FAILURE);
    }

    // Set up socket information for server connection
    struct sockaddr_in server_info, client_info;
    int s_addrlen = sizeof(server_info);
    int c_addrlen = sizeof(client_info);
    memset(&server_info, 0, s_addrlen);
    memset(&client_info, 0, c_addrlen);
    server_info.sin_family = PF_INET;
    server_info.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_info.sin_port = htons(8888);

    // Connect to the server
    int err = connect(sockfd, (struct sockaddr *)&server_info, s_addrlen);
    if (err == -1) {
        printf("Connection to Server error!\n");
        exit(EXIT_FAILURE);
    }
      // Display author name after connection is established
    printf("Title: Group 5 Chat App\n");

    // Display connection information
    getsockname(sockfd, (struct sockaddr *)&client_info, (socklen_t *)&c_addrlen);
    getpeername(sockfd, (struct sockaddr *)&server_info, (socklen_t *)&s_addrlen);
    printf("Connected to Server: %s:%d\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));
    printf("You are: %s:%d\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));

    // Send client's nickname to the server
    send(sockfd, nickname, LENGTH_NAME, 0);

    // Detach shared memory
    shmdt((void *)root);

    // Create threads for sending and receiving messages
    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *)send_msg_handler, NULL) != 0) {
        printf("Create pthread error!\n");
        exit(EXIT_FAILURE);
    }

    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void *)recv_msg_handler, NULL) != 0) {
        printf("Create pthread error!\n");
        exit(EXIT_FAILURE);
    }

    // Main loop to wait for program termination signal
    while (1) {
        if (flag) {
            printf("\nBye\n");
            break;
        }
    }

    // Close the socket
    close(sockfd);

    return 0;
}

// Signal handler for Ctrl+C
void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

// Thread function to handle incoming messages
void recv_msg_handler() {
    char receiveMessage[LENGTH_SEND] = {};
    while (1) {
        int receive = recv(sockfd, receiveMessage, LENGTH_SEND, 0);
        if (receive > 0) {
            printf("\r%s\n", receiveMessage);
            str_overwrite_stdout();
        } else if (receive == 0) {
            break;  // Server closed the connection
        } else {
            // Handle receive error (-1)
        }
    }
}

// Thread function to handle outgoing messages
void send_msg_handler() {
    char message[LENGTH_MSG] = {};
    while (1) {
        str_overwrite_stdout();
        while (fgets(message, LENGTH_MSG, stdin) != NULL) {
            str_trim_lf(message, LENGTH_MSG);
            if (strlen(message) == 0) {
                str_overwrite_stdout();
            } else {
                break;
            }
        }
        send(sockfd, message, LENGTH_MSG, 0);
        if (strcmp(message, "exit") == 0) {
            break;  // User initiated exit
        }
    }
    catch_ctrl_c_and_exit(2);  // Thread exits
}
