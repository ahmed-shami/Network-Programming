#include "server.h"

// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE
extern int numReaders;
extern pthread_mutex_t rw_lock;
extern pthread_mutex_t mutex;

extern struct node *head;
extern struct room *room_head;

extern char const *server_MOTD;

/*
 * Main thread for each client.
 */

static const char *HELP_TEXT =
    "Commands:\n"
    "  login <username>    - login with username\n"
    "  create <room>       - create a room\n"
    "  join <room>         - join a room\n"
    "  leave <room>        - leave a room\n"
    "  users               - list all users\n"
    "  rooms               - list all rooms\n"
    "  connect <user>      - connect to user (DM)\n"
    "  disconnect <user>   - disconnect from user (DM)\n"
    "  exit / logout       - exit chat\n"
    "  help                - show this help\n";

static char *trimwhitespace(char *str) {
    char *end;

    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)
        return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';

    return str;
}

// helper: send "Usage: ..." + prompt
static void send_usage(int client, const char *usage) {
    char buf[MAXBUFF];
    snprintf(buf, sizeof(buf), "Usage: %s\nchat>", usage);
    send(client, buf, strlen(buf), 0);
}

// helper: send error/info message + prompt
static void send_error(int client, const char *msg) {
    char buf[MAXBUFF];
    snprintf(buf, sizeof(buf), "%s\nchat>", msg);
    send(client, buf, strlen(buf), 0);
}

// helper: send just "chat>" prompt
static void send_prompt(int client) {
    const char *p = "chat>";
    send(client, p, strlen(p), 0);
}

// helper to see if a user is already in recipient list
static int in_recipient_list(struct node *list[], int count, struct node *u) {
    int i;
    for (i = 0; i < count; i++) {
        if (list[i] == u) return 1;
    }
    return 0;
}

// helper to build recipient list based on rooms and DMs
static int build_recipients(struct node *sender, struct node *recipients[], int max_recips) {
    int count = 0;

    if (!sender) {
        return 0;
    }

    // all users who share a room with sender
    struct room *r = room_head;
    while (r != NULL) {
        int sender_in_room = 0;
        struct room_user *ru = r->users;

        while (ru != NULL) {
            if (ru->user == sender) {
                sender_in_room = 1;
                break;
            }
            ru = ru->next;
        }

        if (sender_in_room) {
            ru = r->users;
            while (ru != NULL) {
                if (ru->user != sender &&
                    !in_recipient_list(recipients, count, ru->user)) {
                    if (count < max_recips) {
                        recipients[count++] = ru->user;
                    }
                }
                ru = ru->next;
            }
        }

        r = r->next;
    }

    // all DM peers
    struct dm_conn *d = sender->dm_head;
    while (d != NULL) {
        if (d->peer != sender &&
            !in_recipient_list(recipients, count, d->peer)) {
            if (count < max_recips) {
                recipients[count++] = d->peer;
            }
        }
        d = d->next;
    }

    return count;
}

// cleanup user from all structures when disconnecting
static void cleanup_client_user(int client) {
    start_write();
    struct node *me = findUBySocket(head, client);

    if (me) {
        // remove from all rooms
        struct room *r = room_head;
        while (r != NULL) {
            removeUserFromRoom(r, me);
            r = r->next;
        }

        // remove all DMs (this also updates peers)
        while (me->dm_head != NULL) {
            struct node *peer = me->dm_head->peer;
            removeDM(me, peer);
        }

        // remove from global user list
        struct node *cur = head;
        struct node *prev = NULL;
        while (cur != NULL) {
            if (cur == me) {
                if (prev == NULL) {
                    head = cur->next;
                } else {
                    prev->next = cur->next;
                }
                free(cur);
                break;
            }
            prev = cur;
            cur = cur->next;
        }
    }
    end_write();
}

