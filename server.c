// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "proto.h"
#include "server.h"

// Global variables
int server_sockfd = 0;
int shmid; // Shared memory ID
ClientList *root, *now;

// Function prototypes
void catch_ctrl_c_and_exit(int sig);                         // Signal handler for Ctrl+C
void send_to_all_clients(ClientList *np, char tmp_buffer[]); // Send a message to all clients
void client_handler(void *p_client);                         // Thread function to handle a client

// Function prototypes for managing messages
void add_message(ClientList *client, int message_id, const char *message);
void delete_message(ClientList *client, int message_id);
void list_messages(ClientList *client);
void search_message(ClientList *client, const char *search_text);

int main()
{
    // Shared memory initialization for client list
    shmid = shmget(IPC_PRIVATE, sizeof(ClientList), 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment
    root = (ClientList *)shmat(shmid, (void *)0, 0);
    if (root == (ClientList *)(-1))
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Signal handling for Ctrl+C
    signal(SIGINT, catch_ctrl_c_and_exit);

    // Create socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd == -1)
    {
        printf("Fail to create a socket.");
        exit(EXIT_FAILURE);
    }

    // Socket information
    struct sockaddr_in server_info, client_info;
    int s_addrlen = sizeof(server_info);
    int c_addrlen = sizeof(client_info);
    memset(&server_info, 0, s_addrlen);
    memset(&client_info, 0, c_addrlen);
    server_info.sin_family = PF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(8888);

    // Bind and Listen
    bind(server_sockfd, (struct sockaddr *)&server_info, s_addrlen);
    listen(server_sockfd, 5);

    // Print Server IP
    getsockname(server_sockfd, (struct sockaddr *)&server_info, (socklen_t *)&s_addrlen);
    printf("Start Server on: %s:%d\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));

    // Initial linked list for clients
    root->link = NULL;
    now = root;

    // Fork to create the child process (server)
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    { // Child process (server)
        // Fork a thread for each client
        while (1)
        {
            int client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_info, (socklen_t *)&c_addrlen);

            // Print Client IP
            getpeername(client_sockfd, (struct sockaddr *)&client_info, (socklen_t *)&c_addrlen);
            printf("Client %s:%d come in.\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));

            // Append linked list for clients
            ClientList *c = newNode(client_sockfd, inet_ntoa(client_info.sin_addr));
            c->prev = now;
            now->link = c;
            now = c;

            pthread_t id;
            if (pthread_create(&id, NULL, (void *)client_handler, (void *)c) != 0)
            {
                perror("Create pthread error!\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    { // Parent process
        // Wait for the child process to terminate
        waitpid(pid, NULL, 0);
    }

    return 0;
}

// Signal handler for Ctrl+C
void catch_ctrl_c_and_exit(int sig)
{
    // Clean up shared memory
    shmdt((void *)root);
    shmctl(shmid, IPC_RMID, NULL);

    printf("Bye\n");
    exit(EXIT_SUCCESS);
}

// Send a message to all clients
void send_to_all_clients(ClientList *np, char tmp_buffer[])
{
    ClientList *tmp = root->link;
    while (tmp != NULL)
    {
        if (np->data != tmp->data)
        {
            send(tmp->data, tmp_buffer, LENGTH_SEND, 0);
        }
        tmp = tmp->link;
    }
}

// Thread function to handle a client
void client_handler(void *p_client)
{
    int leave_flag = 0;
    char nickname[LENGTH_NAME] = {};
    char recv_buffer[LENGTH_MSG] = {};
    char send_buffer[LENGTH_SEND] = {};
    ClientList *np = (ClientList *)p_client;

    // Naming
    if (recv(np->data, nickname, LENGTH_NAME, 0) <= 0 || strlen(nickname) < 2 || strlen(nickname) >= LENGTH_NAME - 1)
    {
        printf("%s didn't input name.\n", np->ip);
        leave_flag = 1;
    }
    else
    {
        strncpy(np->name, nickname, LENGTH_NAME);
        printf("%s(%s)(%d) joined the chatroom.\n", np->name, np->ip, np->data);
        sprintf(send_buffer, "%s(%s) joined the chatroom.", np->name, np->ip);
        send_to_all_clients(np, send_buffer);
    }

    // Conversation
    while (1)
    {
        if (leave_flag)
        {
            break;
        }
        int receive = recv(np->data, recv_buffer, LENGTH_MSG, 0);
        if (receive > 0)
        {
            if (strlen(recv_buffer) == 0)
            {
                continue;
            }
            sprintf(send_buffer, "%s: %s from %s", np->name, recv_buffer, np->ip);
        }
        else if (receive == 0 || strcmp(recv_buffer, "exit") == 0)
        {
            printf("%s(%s)(%d) left the chatroom.\n", np->name, np->ip, np->data);
            sprintf(send_buffer, "%s(%s) left the chatroom.", np->name, np->ip);
            leave_flag = 1;
        }
        else
        {
            printf("Fatal Error: -1\n");
            leave_flag = 1;
        }
        send_to_all_clients(np, send_buffer);
    }

    // Remove Node
    close(np->data);
    if (np == now)
    { // remove an edge node
        now = np->prev;
        now->link = NULL;
    }
    else
    { // remove a middle node
        np->prev->link = np->link;
        np->link->prev = np->prev;
    }
    free(np);
}

// Function to add a message to a client's message list
void add_message(ClientList *client, int message_id, const char *message)
{
    MessageList *new_message = (MessageList *)malloc(sizeof(MessageList));
    if (new_message == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    new_message->message_id = message_id;
    strncpy(new_message->message, message, sizeof(new_message->message) - 1);
    new_message->message[sizeof(new_message->message) - 1] = '\0'; // Ensure null-termination
    new_message->next = NULL;

    if (client->messages == NULL)
    {
        client->messages = new_message;
    }
    else
    {
        MessageList *current = client->messages;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = new_message;
    }
}

// Function to delete a message from a client's message list
void delete_message(ClientList *client, int message_id)
{
    MessageList *current = client->messages;
    MessageList *prev = NULL;

    while (current != NULL && current->message_id != message_id)
    {
        prev = current;
        current = current->next;
    }

    if (current == NULL)
    {
        printf("Message with ID %d not found.\n", message_id);
        return;
    }

    if (prev == NULL)
    {
        client->messages = current->next;
    }
    else
    {
        prev->next = current->next;
    }

    free(current);
}

// Function to list all messages for a client
void list_messages(ClientList *client)
{
    MessageList *current = client->messages;

    while (current != NULL)
    {
        printf("%d: %s\n", current->message_id, current->message);
        current = current->next;
    }
}

// Function to search for messages containing a specific text for a client
void search_message(ClientList *client, const char *search_text)
{
    MessageList *current = client->messages;

    while (current != NULL)
    {
        if (strstr(current->message, search_text) != NULL)
        {
            printf("%d: %s\n", current->message_id, current->message);
        }
        current = current->next;
    }
}
