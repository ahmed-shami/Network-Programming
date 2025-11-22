#include "server.h"

int chat_serv_sock_fd; // server socket

/////////////////////////////////////////////
// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE

int numReaders = 0; // keep count of the number of readers

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex lock
pthread_mutex_t rw_lock = PTHREAD_MUTEX_INITIALIZER;  // read/write lock

/////////////////////////////////////////////

char const *server_MOTD = "Thanks for connecting to the BisonChat Server.\n\nchat>";

struct node *head = NULL;

// reader / writer lock helpers

void start_read() {
    pthread_mutex_lock(&mutex);
    numReaders++;
    if (numReaders == 1) {
        pthread_mutex_lock(&rw_lock);   // first reader locks writers out
    }
    pthread_mutex_unlock(&mutex);
}

void end_read() {
    pthread_mutex_lock(&mutex);
    numReaders--;
    if (numReaders == 0) {
        pthread_mutex_unlock(&rw_lock); // last reader unlocks
    }
    pthread_mutex_unlock(&mutex);
}

void start_write() {
    pthread_mutex_lock(&rw_lock);       // exclusive access
}

void end_write() {
    pthread_mutex_unlock(&rw_lock);
}

int get_server_socket() {
    int opt = TRUE;   
    int master_socket;
    struct sockaddr_in address; 
    
    // create a master socket  
    if ((master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) {   
        perror("socket failed");   
        exit(EXIT_FAILURE);   
    }   
     
    // allow multiple connections
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {   
        perror("setsockopt");   
        exit(EXIT_FAILURE);   
    }   
     
    // type of socket created  
    address.sin_family = AF_INET;   
    address.sin_addr.s_addr = INADDR_ANY;   
    address.sin_port = htons(PORT);   
         
    // bind the socket to localhost port 8888  
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {   
        perror("bind failed");   
        exit(EXIT_FAILURE);   
    }   

    return master_socket;
}

int start_server(int serv_socket, int backlog) {
   int status = 0;
   if ((status = listen(serv_socket, backlog)) == -1) {
      printf("socket listen error\n");
   }
   return status;
}

int accept_client(int serv_sock) {
   int reply_sock_fd = -1;
   socklen_t sin_size = sizeof(struct sockaddr_storage);
   struct sockaddr_storage client_addr;

   if ((reply_sock_fd = accept(serv_sock,(struct sockaddr *)&client_addr, &sin_size)) == -1) {
      printf("socket accept error\n");
   }
   return reply_sock_fd;
}

int main(int argc, char **argv) {

   signal(SIGINT, sigintHandler);
    
   // create the default room
   start_write();
   createRoom(DEFAULT_ROOM);
   end_write();

   // Open server socket
   chat_serv_sock_fd = get_server_socket();

   // get ready to accept connections
   if (start_server(chat_serv_sock_fd, BACKLOG) == -1) {
      printf("start server error\n");
      exit(1);
   }
   
   printf("Server Launched! Listening on PORT: %d\n", PORT);
    
   // Main execution loop
   while (1) {
      int new_client = accept_client(chat_serv_sock_fd);
      if (new_client != -1) {
         pthread_t new_client_thread;
         pthread_create(&new_client_thread, NULL, client_receive, (void *)&new_client);
         pthread_detach(new_client_thread);
      }
   }

   close(chat_serv_sock_fd);
}

/* Handle SIGINT (CTRL+C) */
void sigintHandler(int sig_num) {
   printf("Error:Forced Exit.\n");

   start_write();  // block other threads while shutting down

   printf("--------CLOSING ACTIVE USERS--------\n");

   // close all client sockets
   struct node *u = head;
   while (u != NULL) {
       close(u->socket);
       u = u->next;
   }

   // free all rooms and room memberships
   struct room *r = room_head;
   while (r != NULL) {
       struct room_user *ru = r->users;
       while (ru != NULL) {
           struct room_user *tmp = ru;
           ru = ru->next;
           free(tmp);
       }
       struct room *rtmp = r;
       r = r->next;
       free(rtmp);
   }

   // free all users and their DM lists
   u = head;
   while (u != NULL) {
       struct dm_conn *d = u->dm_head;
       while (d != NULL) {
           struct dm_conn *dt = d;
           d = d->next;
           free(dt);
       }
       struct node *utmp = u;
       u = u->next;
       free(utmp);
   }

   end_write();

   close(chat_serv_sock_fd);
   exit(0);
}
