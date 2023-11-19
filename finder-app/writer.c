#define __STDC_WANT_LIB_EXT1__ 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

// ssize_t write (int fd, const void *buf, size_t count);
// Using non-buffered IO

int main(int argc, char *argv[])
{
    openlog("Finder-app", LOG_CONS, LOG_USER);
    if (argc < 3)
    {
        syslog(LOG_ERR, "Not enough arguments");
        return 1;
    }

    char *path = argv[1];
    char *str = argv[2];

    int fd = open(path, O_CREAT | O_WRONLY, S_IRWXO|S_IRWXU|S_IRWXG);
    if (fd == -1)
    {
        syslog(LOG_ERR, "File could not be created/opened");
        return 1;
    }

    ssize_t len = strlen(str);
    ssize_t ret;

    while (len != 0 && (ret = write(fd, str, len)) != 0)
    {
        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
            syslog(LOG_ERR, "Could not wrrite to file");
            close(fd);
            return 1;
            break;
        }

        len -= ret;
        str += ret;
    }

    if(fd != -1)
    {
        fsync(fd);
        close(fd);
    }
    return 0;
}