#define __USE_POSIX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <sys/queue.h>

#include "connection_thread.h"

#define USE_AESD_CHAR_DEVICE    1
#define RPI3        1


#if RPI3
/* Dont use syslog on Raspbian */
#define syslog(priority, ...)    printf(__VA_ARGS__)
#endif




#define SERVER_PORT "9000"

#if (USE_AESD_CHAR_DEVICE == 1)
#define FILENAME "/dev/aesdchar"
#else   
#define FILENAME "/var/tmp/aesdsocketdata"
#endif
/* Structs, typedefs */

struct entry
{
    pthread_t thread;
    LIST_ENTRY(entry)
    entries; /* List */
};

LIST_HEAD(listhead, entry);
struct listhead head;

/* Global variables */
static FILE *output_file;
static struct sigaction sig_action;
static pthread_mutex_t filemutex = PTHREAD_MUTEX_INITIALIZER;
static int sockfd, newsockfd;



/* Function prototypes */

static void initSignalHandler(void);
static int startServer(bool runasdaemon);
static void *get_in_addr(struct sockaddr *sa);
static void *timestampThread(void *arg);
static void removeThreadFromList(pthread_t id);
/**
 *  Main function
 */
int main(int argc, char **argv)
{
    bool daemon = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strstr(argv[i], "-d"))
        {
            daemon = true;
        }
    }

    /* Open syslog */
    openlog("aesdsockets", LOG_CONS | LOG_PID, LOG_USER);

    /* Initialize robust mutex */
    pthread_mutexattr_t mutex_att;
    pthread_mutexattr_init(&mutex_att);
    pthread_mutexattr_settype(&mutex_att, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutexattr_setrobust(&mutex_att, PTHREAD_MUTEX_ROBUST);

    pthread_mutex_init(&filemutex, &mutex_att);

    pthread_mutexattr_destroy(&mutex_att);
    

    /* Install signal handler */
    initSignalHandler();

    /* Open socket */
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1)
    {
        syslog(LOG_ERR, "Could not create socket\n");
        return -1;
    }

    /* Launch server*/
    startServer(daemon);

    return 0;
}

int startServer(bool runasdaemon)
{
    struct addrinfo hints, *servinfo, *pAddrInfo;
    int yes = 1;
    char str[INET_ADDRSTRLEN];
    int result;
    socklen_t sin_size;
    struct sockaddr_storage their_addr;
    int n = 0;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((result = getaddrinfo(NULL, SERVER_PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        exit(-11);
    }

    // loop through all the results and bind to the first we can
    for (pAddrInfo = servinfo; pAddrInfo != NULL; pAddrInfo = pAddrInfo->ai_next)
    {
        if ((sockfd = socket(pAddrInfo->ai_family, pAddrInfo->ai_socktype,
                             pAddrInfo->ai_protocol)) == -1)
        {
            perror("server: socket\n");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("setsockopt\n");
            exit(-1);
        }

        if (bind(sockfd, pAddrInfo->ai_addr, pAddrInfo->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind\n");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (pAddrInfo == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(-1);
    }

    if (runasdaemon)
    {
        if (daemon(0, 1) == -1)
        {
            printf("Couldnt daemonize\n");
            exit(-1);
        }
    }

    if (listen(sockfd, 1) == -1)
    {
        perror("listen\n");
        exit(-1);
    }

    /* Initialize thread list */
    LIST_INIT(&head);

    /* Create timestamp thread */
    struct entry *element = malloc(sizeof(struct entry));
    LIST_INSERT_HEAD(&head, element, entries);
    pthread_create(&element->thread, NULL, timestampThread, NULL);
    while (1)
    {
        sin_size = sizeof their_addr;
        newsockfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newsockfd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  str, sizeof str);
        syslog(LOG_USER, "Accepted connection from %s\n", str);

        /* Create new thread and add it to the list of threads */
        element = malloc(sizeof(struct entry));
        LIST_INSERT_HEAD(&head, element, entries);

        /* Allocate and fill param structure, thread will be responsible of freeing
        this memory location */
        ThreadParams_t *params = malloc(sizeof(ThreadParams_t));
        params->thread_id = n++;
        params->socket_fd = newsockfd;
        if(output_file == NULL)
        {
            /* Open output file */
            output_file = fopen(FILENAME, "a+");
            if (output_file == NULL)
            {
                syslog(LOG_ERR, "Could not open/create file, exiting\n");
                return -1;
            }
        }
        params->file = output_file;
        if(output_file== NULL)
        {
            /* Open output file */
            output_file = fopen(FILENAME, "a+");
            if (output_file == NULL)
            {
                syslog(LOG_ERR, "Could not open/create file, exiting\n");
                return -1;
            }
        }
        params->filemutex = &filemutex;
        params->remove_me_from_list = removeThreadFromList;
        strncpy(params->address, str, 4 * 3 + 3 + 1);

        pthread_create(&element->thread,
                       NULL,
                       connection_thread,
                       params);
    }
}

/**
 *  Signal handler
 */
void signalHandler(int signo)
{
    syslog(LOG_USER, "Caught signal, exiting\n");

    /* Join threads */
    struct entry *n1, *n2, *np;
    LIST_FOREACH(np, &head, entries)
    {
        pthread_cancel(np->thread);
        pthread_join(np->thread, NULL);
    }

    /* Free list */
    n1 = LIST_FIRST(&head);
    while (n1 != NULL) {
        n2 = LIST_NEXT(n1, entries);
        free(n1);
        n1 = n2;
    }
    LIST_INIT(&head);

    

    closelog();

    close(sockfd);
    fflush(output_file);
    fclose(output_file);
#if (USE_AESD_CHAR_DEVICE == 0)
    remove(FILENAME);
#endif
    exit(0);
}

/**
 *  Initializes signal handling
 */
void initSignalHandler(void)
{
    sig_action.sa_handler = signalHandler;

    /* Block all signals */
    memset(&sig_action.sa_mask, 1, sizeof(sig_action.sa_mask));

    sigaction(SIGINT, &sig_action, NULL);
    sigaction(SIGTERM, &sig_action, NULL);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

/**
 * Timestamp thread
 */
void *timestampThread(void *arg)
{
    char outstr[200];
    time_t t;
    struct tm *tmp;
    size_t size;

#if USE_AESD_CHAR_DEVICE
    pthread_exit(NULL);
#endif

    while (1)
    {
        sleep(10);

        t = time(NULL);
        tmp = localtime(&t);

        size = strftime(outstr, sizeof(outstr), "timestamp:%a, %d %b %Y %T %z\n", tmp);

        pthread_mutex_lock(&filemutex);
        //fwrite(outstr, sizeof(char), size, output_file);
        write(fileno(output_file) ,outstr, size);
        fflush(output_file);
        pthread_mutex_unlock(&filemutex);
    }

    return NULL;
}

void removeThreadFromList(pthread_t id)
{
    struct entry* np = NULL;
    LIST_FOREACH(np, &head, entries)
    {
        if(pthread_equal(id, np->thread))
        {
            break;
        }
    }

    if(np != NULL)
    {
        LIST_REMOVE(np, entries);
        free(np);
    }

}