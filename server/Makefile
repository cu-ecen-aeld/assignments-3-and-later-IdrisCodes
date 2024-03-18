
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror
LDFLAGS ?= -lpthread

all: aesdsocket
default: aesdsocket

aesdsocket: aesdsocket.c connection_thread.c
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS) -g3 -ggdb

clean:
	rm -f aesdsocket aesdsocket.o
