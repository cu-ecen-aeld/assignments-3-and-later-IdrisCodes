#ifndef CONNECTION_THREAD_H_
#define CONNECTION_THREAD_H_


#include <pthread.h>
#include <stdio.h>

typedef struct{
    int thread_id;
    int socket_fd;  /* Socket descriptor */
    pthread_mutex_t* filemutex; /* File mutex */
    FILE* file;
    char address[4*3+3+1];
    char* buff;
    size_t sz;
    void (*remove_me_from_list)(pthread_t thread_id);

} ThreadParams_t;

void *connection_thread(void *arg);


#endif /* CONNECTION_THREAD_H_*/
