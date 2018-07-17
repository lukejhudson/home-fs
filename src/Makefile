CFLAGS=-Wall -Werror -std=gnu99
LIBS=-lpthread

CLANG=$(shell which clang)
ifeq ($(CLANG), /usr/bin/clang)
	CC=clang
else
	CC=cc
endif


OBJS = server_multi.o service_client_socket.o make_printable_addr.o

all: server

server: $(OBJS) 
	$(CC) -o $@ $(CFLAGS) $+ $(LIBS)

clean:
	rm -f $(OBJS) server *~
