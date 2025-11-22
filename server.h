#ifndef SERVER_H
#define SERVER_H

/* System Header Files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <ctype.h>
#include <pthread.h>

/* Local Header Files */
#include "list.h"

#define MAX_READERS 25
#define TRUE   1  
#define FALSE  0  
#define PORT 8888  
#define delimiters " "
#define max_clients  30
#define DEFAULT_ROOM "Lobby"
#define MAXBUFF   2096
#define BACKLOG 2 

// global variables provided in server.c
extern int chat_serv_sock_fd;
extern int numReaders;
extern pthread_mutex_t mutex;
extern pthread_mutex_t rw_lock;

// global user list head (defined in server.c)
extern struct node *head;

// global room list head (defined in list.c)
extern struct room *room_head;

// Message of the day
extern char const *server_MOTD;

// prototypes

int get_server_socket();
int start_server(int serv_socket, int backlog);
int accept_client(int serv_sock);
void sigintHandler(int sig_num);
void *client_receive(void *ptr);

// reader / writer helpers
void start_read();
void end_read();
void start_write();
void end_write();

#endif
