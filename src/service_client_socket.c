#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define buffer_size 1024

/* Reads a file file and places the contents of the file in file_buffer and the length in length.*/
int read_file(char *file, long *length, char **file_buffer) {
	FILE *fp;
	if ((fp = fopen(file, "rb")) == NULL) {
		// 404 File Not Found
		perror("Failed to open file");
	} else {
		// 200 OK
		if (fseek(fp, 0, SEEK_END) == 0) {
			if (((*length = ftell(fp)) > 0)) {
				if (fseek(fp, 0, SEEK_SET) == 0) {
					if (((*file_buffer = malloc(*length + 1)) != NULL)) {
						if (fread(*file_buffer, 1, *length, fp) == *length) {
							if (fclose(fp) == 0) {
								return 0;
							}
						}
					}

				}
			}
		}
	}
	return -1;
}

/* Send an HTTP message, consisting of header header and body body, to port s.*/
int send_response(const int s, char *header, char *body) {
	char *send = malloc(strlen(header) + 20 + strlen(body));
	int len = strlen(body);
	sprintf(send, header, len);
	strcat(send, body);
	if (write(s, send, strlen(send)) != strlen(send)) {
		perror("write");
		free(send);
		return -1;
	}
	free(send);
	return 0;
}

/* Send a file over HTTP with a header header and file file of length length to port s.*/
int send_file(const int s, char *header, char *file, long length) {
	char *send = malloc(strlen(header) + 20);
	sprintf(send, header, length);
	if (write(s, send, strlen(send)) != strlen(send)) {
		perror("write");
		return -1;
	}
	if (write(s, file, length) != length) {
		perror("write");
		return -1;
	}
	free(send);
	free(file);
	return 0;
}

