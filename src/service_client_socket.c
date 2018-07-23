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

#define buffer_size 2048
// Status codes for messages which are commonly sent
char status200[] = "HTTP/1.1 200 OK\nContent-Length: %d\n\n";
char error500[] = "HTTP/1.1 500 Internal Server Error\nContent-Length: %d\n\n";
char error500http[] = "<html><body><h1>Error 500 Internal Server Error</h1></body></html>";
char error501[] = "HTTP/1.1 501 Not Implemented\nContent-Length: %d\n\n";
char error501http[] = "<html><body><h1>Error 501 Not Implemented</h1></body></html>";

/* Reads a file file and places the contents of the file in file_buffer and the length in length.*/
int read_file(char *file, long *length, char **file_buffer) {
	FILE *fp;
	if ((fp = fopen(file, "rb")) == NULL) {
		// 404 File Not Found
		perror("Failed to open file");
		return -1;
	} else {
		// 200 OK
		if (fseek(fp, 0, SEEK_END) != 0) { // Seek last byte
			perror("fseek");
			return -1;
		} else if (((*length = ftell(fp)) <= 0)) { // Find length from beginning to end of file
			perror("ftell");
			return -1;
		} else if (fseek(fp, 0, SEEK_SET) != 0) { // Go back to the beginning of the file
			perror("fseek");
			return -1;
		} else if (((*file_buffer = malloc(*length + 1)) == NULL)) { // Allocate memory for string to store the file
			perror("malloc");
			return -1;
		} else if (fread(*file_buffer, 1, *length, fp) != *length) { // Read whole file into string
			perror("fread");
			return -1;
		} else if (fclose(fp) != 0) { // Close the file
			perror("fclose");
			return -1;
		}
	}
	return 0;
}

/* Send an HTTP message, consisting of header header and body body, to port s.*/
int send_response(const int s, char *header, char *body) {
	// Allocate memory
	char *send = malloc(strlen(header) + 20 + strlen(body));
	int len = strlen(body);
	// Add the header and body to the string to be sent
	sprintf(send, header, len);
	strcat(send, body);
	// Send the whole message
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
	// Allocate memory
	char *send = malloc(strlen(header) + 20);
	// Add header to string to be sent
	sprintf(send, header, length);
	// Send the header
	if (write(s, send, strlen(send)) != strlen(send)) {
		perror("write");
		return -1;
	}
	// Send the file
	if (write(s, file, length) != length) {
		perror("write");
		return -1;
	}
	free(send);
	free(file);
	return 0;
}

int list_directory(DIR *directory, char *resource, char *resource_dir, struct stat file_stats, const int s) {
	// Directory info
	struct dirent *entry;

	char *send_dir = malloc(2 * buffer_size);
	char *send_files = malloc(2 * buffer_size);
	char *upload_files = malloc(buffer_size);
	// Format the beginning of the HTML
	strcpy(send_dir, "<style>\
			table {\
			    font-family: arial, sans-serif;\
			    border-collapse: collapse;\
			    width: 50%;\
		    	    margin: 25px;\
			}\
			td, th {\
			    border: 1px solid #dddddd;\
			    text-align: left;\
			    padding: 8px;\
			}\
			tr:nth-child(even) {\
			    background-color: #dddddd;\
			}\
			</style>\
			<table>\
			<tr>\
				<th>Directory</th>\
			</tr>");
	strcpy(send_files, "<table>\
			<tr>\
				<th>File</th>\
				<th>Size</th>\
			</tr>");
	char httpline[200];
	char file_name[200];
	char file_path[202];
	// Read items in directory
	while ((entry = readdir(directory)) != NULL) {
		// Format the link correctly
		if (strcmp(resource, "/") != 0) {
			strcpy(file_name, resource);
			strcat(file_name, "/");
		} else {
			strcpy(file_name, "/");
		}
		strcat(file_name, entry->d_name);
		strcpy(file_path, "..");
		strcat(file_path, file_name);
		// Make .. directories link to the directory above, e.g.:
		// '../src/..' --> '..'
		/*
		if (strcmp(entry->d_name, "..") == 0
				&& strlen(file_name) > 2) { // Don't link to outside the top level
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
		}*/
		// Fetch stats on the file/directory
		if (stat(file_path, &file_stats) != 0) {
			perror("stat");
			return -1;
		}
		// Create HTML link to file/directory
		printf("name: %s, type: %s, size: %ld, links: %lu\n", file_name, entry->d_type == DT_DIR ? "dir" : "file", file_stats.st_size, file_stats.st_nlink);
		if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0) { // Directory and not "."
			sprintf(httpline, "<tr><td><a href=\"%s\">%s</a><br></td></tr>", file_name, entry->d_name);
			strcat(send_dir, httpline);
		} else if (entry->d_type == DT_REG // File
				&& !(file_stats.st_mode & S_IXUSR) // Not executable
				&& (entry->d_name[0] != '.')) { // Doesn't start with "."
			sprintf(httpline, "<tr><td><a href=\"%s\">%s</a></td><td>%ld bytes</td></tr>", file_name, entry->d_name, file_stats.st_size);
			strcat(send_files, httpline);
		}
	}
	// Send data
	strcat(send_dir, "</table>");
	strcat(send_files, "</table><br><br>");
	strcpy(upload_files, "<form action=\"src/upload.php\" method=\"post\" nctype=\"multipart/form-data\">\
  			Select files: \
  			<input type=\"file\" name=\"file_to_upload\" id=\"file_to_upload\" multiple><br><br>\
  			<input type=\"submit\" value=\"Upload\" name=\"submit\">\
			</form>");
	char *send = malloc(strlen(send_dir) + strlen(send_files) + strlen(upload_files) + 1);
	strcpy(send, send_dir);
	strcat(send, send_files);
	strcat(send, upload_files);
	if (send_response(s, status200, send) != 0) {
		perror("send_response");
		return -1;
	}
	free(resource_dir);
	free(send_dir);
	free(send_files);
	free(send);
	closedir(directory);
	return 0;
}

