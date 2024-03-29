#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "connection_thread.h"
#include "aesd_ioctl.h"

#define RPI3        1


#if RPI3
/* Dont use syslog on Raspbian */
#define syslog(priority, ...)    printf(__VA_ARGS__)
#endif

static void sendFile(int socket, FILE *file);

struct uint_pair
{
    uint32_t first;
    uint32_t second;
};

static void cleanup_handler(void *arg)
{
    ThreadParams_t* params = (ThreadParams_t *)arg;
    params->remove_me_from_list(pthread_self());
    close(params->socket_fd);
    free(arg);
    
}

void *connection_thread(void *arg)
{
    /* Cast the argument */
    ThreadParams_t* params = (ThreadParams_t *)arg;
    size_t received;
    char data[1501];
    char *internal_buffer = NULL;

    pthread_cleanup_push(cleanup_handler, &params);
    size_t currentSize = 0;
    do
    {
        received = recv(params->socket_fd, data, 1500,0);
        if (internal_buffer == NULL) /* New line */
        {
            internal_buffer = malloc(received * sizeof(char));
            params->buff = internal_buffer;
            if (internal_buffer == NULL)
            {
                syslog(LOG_CRIT, "Cannot allocate memory for new write\n");
                pthread_exit(NULL);
            }
            currentSize = received;
            memcpy(internal_buffer, data, received);
        }
        else /* Continue with line */
        {
            char *new_buffer = realloc(internal_buffer, currentSize + received);
            if (new_buffer == NULL)
            {
                syslog(LOG_CRIT, "Cannot allocate memory for new write\n");
                pthread_exit(NULL);
            }
            else
            {
                memcpy(new_buffer + currentSize, data, received);
                internal_buffer = new_buffer;
                params->buff = internal_buffer;
                currentSize += received;
            }
        }

        
        /* Look for end of line character */
        size_t i = 0ULL;
        struct aesd_seekto pair;
        for (i = 0ULL; i < currentSize; ++i)
        {
            if (internal_buffer[i] == '\n')
            {
                /* TODO: Handle commands */
                if(strstr(internal_buffer, "AESDCHAR_IOCSEEKTO")== internal_buffer)
                {
                    
                    pthread_mutex_lock(params->filemutex);
                    sscanf(internal_buffer,"AESDCHAR_IOCSEEKTO:%u,%u", &pair.write_cmd, &pair.write_cmd_offset);
                    printf("Found command %u, %u\n",pair.write_cmd, pair.write_cmd_offset);
                    ioctl(fileno(params->file), AESDCHAR_IOCSEEKTO, &pair);
                    sendFile(params->socket_fd, params->file);
                    pthread_mutex_unlock(params->filemutex);
                    currentSize = 0;
                    break;

                }
                pthread_mutex_lock(params->filemutex);
                //fwrite(internal_buffer, sizeof(char), i + 1, params->file);
                write(fileno(params->file), internal_buffer, i + 1);
                rewind(params->file);
                fflush(params->file);
                sendFile(params->socket_fd, params->file);
                pthread_mutex_unlock(params->filemutex);

                if ((i + 1) < currentSize)
                    memmove(internal_buffer, internal_buffer + i + 1, currentSize - i);
                currentSize -= (i + 1);
            }
        }

        if (currentSize == 0)
        {
            free(internal_buffer);
            internal_buffer = NULL;
            params->buff = internal_buffer;
        }
        else
        {
            /* Shrink the buffer */
            internal_buffer = realloc(internal_buffer, currentSize);
            params->buff = internal_buffer;
        }

    } while (received > 0);
    syslog(LOG_USER, "Closed connection from %s\n", params->address);

    pthread_cleanup_pop(0);

    params->remove_me_from_list(pthread_self());
    
    close(params->socket_fd);
    params = NULL;
    free(arg);


    free(internal_buffer);
    internal_buffer = NULL;
    
    
    pthread_exit(NULL);
}

void sendFile(int socket, FILE *file)
{
    char buff[1024];
    size_t count;

    syslog(LOG_USER, "Sending whatever is in the file\n");
    
    do
    {
        //count = fread(buff, sizeof(char), sizeof(buff) / sizeof(char), file);
        count = read(fileno(file), buff, sizeof(buff));
        buff[count+1]=0;
        syslog(LOG_USER, "Sending %s\n", buff);
        if (count)
        {
            send(socket, buff, count, 0);
        }
    } while (count);
}