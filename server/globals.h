#ifndef SHARED_GLOBALS_H
#define SHARED_GLOBALS_H

#include <semaphore.h>

extern char *path;          //Given by user as the root directory 
extern int inc_num;

extern int sockfd;

extern sem_t sem_list;      //Semaphore to add new nodes to list/ read info from the list/ change list's info

extern sem_t sem_fdlist;    //Semapore to add new nodes/change node contents/read node contents

#endif 