int service_client_socket(const int s, const char * const tag) {
	char buffer[buffer_size];
	size_t bytes;
	char *request; // e.g. GET
	char *resource; // e.g.  /  or  /test.txt
	char *host; // e.g. localhost:8080
	// File info
	char *file_buffer;
	long length;
	// Directory info
	DIR *directory;
	// File info - Find if file is executable
	struct stat file_stats;
	// Current working directory
	char cwd[buffer_size];
	if (getcwd(cwd, sizeof(cwd)) != NULL)
		fprintf(stdout, "Current working dir: %s\n", cwd);
	else
		perror("getcwd()");

	// Find directory up one level by removing everything after the last '/'
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
		printf("%s\n", buffer);
		// Create a copy of the buffer for editing
		char *buffercopy = malloc(sizeof(buffer));
		memcpy(buffercopy, buffer, buffer_size);
		// Parse HTML request:
		host = strstr(buffercopy, "Host: "); // Get pointer to start of host line
		host = strtok(host, "\r\n"); // Remove everything after the host line (not needed)
		host += 6; // Remove "Host: " to get just host:port, e.g. localhost:8080
		
		request = strtok(buffercopy, " "); // Get everything before the first space (i.e. "GET", "POST" etc)
		if (strcmp(request, "GET") != 0 && strcmp(request, "POST") != 0) { // Make sure it is a GET request
			// Error 501
			fprintf(stderr, "Invalid request: %s", request);
			if (send_response(s, error501, error501http) == -1) {
				return -1;
			}
		}
		resource = strtok(NULL, " "); // Get second word in request, e.g. / or /test.txt
		// Using cwd and the requested resource, find the path to the resource
		char *working_dir = malloc(strlen(cwd) + strlen(resource) + 1);
		strcpy(working_dir, cwd);
		strcat(working_dir, resource);

		// If the request is a file transfer
		printf("%s\n", resource);
		
		if (stat(working_dir, &file_stats) == 0 && !(file_stats.st_mode & S_IXUSR)) { // Do not show executable files
			// Call read_file to copy the file data into file_buffer
			if (read_file(working_dir, &length, &file_buffer) == -1) {
				// 500 Internal Server Error
				perror("Failed to read file");
				if (send_response(s, error500, error500http) == -1) {
					perror("Failed to send fail response");
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
			// Get correct path to request directory
			// (Since code is running in /src we need to go up a directory)
			char *resource_dir = malloc(strlen(working_dir) + 2);
			strcpy(resource_dir, "..");
			strcat(resource_dir, resource);
			//printf("working_dir: %s , resource: %s , resource_dir: %s\n", working_dir, resource, resource_dir);
			// Open the directory resource_dir, e.g. ./testdir
			if ((directory = opendir(resource_dir)) != NULL) {
				if (list_directory(directory, resource, resource_dir, file_stats, s) != 0) {
					perror("list_directory");
				}
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