void *client_receive(void *ptr) {
    int client = *(int *) ptr;  // socket
  
    int received, i;
    char buffer[MAXBUFF], sbuffer[MAXBUFF];  // data buffer  
    char tmpbuf[MAXBUFF];                    // temp buffer  
    char cmd[MAXBUFF], username[20];
    char *arguments[80];

    struct node *currentUser;
    
    send(client, server_MOTD, strlen(server_MOTD), 0); // Send MOTD

    // Creating the guest user name
    snprintf(username, sizeof(username), "guest%d", client);

    // add user and put into Lobby
    start_write();
    head = insertFirstU(head, client, username);
    struct node *me_init = findUBySocket(head, client);

    struct room *lobby = createRoom(DEFAULT_ROOM);
    if (lobby && me_init) {
        addUserToRoom(lobby, me_init);
    }
    end_write();
   
    while (1) {
        received = read(client, buffer, MAXBUFF - 1);
        if (received > 0) {
            buffer[received] = '\0'; 
            strcpy(cmd, buffer);  
            strcpy(sbuffer, buffer);

            // cache current user for this iteration
            start_read();
            struct node *me = findUBySocket(head, client);
            end_read();

            if (!me) {
                // user missing from list, clean up and exit
                close(client);
                pthread_exit(NULL);
            }
         
            // tokenize input
            arguments[0] = strtok(cmd, delimiters);
            i = 0;
            while (arguments[i] != NULL) {
                arguments[i] = trimwhitespace(arguments[i]);
                i++;
                arguments[i] = strtok(NULL, delimiters); 
            }

            if (arguments[0] == NULL) {
                send_prompt(client);
                continue;
            }

            // Execute command

            if (strcmp(arguments[0], "create") == 0) {
                if (arguments[1] == NULL) {
                    send_usage(client, "create <room>");
                    continue;
                }

                printf("create room: %s\n", arguments[1]); 
              
                start_write();
                createRoom(arguments[1]);
                end_write();
              
                snprintf(buffer, sizeof(buffer), "Room %s created (or already exists)\nchat>", arguments[1]);
                send(client, buffer, strlen(buffer), 0);
            }
            else if (strcmp(arguments[0], "join") == 0) {
                if (arguments[1] == NULL) {
                    send_usage(client, "join <room>");
                    continue;
                }

                printf("join room: %s\n", arguments[1]);  

                start_write();
                struct room *r = findRoom(arguments[1]);
                if (!r) {
                    r = createRoom(arguments[1]);
                }
                if (r) {
                    addUserToRoom(r, me);
                }
                end_write();
              
                snprintf(buffer, sizeof(buffer), "Joined room %s\nchat>", arguments[1]);
                send(client, buffer, strlen(buffer), 0);
            }
            else if (strcmp(arguments[0], "leave") == 0) {
                if (arguments[1] == NULL) {
                    send_usage(client, "leave <room>");
                    continue;
                }

                printf("leave room: %s\n", arguments[1]); 

                start_write();
                struct room *r = findRoom(arguments[1]);
                if (r) {
                    removeUserFromRoom(r, me);
                    deleteEmptyRooms(DEFAULT_ROOM);
                    snprintf(buffer, sizeof(buffer), "Left room %s\nchat>", arguments[1]);
                } else {
                    snprintf(buffer, sizeof(buffer), "Room %s does not exist\nchat>", arguments[1]);
                }
                end_write();

                send(client, buffer, strlen(buffer), 0);
            } 
            else if (strcmp(arguments[0], "connect") == 0) {
                if (arguments[1] == NULL) {
                    send_usage(client, "connect <user>");
                    continue;
                }

                printf("connect to user: %s \n", arguments[1]);

                start_write();
                struct node *peer = findU(head, arguments[1]);
                if (peer) {
                    if (me == peer) {
                        send_error(client, "Cannot connect to yourself");
                    } else {
                        addDM(me, peer);
                        snprintf(buffer, sizeof(buffer), "Connected to %s\nchat>", arguments[1]);
                        send(client, buffer, strlen(buffer), 0);
                    }
                } else {
                    snprintf(buffer, sizeof(buffer), "User %s not found\nchat>", arguments[1]);
                    send(client, buffer, strlen(buffer), 0);
                }
                end_write();
            }
            else if (strcmp(arguments[0], "disconnect") == 0) {             
                if (arguments[1] == NULL) {
                    send_usage(client, "disconnect <user>");
                    continue;
                }

                printf("disconnect from user: %s\n", arguments[1]);
               
                start_write();
                struct node *peer = findU(head, arguments[1]);
                if (peer) {
                    removeDM(me, peer);
                    snprintf(buffer, sizeof(buffer), "Disconnected from %s\nchat>", arguments[1]);
                    send(client, buffer, strlen(buffer), 0);
                } else {
                    snprintf(buffer, sizeof(buffer), "User %s not found\nchat>", arguments[1]);
                    send(client, buffer, strlen(buffer), 0);
                }
                end_write();
            }                  
            else if (strcmp(arguments[0], "rooms") == 0) {
                printf("List all the rooms\n");
              
                start_read();
                listRooms(room_head, buffer, MAXBUFF);
                end_read();
       
                strncat(buffer, "chat>", MAXBUFF - strlen(buffer) - 1);
                send(client, buffer, strlen(buffer), 0);
            }   
            else if (strcmp(arguments[0], "users") == 0) {
                printf("List all the users\n");
              
                start_read();
                listUsers(head, buffer, MAXBUFF);
                end_read();
                
                strncat(buffer, "chat>", MAXBUFF - strlen(buffer) - 1);
                send(client, buffer, strlen(buffer), 0);
            }                           
            else if (strcmp(arguments[0], "login") == 0) {
                if (arguments[1] == NULL) {
                    send_usage(client, "login <username>");
                    continue;
                }

                start_write();
                strncpy(me->username, arguments[1], sizeof(me->username) - 1);
                me->username[sizeof(me->username) - 1] = '\0';
                end_write();
                
                snprintf(buffer, sizeof(buffer), "Logged in as %s\nchat>", arguments[1]);
                send(client, buffer, strlen(buffer), 0);
            } 
            else if (strcmp(arguments[0], "help") == 0) {
                send(client, HELP_TEXT, strlen(HELP_TEXT));
                send_prompt(client);
            }
            else if (strcmp(arguments[0], "exit") == 0 || strcmp(arguments[0], "logout") == 0) {
                cleanup_client_user(client);
                close(client);
                pthread_exit(NULL);
            }                         
            else { 
                // sending a message according to rooms and DMs

                start_read();
                currentUser = me;
                if (!currentUser) {
                    end_read();
                    continue;
                }

                const char *from = currentUser->username;

                // sbuffer still has the original message text
                snprintf(tmpbuf, sizeof(tmpbuf), "\n::%s> %s\nchat>", from, sbuffer);
                size_t msglen = strlen(tmpbuf);

                struct node *recipients[max_clients];
                int rc = build_recipients(currentUser, recipients, max_clients);

                if (rc == 0) {
                    end_read();
                    send_error(client, "No recipients. Join a room or connect to a user first.");
                } else {
                    int k;
                    for (k = 0; k < rc; k++) {
                        if (recipients[k]->socket != client) {
                            send(recipients[k]->socket, tmpbuf, msglen, 0);
                        }
                    }
                    end_read();
                }
            }
 
            memset(buffer, 0, sizeof(buffer));
        } else {
            // client disconnected or error
            cleanup_client_user(client);
            close(client);
            pthread_exit(NULL);
        }
    }

    // fallback
    cleanup_client_user(client);
    close(client);
    pthread_exit(NULL);
}
