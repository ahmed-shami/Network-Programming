#ifndef LIST_H
#define LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Forward declarations so we can use pointers between structs
struct node;
struct room;
struct room_user;
struct dm_conn;

// DM connections per user
struct dm_conn {
    struct node *peer;           // the other user in a DM
    struct dm_conn *next;
};

// user node
struct node {
    char username[30];
    int socket;
    struct node *next;
    struct dm_conn *dm_head;   // head of DM connections list
};

// room membership node (linked list of users in a room)
struct room_user {
    struct node *user;           // pointer to user node
    struct room_user *next;
};

// room list node
struct room {
    char name[30];
    struct room_user *users;     // linked list of users in this room
    struct room *next;
};

/////////////////// USERLIST //////////////////////////

// insert node at the first location (if username not already present)
struct node* insertFirstU(struct node *head, int socket, char *username);

// find a node with given username
struct node* findU(struct node *head, char* username);

// find a node with given socket
struct node* findUBySocket(struct node *head, int socket);

/////////////////// ROOMLIST //////////////////////////

// global head of room list, defined in list.c
extern struct room *room_head;

// find room by name
struct room* findRoom(char *roomname);

// create room, return pointer (creates if missing)
struct room* createRoom(char *roomname);

// add user to room
int addUserToRoom(struct room *room, struct node *user);

// remove user from room
int removeUserFromRoom(struct room *room, struct node *user);

// list rooms into buffer
void listRooms(struct room *head, char *buffer, int maxlen);

// list users into buffer
void listUsers(struct node *user_head, char *buffer, int maxlen);

// delete empty rooms except the default room name
void deleteEmptyRooms(const char *default_room_name);

/////////////////// DM CONNECTIONS //////////////////////////

// add a DM connection between two users (bidirectional)
int addDM(struct node *userA, struct node *userB);

// remove a DM connection between two users (bidirectional)
int removeDM(struct node *userA, struct node *userB);

// return 1 if users are in DM, 0 otherwise
int isDM(struct node *userA, struct node *userB);

#endif
