//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>



int main(int argc, char* argv[])
{
    openlog(NULL, LOG_CONS, LOG_USER);
    if(argc <3)
    {
        syslog(LOG_ERR, "Illegal number of arguments");
        return 1;
    }
    
    char* file = argv[1];
    char* str = argv[2];
    
    FILE *stream = fopen(file, "w");

    if(stream == NULL)
    {
        syslog(LOG_ERR, "Could not open/create file");
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", str, file);

    if(fputs(str, stream) < 0)
    {
        syslog(LOG_ERR, "Could not write to file");
        return 1;
    }

    fclose(stream);
    return 0;
}