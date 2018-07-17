#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <memory.h>

#include <sys/socket.h>

#include <pthread.h>

#include "make_printable_addr.h"
#include "service_client_socket.h"

typedef struct thread_control_block {
	int client;
	struct sockaddr_in6 their_address;
	socklen_t their_address_size;
} thread_control_block_t;

static void *client_thread(void *data) {
	thread_control_block_t *tcb_p = (thread_control_block_t *) data;
	char buffer[INET6_ADDRSTRLEN + 32];
	char *printable;
	// Create a printable version of ipv6/ipv4 address and port
	printable = make_printable_addr(&(tcb_p->their_address),
			tcb_p->their_address_size, buffer, sizeof(buffer));
	// Handle the connection
	service_client_socket(tcb_p->client, printable);

	free(printable); // strdup'd
	free(data); //malloc'd
	pthread_exit(0);
}

int main(int argc, char **argv) {
	// Checking arguments
	char *myname = argv[0];
	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", myname);
		exit(1);
	}
	char *endp; // Check that port is a number
	int port = strtol(argv[1], &endp, 10);
	if (*endp != '\0') {
		fprintf(stderr, "%s is not a number\n", argv[1]);
		exit(1);
	}
	if (port < 1024 || port > 65535) {
		perror("Port must be between 1024 and 65535\n");
		exit(1);
	}

	// Getting socket
	struct sockaddr_in6 my_addr;
	memset(&my_addr, '\0', sizeof(my_addr));
	my_addr.sin6_family = AF_INET6; // ipv6 address
	my_addr.sin6_addr = in6addr_any; // bind to all interfaces
	my_addr.sin6_port = htons(port); //network order

	int s = socket(PF_INET6, SOCK_STREAM, 0);
	if (s < 0) {
		perror("error getting socket\n");
		exit(1);
	}
	// Allows us to kill and restart the server on the same port number
	const int one = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
		perror("setsocket");
		// not the end of the world, so continue
	}
	// Bind
	if (bind(s, (struct sockaddr *) &my_addr, sizeof(my_addr)) != 0) {
		perror("bind");
		exit(1);
	}
	// Listen
	if (listen(s, 5) != 0) {
		perror("listen");
		exit(1);
	}

	while (1) {
		// Create thread info
		thread_control_block_t *tcb_p = malloc(sizeof(*tcb_p));
		if (tcb_p == 0) {
			perror("malloc");
			exit(1);
		}
		tcb_p->their_address_size = sizeof(tcb_p->their_address);
		// Wait for connections and start a thread when one arrives
		if ((tcb_p->client = accept(s,
				(struct sockaddr*) &(tcb_p->their_address),
				&(tcb_p->their_address_size))) < 0) {
			perror("accept");
			// Carry on/try again
		} else {
			pthread_t thread;
			if (pthread_create(&thread, 0, &client_thread, (void *) tcb_p)
					!= 0) {
				perror("pthread_create");
				goto error_exit;
			}
		}

	}
	error_exit: return 0;
}

