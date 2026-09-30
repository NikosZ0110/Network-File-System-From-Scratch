#ifndef __SERVER_H_
#define __SERVER_H_

#define PORT                8080
#define MAX_BUFFER_SIZE     40000
#define MAX_FD_MULTITUDE    100
#define THIRTY_MINUTES_IN_MICROSECONDS (1 * 60 * 1000000)
#define THIRTY_MINUTES_IN_SECONDS (1 * 60)

#define INCARNATION_FILE    "incarnation_num"

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ifaddrs.h>

//Node for file info list
typedef struct Node Node;
struct Node {
    char* fileName;
    int tmod;
    int fd;
    int fileSize;
    
    struct timeval lastUsed;    //Last time the file was used - (for fd closing)
    
    sem_t mutex;        //Counters safety semaphore
    sem_t readers_q;    //Readers semaphore
    sem_t writers_q;    //Writers semaphore

    int w_waiting;      //Writers waiting queue
    int r_waiting;      //Readers waiting queue
    int w_critical;     //Writers in critical spot
    int r_critical;     //Readers in critical spot

    Node *next;
};

//Node for fd list
typedef struct fdNode fdNode;
struct fdNode {
    int fd;
    int version;
    fdNode *next;
};

typedef struct {
    char *contents;
    struct sockaddr_in client_addr;
} ThreadArgs;


int renew_incarnation_number    (const char *path); 

Node* createNode                (char *fname, int fd);
Node* enqueueNode               (char *fname, int fd);

Node* findNodeByName            (char *name);
Node* findNodeByFd              (int fd);

fdNode *createFdNode            (int fd);
void enqueueFdNode              (int fd);

int findVersion                 (int fd);
int renewVersion                (int fd);

void *handleClient              (void *arg);
void *handleFdClosure           ();

#endif
