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

#define SERVER_PORT "9000"
#define FILENAME "/var/tmp/aesdsocketdata"


/* Global variables */
static FILE* output_file;
static char* internal_buffer;
struct sigaction sig_action;

int sockfd, newsockfd;

/* Function prototypes */
static void my_handler (int signo, siginfo_t *si, void *ucontext);
static int appendData(char* data, size_t size, FILE* file);
static void initSignalHandler(void);
static int socketHandler(int socket, FILE* file);
static int startServer(bool runasdaemon);
static void *get_in_addr(struct sockaddr *sa);
static void sendFile(int socket, FILE* file);
/**
 *  Main function
*/
int main(int argc, char** argv)
{
    int retval = 0;
    bool daemon = false;
    for(int i=1; i < argc; ++i)
    {
        if(strstr(argv[i],"-d"))
        {
            daemon = true;
        }
    }
    openlog("aesdsockets", LOG_CONS | LOG_PID, LOG_USER);
    
    /* Open output file */
    output_file = fopen(FILENAME, "a+");
    if(output_file == NULL)
    {
        syslog(LOG_ERR, "Could not open/create file, exiting");
        return -1;
    }

    /* Install signal handler */
    initSignalHandler();


    /* Open socket */
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd == -1)
    {
        syslog(LOG_ERR, "Could not create socket");
        return -1;
    }

    startServer(daemon);

    return 0;
}

int startServer(bool runasdaemon)
{
    struct addrinfo hints, *servinfo, *p;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    socklen_t sin_size;
    struct sockaddr_storage their_addr;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, SERVER_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit (-11);
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(-1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(-1);
    }


    if(runasdaemon)
    {
        if(daemon(0, 1) == -1)
        {
            printf("Couldnt daemonize");
            exit(-1);
        }
    }



    if (listen(sockfd, 1) == -1) {
        perror("listen");
        exit(-1);
    }

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        newsockfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newsockfd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        
        syslog(LOG_USER, "Accepted connection from %s", s);

        socketHandler(newsockfd, output_file);
        
        syslog(LOG_USER, "Closed connection from %s", s);
    }

}

int socketHandler(int socket, FILE* file)
{
//    struct sockaddr client_address;
//    socklen_t len; 
//    char ipaddr[16];
//    /* Print peer address to log */
//    if(getpeername(socket, &client_address, &len))
//    {
//        syslog(LOG_ERR, "Could not obtain peer address");
//        exit(-1);
//    }
//
//    inet_ntop(AF_INET, &client_address, ipaddr , sizeof ipaddr);
//    syslog(LOG_DEBUG, "Accepted connection from %s",ipaddr );

    ssize_t received;
    uint8_t buff[1501];
    do
    {
        received = read(newsockfd, buff, 1500);
        appendData(buff, received, output_file);
        //send(newsockfd, buff, received,0);
    }while (received>0);

    return 0;
}


/**
 *  Signal handler
*/
void signalHandler (int signo)
{
    syslog(LOG_USER, "Caught signal, exiting");

    closelog();

    close(sockfd);
    close(newsockfd);

    fclose(output_file);
    remove(FILENAME);

    free(internal_buffer);

    exit(0);
}

/**
 *  Appends data to buffer
 *  Writes buffer when a line is complete
*/
int appendData(char* data, size_t size, FILE* file)
{
    static size_t currentSize = 0;
    
    if(internal_buffer == NULL) /* New line */
    {
        internal_buffer = malloc(size * sizeof(char));
        if(internal_buffer == NULL)
        {
            syslog(LOG_CRIT, "Cannot allocate memory for new write");
            return -1;
        }
        currentSize = size;
        memcpy(internal_buffer, data, size);
    }
    else    /* Continue with line */
    {
        char* new_buffer = realloc(internal_buffer, currentSize + size);
        if(new_buffer == NULL)
        {
            syslog(LOG_CRIT, "Cannot allocate memory for new write, flushing buffer");
            
            if(fwrite(internal_buffer, sizeof(char), currentSize, file) <= 0)
            {
                syslog(LOG_CRIT, "Cannot write to file");
                return -1;
            }
            fflush(file);
            free(internal_buffer);
            currentSize = 0;
            return appendData(data, size, file);
        }
        else
        {
            memcpy(new_buffer + currentSize, data, size);
            internal_buffer = new_buffer;
            currentSize += size;
            
        }
    }

    /* Look for end of line character */
    size_t i = 0ULL;
    
    for(i=0ULL; i < currentSize; ++i)
    {
        if(internal_buffer[i] == '\n')
        {
            fwrite(internal_buffer, sizeof(char), i+1, file);
            fflush(file);
            printf("Sending back file");
            sendFile(newsockfd, file);
            if((i+1) < currentSize)
                memmove(internal_buffer, internal_buffer + i + 1, currentSize - i);
            currentSize -= (i+1);
        }
    }

    if(currentSize == 0)
    {
        free(internal_buffer);
        internal_buffer = NULL;
    }
    else
    {
        /* Shrink the buffer */
        internal_buffer = realloc(internal_buffer, currentSize);
    }

    return 0;
}


void sendFile(int socket, FILE* file)
{
    char buff[1024];
    size_t count;
    rewind(file);
    do
    {
        count = fread(buff, sizeof(char), sizeof(buff)/sizeof(char), file);
        
        if(count)
        {
            send(socket, buff, count, 0);
            printf("sending %lu bytes\n", count);
        }
    } while (count);
}
/**
 *  Initializes signal handling
*/
void initSignalHandler(void)
{
    sig_action.sa_handler = signalHandler;

    /* Block all signals */
    memset (&sig_action.sa_mask, 1, sizeof(sig_action.sa_mask));

    sigaction(SIGINT, &sig_action, NULL);
    sigaction(SIGTERM, &sig_action, NULL);

}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}