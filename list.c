#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "list.h"

// global room list head
struct room *room_head = NULL;

// insert link at the first location in user list
struct node* insertFirstU(struct node *head, int socket, char *username) {
    if (findU(head, username) == NULL) {
        struct node *link = (struct node*) malloc(sizeof(struct node));
        if (!link) {
            perror("malloc");
            return head;
        }

        link->socket = socket;
        strncpy(link->username, username, sizeof(link->username) - 1);
        link->username[sizeof(link->username) - 1] = '\0';
        link->dm_head = NULL;

        link->next = head;
        head = link;
    } else {
        printf("Duplicate: %s\n", username);
    }
    return head;
}

// find a node with given username
struct node* findU(struct node *head, char* username) {
    struct node* current = head;

    if (head == NULL) {
        return NULL;
    }

    while (current != NULL && strcmp(current->username, username) != 0) {
        current = current->next;
    }

    return current;
}

// find a node with given socket
struct node* findUBySocket(struct node *head, int socket) {
    struct node *current = head;
    while (current != NULL) {
        if (current->socket == socket) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

////////////////////// ROOM HELPERS /////////////////////////

struct room* findRoom(char *roomname) {
    struct room *curr = room_head;
    while (curr != NULL) {
        if (strcmp(curr->name, roomname) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

struct room* createRoom(char *roomname) {
    struct room *existing = findRoom(roomname);
    if (existing != NULL) {
        return existing;
    }

    struct room *r = (struct room*) malloc(sizeof(struct room));
    if (!r) {
        perror("malloc");
        return NULL;
    }

    strncpy(r->name, roomname, sizeof(r->name) - 1);
    r->name[sizeof(r->name) - 1] = '\0';
    r->users = NULL;

    // insert at front of global room list
    r->next = room_head;
    room_head = r;

    return r;
}

int addUserToRoom(struct room *room, struct node *user) {
    if (!room || !user) return -1;

    // check if already in room
    struct room_user *cur = room->users;
    while (cur != NULL) {
        if (cur->user == user) {
            return 0;   // already in room
        }
        cur = cur->next;
    }

    struct room_user *ru = (struct room_user*) malloc(sizeof(struct room_user));
    if (!ru) {
        perror("malloc");
        return -1;
    }
    ru->user = user;
    ru->next = room->users;
    room->users = ru;

    return 0;
}

int removeUserFromRoom(struct room *room, struct node *user) {
    if (!room || !user) return -1;

    struct room_user *cur = room->users;
    struct room_user *prev = NULL;

    while (cur != NULL) {
        if (cur->user == user) {
            if (prev == NULL) {
                room->users = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    return -1;
}

void listUsers(struct node *user_head, char *buffer, int maxlen) {
    buffer[0] = '\0';
    strncat(buffer, "Users:\n", maxlen - strlen(buffer) - 1);

    struct node *cur = user_head;
    while (cur != NULL) {
        if ((int)strlen(buffer) >= maxlen - 40) break;

        strncat(buffer, "  ", maxlen - strlen(buffer) - 1);
        strncat(buffer, cur->username, maxlen - strlen(buffer) - 1);
        strncat(buffer, "\n", maxlen - strlen(buffer) - 1);

        cur = cur->next;
    }
}

void listRooms(struct room *head, char *buffer, int maxlen) {
    buffer[0] = '\0';
    strncat(buffer, "Rooms:\n", maxlen - strlen(buffer) - 1);

    struct room *cur = head;
    while (cur != NULL) {
        if ((int)strlen(buffer) >= maxlen - 40) break;

        strncat(buffer, "  ", maxlen - strlen(buffer) - 1);
        strncat(buffer, cur->name, maxlen - strlen(buffer) - 1);
        strncat(buffer, "\n", maxlen - strlen(buffer) - 1);

        cur = cur->next;
    }
}

// delete empty rooms except the default room name
void deleteEmptyRooms(const char *default_room_name) {
    struct room *cur = room_head;
    struct room *prev = NULL;

    while (cur != NULL) {
        if (cur->users == NULL &&
            default_room_name != NULL &&
            strcmp(cur->name, default_room_name) != 0) {

            struct room *tmp = cur;
            if (prev == NULL) {
                room_head = cur->next;
                cur = room_head;
            } else {
                prev->next = cur->next;
                cur = cur->next;
            }
            free(tmp);
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}

////////////////////// DM HELPERS /////////////////////////

// internal helper to check if peer exists in a list
static int dmContains(struct dm_conn *head, struct node *peer) {
    struct dm_conn *cur = head;
    while (cur != NULL) {
        if (cur->peer == peer) {
            return 1;
        }
        cur = cur->next;
    }
    return 0;
}

int isDM(struct node *userA, struct node *userB) {
    if (!userA || !userB) return 0;
    return dmContains(userA->dm_head, userB);
}

int addDM(struct node *userA, struct node *userB) {
    if (!userA || !userB) return -1;
    if (userA == userB) return -1;

    // if already connected, do nothing
    if (dmContains(userA->dm_head, userB) || dmContains(userB->dm_head, userA)) {
        return 0;
    }

    struct dm_conn *ab = (struct dm_conn*) malloc(sizeof(struct dm_conn));
    struct dm_conn *ba = (struct dm_conn*) malloc(sizeof(struct dm_conn));
    if (!ab || !ba) {
        perror("malloc");
        if (ab) free(ab);
        if (ba) free(ba);
        return -1;
    }

    ab->peer = userB;
    ab->next = userA->dm_head;
    userA->dm_head = ab;

    ba->peer = userA;
    ba->next = userB->dm_head;
    userB->dm_head = ba;

    return 0;
}

int removeDM(struct node *userA, struct node *userB) {
    if (!userA || !userB) return -1;

    struct dm_conn *cur, *prev;

    // remove B from A list
    cur = userA->dm_head;
    prev = NULL;
    while (cur != NULL) {
        if (cur->peer == userB) {
            if (prev == NULL) {
                userA->dm_head = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    // remove A from B list
    cur = userB->dm_head;
    prev = NULL;
    while (cur != NULL) {
        if (cur->peer == userA) {
            if (prev == NULL) {
                userB->dm_head = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    return 0;
}