int service_client_socket(const int s, const char * const tag) {
	char buffer[buffer_size];
	size_t bytes;
	// Status codes for messages which are commonly sent
	char status200[] = "HTTP/1.1 200 OK\nContent-Length: %d\n\n";
	char error500[] =
			"HTTP/1.1 500 Internal Server Error\nContent-Length: %d\n\n";
	char error500http[] =
			"<html><body><h1>Error 500 Internal Server Error</h1></body></html>";
	char error501[] = "HTTP/1.1 501 Not Implemented\nContent-Length: %d\n\n";
	char error501http[] =
			"<html><body><h1>Error 501 Not Implemented</h1></body></html>";
	char *request; // e.g. GET
	char *resource; // e.g.  /  or  /test.txt
	char *host; // e.g. localhost:8080
	// File info
	char *file_buffer;
	long length;
	// Directory info
	DIR *dp;
	struct dirent *ep;
	struct stat sb;
	char cwd[buffer_size];
	if (getcwd(cwd, sizeof(cwd)) != NULL)
		fprintf(stdout, "Current working dir: %s\n", cwd);
	else
		perror("getcwd() error");

	char *ptr = NULL;
	if ((ptr = strrchr(cwd, '/')) != NULL) {
		*ptr = '\0';
	} else {
		perror("getcwd");
		return -1;
	}
	fprintf(stdout, "Up one: %s\n", cwd);

	printf("New connection from %s\n", tag);

	// Repeatedly read data from the client
	while ((bytes = read(s, buffer, buffer_size - 1)) > 0) {
		// Create a copy of the buffer for editing
		char *buffercopy = malloc(sizeof(buffer));
		memcpy(buffercopy, buffer, buffer_size);
		// Parse HTML request:
		host = strstr(buffercopy, "Host: "); // Get pointer to start of host line
		host = strtok(host, "\r\n"); // Remove everything after the host line (not needed)
		host += 6; // Remove "Host: " to get just host:port
		request = strtok(buffercopy, " "); // Get everything before the first space (i.e. "GET", "POST" etc)
		if (strcmp(request, "GET") != 0) { // Make sure it is a GET request
			// Error 501
			fprintf(stderr, "Invalid request: %s", request);
			if (send_response(s, error501, error501http) == -1) {
				return -1;
			}
		}
		resource = strtok(NULL, " "); // Get second word in request, e.g. /test.txt
		char *working_dir = malloc(strlen(cwd) + strlen(resource) + 1);
		strcpy(working_dir, cwd);
		strcat(working_dir, resource);

		// If the request is a file transfer
		if (stat(working_dir, &sb) == 0 && !(sb.st_mode & S_IXUSR)) { // Do not show executable files
			// Call read_file to copy the file data into file_buffer
			if (read_file(working_dir, &length, &file_buffer) == -1) {
				// 500 Internal Server Error
				perror("Failed to read file");
				if (send_response(s, error500, error500http) == -1) {
					return -1;
				}
			} else {
				// Send the file
				send_file(s, status200, file_buffer, length);
			}
		} else { // Not a file, so must be a directory
			// Remove trailing "/"'s
			while (resource[strlen(resource) - 1] == '/') {
				resource[strlen(resource) - 1] = '\0';
			}
			char *resource_dir = malloc(strlen(working_dir) + 2);
			strcpy(resource_dir, "..");
			strcat(resource_dir, resource);
			// Open the directory resource_dir, e.g. ./testdir
			if ((dp = opendir(resource_dir)) != NULL) {
				char *send_dir = malloc(buffer_size);
				char *send_files = malloc(buffer_size);
				// Format the beginning of the HTML
				strcpy(send_dir, "<p><b>DIRECTORIES</b></p><p>");
				strcpy(send_files, "<p><b>FILES</b></p><p>");
				char httpline[200];
				char file_name[100];
				char file_path[101];
				// Read items in directory
				while ((ep = readdir(dp)) != NULL) {
					// Format the link correctly
					if (strcmp(resource_dir, "/") != 0) {
						strcpy(file_name, resource_dir);
						strcat(file_name, "/");
					} else {
						strcpy(file_name, "/");
					}
					strcat(file_name, ep->d_name);
					strcpy(file_path, ".");
					strcat(file_path, file_name);
					// Create HTML link to file/directory
					// Make .. directories link to the directory above
					if (strcmp(ep->d_name, "..") == 0
							&& strlen(file_name) > 2) {
						char *ptr = NULL;
						if ((ptr = strrchr(file_name, '/')) != NULL) {
							*ptr = '\0';
							if ((ptr = strrchr(file_name, '/')) != NULL) {
								*ptr = '\0';
							}
						} else {
							perror("strrchr");
							return -1;
						}
					}
					sprintf(httpline, "<a href=\"%s\">%s</a><br>", file_name,
							ep->d_name);
					if (ep->d_type == DT_DIR && strcmp(ep->d_name, ".") != 0) { // Directory
						strcat(send_dir, httpline);
					} else if (ep->d_type == DT_REG && stat(file_name, &sb) == 0
							&& !(sb.st_mode & S_IXUSR)
							&& (ep->d_name[0] != '.')) { // File
						strcat(send_files, httpline);
					}
				}
				// Send data
				strcat(send_dir, "</p>");
				strcat(send_files, "</p>");
				char *send = malloc(strlen(send_dir) + strlen(send_files) + 1);
				strcpy(send, send_dir);
				strcat(send, send_files);
				if (send_response(s, status200, send) != 0) {
					return -1;
				}
				free(resource_dir);
				free(send_dir);
				free(send_files);
				free(send);
				closedir(dp);
			} else {
				// Error 500
				fprintf(stderr, "Error opening directory %s\n", resource);
				if (send_response(s, error500, error500http) != 0) {
					return -1;
				}
			}
		}
		free(working_dir);
		free(buffercopy);
	}

	if (bytes != 0) {
		perror("read");
		return -1;
	}
	printf("Connection from %s closed\n", tag);
	close(s);
	return 0;
}

