// server.h
#ifndef LIST
#define LIST

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "proto.h"

typedef struct MessageNode
{
    int message_id;
    char message[LENGTH_MSG];
    struct MessageNode *next;
} MessageList;

typedef struct ClientNode
{
    int data;
    struct ClientNode *prev;
    struct ClientNode *link;
    char ip[16];
    char name[31];
    MessageList *messages; // Linked list of messages for each client
} ClientList;

ClientList *newNode(int sockfd, char *ip)
{
    ClientList *np = (ClientList *)malloc(sizeof(ClientList));
    void add_message(ClientList * client, int message_id, const char *message);
    void delete_message(ClientList * client, int message_id);
    void list_messages(ClientList * client);
    void search_message(ClientList * client, const char *search_text);
    if (np == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    np->data = sockfd;
    np->prev = NULL;
    np->link = NULL;

    // Use strncpy with appropriate buffer size
    strncpy(np->ip, ip, sizeof(np->ip) - 1);
    np->ip[sizeof(np->ip) - 1] = '\0'; // Ensure null-termination

    strncpy(np->name, "NULL", sizeof(np->name) - 1);
    np->name[sizeof(np->name) - 1] = '\0'; // Ensure null-termination

    return np;
}

#endif // LIST
