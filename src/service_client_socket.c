#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>

#include <time.h>

#define BUFFER_SIZE 2048
// Status codes for messages which are commonly sent
char status200[] = "HTTP/1.1 200 OK\nContent-Length: %d\n\n";
char error500[] = "HTTP/1.1 500 Internal Server Error\nContent-Length: %d\n\n";
char error500http[] = "<html><body><h1>Error 500 Internal Server Error</h1></body></html>";
char error501[] = "HTTP/1.1 501 Not Implemented\nContent-Length: %d\n\n";
char error501http[] = "<html><body><h1>Error 501 Not Implemented</h1></body></html>";
FILE *log_fp; // File pointer for the log file
pthread_mutex_t log_mut; // Lock for the log file

int write_log(char *info) {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	pthread_mutex_lock (&log_mut);
	fprintf(log_fp, "[%d:%d:%d  %d-%d-%d] %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, info);
	fflush(log_fp);
	pthread_mutex_unlock (&log_mut);
}

/* Reads a file file and places the contents of the file in file_buffer and the length in length.*/
int read_file(char *file, long *length, char **file_buffer) {
	FILE *fp;
	if ((fp = fopen(file, "rb")) == NULL) {
		// 404 File Not Found
		perror("Failed to open file");
		char tmp[200];
		sprintf(tmp, "read_file: Failed to open file (length: %ld): %s", *length, file);
		write_log(tmp);
		return -1;
	} else {
		// 200 OK
		if (fseek(fp, 0, SEEK_END) != 0) { // Seek last byte
			perror("fseek");
			write_log("read_file: fseek failed");
			return -1;
		} else if (((*length = ftell(fp)) < 0)) { // Find length from beginning to end of file
			perror("ftell");
			char tmp[200];
			sprintf(tmp, "read_file: ftell failed, length: %ld", *length);
			write_log(tmp);
			return -1;
		} else if (fseek(fp, 0, SEEK_SET) != 0) { // Go back to the beginning of the file
			perror("fseek");
			write_log("read_file: fseek failed");
			return -1;
		} else if (((*file_buffer = malloc(*length + 1)) == NULL)) { // Allocate memory for string to store the file
			perror("malloc");
			write_log("read_file: malloc failed");
			return -1;
		} else if (fread(*file_buffer, 1, *length, fp) != *length) { // Read whole file into string
			perror("fread");
			write_log("read_file: fread failed");
			return -1;
		} else if (fclose(fp) != 0) { // Close the file
			perror("fclose");
			write_log("read_file: fclose failed");
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
		char tmp[BUFFER_SIZE];
		sprintf(tmp, "send_response: Failed to write message:\nheader:\n%s\nbody:\n%s", header, body);
		write_log(tmp);
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
		char tmp[200 + BUFFER_SIZE];
		sprintf(tmp, "send_file: Failed to write header:\n%s", header);
		write_log(tmp);
		return -1;
	}
	// Send the file
	if (write(s, file, length) != length) {
		perror("write");
		char tmp[200 + BUFFER_SIZE];
		sprintf(tmp, "send_file: Failed to write file (length: %ld):\n%s", length, file);
		write_log(tmp);
		return -1;
	}
	free(send);
	free(file);
	return 0;
}

// Function passed to qsort to use strcmp
int cmpstr(const void *a, const void *b) 
{ 
    const char **ia = (const char **)a;
    const char **ib = (const char **)b;
    return strcmp(*ia, *ib);
} 

/*
Creates the HTML that displays the files and directories within a directory
resource = "/" or "/uploads"
resource_dir = "/.." or "/../uploads"
update = true if the page being sent is a response to a php request (e.g. uploading a file)
response = if update, contains the response from the php server (e.g. error messages)
	Must be freed
*/
int list_directory(char *resource, char *resource_dir, struct stat file_stats, const int s, bool update, char *response) {
	// Directory info
	DIR *directory;
	struct dirent *entry;
	
	if ((directory = opendir(resource_dir)) == NULL) {
		// Error 500
		fprintf(stderr, "Cannot find file/directory1 %s\n", resource);
		char tmp[200];
		sprintf(tmp, "list_directory: Cannot find file/directory1: resource: %s", resource);
		write_log(tmp);
		if (send_response(s, error500, error500http) != 0) {
			perror("send_response error500");
			write_log("list_directory: send_response: Failed to send error500 response");
			return -1;
		}
		free(resource_dir);
		if (update) free(response);
		return -1;
	}
	
	// Find number of directories (-2)
	// Find number of files
	// Allocate array for files, array for directories
	// *files[num_files]
	// *dirs[num_dirs]
	// Sort them independently with qsort
	
	// Find number of files in directory
	char httpline[300];
	char file_name[200];
	char file_path[202];
	char tmp[200];
	int num_files = 0;
	int num_dirs = 0;
	
	while ((entry = readdir(directory)) != NULL) {
		if (strcmp(resource, "/") != 0) {
			strcpy(file_name, resource);
			strcat(file_name, "/");
		} else {
			strcpy(file_name, "/");
		}
		strcat(file_name, entry->d_name);
		strcpy(file_path, "..");
		strcat(file_path, file_name);
		// Fetch stats on the file/directory
		if (stat(file_path, &file_stats) != 0) {
			perror("stat");
			char tmp[200];
			sprintf(tmp, "list_directory: stat failed: file_path: %s", file_path);
			write_log(tmp);
			return -1;
		}
		if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 // Directory and not "."
				&& strcmp(entry->d_name, "..") != 0) { // Not ".." either
			num_dirs++;
		} else if (entry->d_type == DT_REG // File
				&& !(file_stats.st_mode & S_IXUSR) // Not executable
				&& (entry->d_name[0] != '.')) { // Doesn't start with "."
			num_files++;
		}
	}
	closedir(directory);
	
	char **dirs = malloc(num_dirs * sizeof(char*));
	char **files = malloc(num_files * sizeof(char*));
	
	char *css = malloc(BUFFER_SIZE + strlen(response));
	char *send_dir = malloc(BUFFER_SIZE + 512 * num_dirs);
	char *send_files = malloc(BUFFER_SIZE + 512 * num_files);
	char *upload_files = malloc(BUFFER_SIZE);
	// Format the beginning of the HTML
	strcpy(css, "<style>\
			table {\
			    font-family: arial, sans-serif;\
			    border-collapse: collapse;\
			    width: 70%;\
		    	    margin: 25px;\
			}\
			td, th {\
			    border: 1px solid #dddddd;\
			    text-align: left;\
			    padding: 8px;\
			}\
			td.size {\
				text-align: right;\
			}\
			tr:nth-child(even) {\
			    background-color: #dddddd;\
			}\
			.alert {\
			    padding: 20px;\
			    background-color: #2196f3;\
			    color: white;\
			}\
			.alert.success {background-color: #4caf50;}\
			.alert.error {background-color: #f44336;}\
			.closebtn {\
			    margin-left: 15px;\
			    color: white;\
			    font-weight: bold;\
			    float: right;\
			    font-size: 22px;\
			    line-height: 20px;\
			    cursor: pointer;\
			    transition: 0.3s;\
			}\
			.closebtn:hover {\
			    color: black;\
			}\
			input {\
				font-family: arial, sans-serif;\
			}\
			input[type=text] {\
				margin-left: 25px;\
			}\
			input[type=submit].delete {\
				margin: 0px;\
				border: none;\
				background-color: transparent;\
				padding: 0px;\
				transition: 0.3s;\
				font-weight: bold;\
				line-height: 20px;\
				font-size: 30px;\
				float: right;\
			}\
			input[type=submit].delete:hover {\
				color: red\
			}\
		</style>");
	
	// Create the path to the current directory next to the Directory heading
	char *resource_copy = strdup(resource);
	char path_to_dir[BUFFER_SIZE];
	char *token;
	char path[200];
	strcpy(path, "");
	strcpy(path_to_dir, "&#160;&#160;&#160;<a href=\"/\">home</a>");
	token = strtok(resource_copy, "/");
	// Check whether we are in the uploads folder 
	bool uploads = false;
	if (token != NULL) {
		uploads = (strcmp(token, "uploads") == 0);
	}
	while (token != NULL) {
		strcat(path, "/");
		strcat(path, token);
		
		sprintf(tmp, "&#160;&#160;&#160;>&#160;&#160;&#160;<a href=\"%s\">%s</a>", path, token);
		strcat(path_to_dir, tmp);
		
		token = strtok(NULL, "/");
	}
	free(resource_copy);
	
	sprintf(send_dir, "<table>\
			<col width=\"90%%\">\
			<col width=\"10%%\">\
			<tr>\
				<th>Directory   %s</th>\
				%s\
			</tr>", path_to_dir, uploads ? "<th>Delete</th>" : "");
	sprintf(send_files, "<table>\
			<col width=\"70%%\">\
			<col width=\"20%%\">\
			<col width=\"10%%\">\
			<tr>\
				<th>File</th>\
				<th>Size</th>\
				%s\
			</tr>", uploads ? "<th>Delete</th>" : "");
	
	if ((directory = opendir(resource_dir)) == NULL) {
		// Error 500
		fprintf(stderr, "Cannot find file/directory2 %s\n", resource);
		char tmp[200];
		sprintf(tmp, "list_directory: Cannot find file/directory2: resource: %s", resource);
		write_log(tmp);
		if (send_response(s, error500, error500http) != 0) {
			perror("send_response error500");
			write_log("list_directory: send_response: Failed to send error500 response");
			return -1;
		}
		return -1;
	}
	
	int count_dirs = 0;
	int count_files = 0;
	
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
		// Fetch stats on the file/directory
		if (stat(file_path, &file_stats) != 0) {
			perror("stat");
			return -1;
		}
		// Create HTML link to file/directory
		if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 // Directory and not "."
				&& strcmp(entry->d_name, "..") != 0) { // Not ".." either
			sprintf(tmp, "<td><form action=\"%s/delete.php?file=%s&path=%s\" method=\"post\" onclick=\"return confirm('Are you sure you want to delete %s and all of its contents?');\">\
	  			<input type=\"submit\" class=\"delete\" value=\"&#160;&#160;&#160;&times;&#160;&#160;&#160;\">\
				</form></td>", resource, entry->d_name, resource, entry->d_name);
			
			sprintf(httpline, "<tr><td><a href=\"%s\">%s</a></td>%s</tr>", 
				file_name, entry->d_name, uploads ? tmp : "");
			
			dirs[count_dirs] = strdup(httpline);
			count_dirs++;
		} else if (entry->d_type == DT_REG // File
				&& !(file_stats.st_mode & S_IXUSR) // Not executable
				&& (entry->d_name[0] != '.')) { // Doesn't start with "."
			sprintf(tmp, "<td><form action=\"%s/delete.php?file=%s&path=%s\" method=\"post\" onclick=\"return confirm('Are you sure you want to delete %s?');\">\
	  			<input type=\"submit\" class=\"delete\" value=\"&#160;&#160;&#160;&times;&#160;&#160;&#160;\">\
				</form></td>", resource, entry->d_name, resource, entry->d_name);
			
			sprintf(httpline, "<tr><td><a href=\"%s\">%s</a></td><td class=\"size\">%ld bytes</td>%s</tr>", 
				file_name, entry->d_name, file_stats.st_size, uploads ? tmp : "");
			
			files[count_files] = strdup(httpline);
			count_files++;
		}
	}
	qsort(dirs, num_dirs, sizeof(char*), cmpstr);
	qsort(files, num_files, sizeof(char*), cmpstr);
	
	for (int i = 0; i < num_dirs; i++) {
		strcat(send_dir, dirs[i]);
		free(dirs[i]);
	}
	for (int i = 0; i < num_files; i++) {
		strcat(send_files, files[i]);
		free(files[i]);
	}
	free(dirs);
	free(files);
	
	// Send data
	sprintf(tmp, "</table>\
			<form action=\"%s/mkdir.php?path=%s\" method=\"post\">\
				<input type=\"text\" name=\"name\" placeholder=\"Directory name\" size=\"10\">\
	  			<input type=\"submit\" value=\"Create Directory\">\
			</form>", resource, resource);
	strcat(send_dir, uploads ? tmp : "</table>");
	strcat(send_files, "</table>");
	if (uploads) {
		sprintf(upload_files, "<form action=\"%s/upload.php?path=%s\" method=\"post\" enctype=\"multipart/form-data\">\
			  			<span style=\"padding-left:25px;font-family: arial, sans-serif;\">Select files: </span>\
			  			<input type=\"file\" name=\"file_to_upload[]\" id=\"file_to_upload\" multiple>\
			  			<input type=\"submit\" value=\"Upload Here\" name=\"submit\">\
					</form>", resource, resource);
	} else {
		sprintf(upload_files, "<span style=\"padding-left:25px;font-family: arial, sans-serif;\">\
						See the <a href=\"uploads\">uploads</a> directory to use the file server.\
					</span>");
	}
	if (update) {
		char alert[BUFFER_SIZE + strlen(response)];
		char type[20];
		if (strstr(response, "Success") && strstr(response, "Error")) {
			strcpy(type, "");
		} else if (strstr(response, "Success")) {
			strcpy(type, "success");
		} else if (strstr(response, "Error")) {
			strcpy(type, "error");
		} else {
			strcpy(type, "");
		}
		sprintf(alert, "<div class=\"alert %s\">\
				<span class=\"closebtn\" onclick=\"this.parentElement.style.display='none';\">&times;</span> \
				%s\
			</div>", type, response);
		strcat(css, alert);
	}
	char *send = malloc(strlen(css) + strlen(send_dir) + strlen(send_files) + strlen(upload_files) + 1);
	strcpy(send, css);
	strcat(send, send_dir);
	strcat(send, send_files);
	strcat(send, upload_files);
	if (send_response(s, status200, send) != 0) {
		perror("send_response");
		write_log("list_directory: send_response: Failed to send status200 response");
		return -1;
	}
	free(css);
	free(send_dir);
	free(send_files);
	free(send);
	free(upload_files);
	free(resource_dir);
	if (update) {
		free(response);
	}
	closedir(directory);
	return 0;
}

int send_page(char *resource, char *working_dir, struct stat file_stats, const int s, bool update, char *response) {
	// Remove trailing "/"'s
	while (strlen(resource) > 0 && resource[strlen(resource) - 1] == '/') {
		resource[strlen(resource) - 1] = '\0';
	}
	// Get correct path to request directory
	// (Since code is running in /src we need to go up a directory)
	char *resource_dir = malloc(strlen(working_dir) + 2);
	strcpy(resource_dir, "..");
	strcat(resource_dir, resource);
	// Open the directory resource_dir, e.g. ./testdir
	if (list_directory(resource, resource_dir, file_stats, s, update, response) != 0) {
		perror("list_directory");
		char tmp[200];
		sprintf(tmp, "send_page: list_directory failed: resource: %s, resource_dir: %s, update: %s, response: %s", 
			resource, resource_dir, update ? "true" : "false", response);
		return -1;
	}
	return 0;
}

// http://man7.org/linux/man-pages/man3/getaddrinfo.3.html
int run_php(char *msg, int sfd, size_t bytes, int uploading, const int s, char *resource, char *working_dir, struct stat file_stats, bool first_read) {
	int j;
	size_t len;
	ssize_t nread;
	char buffer[BUFFER_SIZE * 2];
	
	len = bytes;

	if (len + 1 > BUFFER_SIZE) {
		fprintf(stderr, "Can't send to php server; request too large\n");
		char tmp[200];
		sprintf(tmp, "run_php: Can't send to php server: Request too large (%ld)", len);
		write_log(tmp);
		return -1;
	}
	
	// Edit the headers being sent to the php server
	if (first_read) {
		char php[20];
		if (strstr(resource, "upload.php") != NULL) {
			strcpy(php, "upload.php");
		} else if (strstr(resource, "delete.php") != NULL) {
			strcpy(php, "delete.php");
		} else if (strstr(resource, "mkdir.php") != NULL) {
			strcpy(php, "mkdir.php");
		}
		char path[BUFFER_SIZE];
		char *ptr = NULL;
		
		// Find the correct path, e.g. /src/upload.php --> /src/
		ptr = strstr(msg, "/");
		strcpy(path, ptr);
		strtok(path, "\n");
		ptr = strstr(path, php);
		*ptr = '\0';
		strcpy(resource, path);
		
		// Remove the /src/ part from the message being sent to server
		ptr = strstr(msg, path);
		int i = 0;
		while (*ptr == path[i]) {
			i++;
			ptr++;
		}
		char tmp[BUFFER_SIZE];
		strcpy(tmp, msg);
		strtok(msg, "/");
		sprintf(tmp, "%s/%s", msg, ptr);
		strcpy(msg, tmp);
		
		len -= strlen(path) - 1;
	}
	

	if (write(sfd, msg, len) != len) {
		fprintf(stderr, "partial/failed write\n");
		write_log("run_php: Failed write");
		return -1;
	}
	if (!uploading) {
		// Check response from php server
		nread = read(sfd, buffer, BUFFER_SIZE * 2);
		if (nread == -1) {
			perror("read");
			write_log("run_php: Failed read");
			return -1;
		}
		buffer[nread] = '\0';
		// Remove headers from response
		char *response;
		if (nread != 0 && (response = strstr(buffer, "\r\n\r\n") + 4) != NULL) {
			// Remove double line at end
			char *end;
			if ((end = strstr(response, "\n\n")) != NULL) *end = '\0';
		} else {
			response = "There was a problem uploading your file";
			char tmp[BUFFER_SIZE];
			sprintf(tmp, "run_php: Failed to parse response from php server:\n%s", buffer);
			write_log(tmp);
		}
		// Send back page with added popup with response message
		if (send_page(resource, working_dir, file_stats, s, true, strdup(response)) != 0) {
			perror("send_page");
			char tmp[BUFFER_SIZE];
			sprintf(tmp, "run_php: send_page: resource: %s, working_dir: %s, response: %s", resource, working_dir, response);
			write_log(tmp);
		}
	}
	return 0;
}

// http://man7.org/linux/man-pages/man3/getaddrinfo.3.html
int connect_php(int *sfd) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	/* Obtain address(es) matching host/port */
	
	printf("Connecting to PHP server... ");

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;          /* Any protocol */

	s = getaddrinfo("localhost", "8000", &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		perror("Could not connect to php server");
		char tmp[200];
		sprintf(tmp, "connect_php: Could not connect to php server (s = %d)", s);
		write_log(tmp);
		return -1;
	}

	/* getaddrinfo() returns a list of address structures.
	Try each address until we successfully connect(2).
	If socket(2) (or connect(2)) fails, we (close the socket
	and) try the next address. */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		*sfd = socket(rp->ai_family, rp->ai_socktype,
		    rp->ai_protocol);
		if (*sfd == -1)
			continue;

		if (connect(*sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */

		close(*sfd);
	}

	if (rp == NULL) {               /* No address succeeded */
		perror("Could not connect to php server");
		write_log("connect_php: Could not connect to php server (rp = NULL)");
		return -1;
	}

	freeaddrinfo(result);           /* No longer needed */
	
	printf("Connected\n");
}

int parse_http_headers(char *buffer, char **host, char **request, char **resource, int *clen) {
	char *cont_len;
	char *host_tmp;
	char *request_tmp;
	char *resource_tmp;
	if ((cont_len = strstr(buffer, "Content-Length: ")) != NULL) {
		cont_len = strtok(cont_len, "\r\n");
		cont_len += 16;
		char *endp; // Check that content_length is a number
		*clen = strtol(cont_len, &endp, 10);
		if (*endp != '\0') {
			fprintf(stderr, "%s is not a number\n", cont_len);
			char tmp[200];
			sprintf(tmp, "parse_http_headers: Content-Length (\"%s\") is not a number", cont_len);
			write_log(tmp);
			return -1;
		}
	} else {
		*clen = -1;
	}
	
	host_tmp = strstr(buffer, "Host: "); // Get pointer to start of host line
	host_tmp = strtok(host_tmp, "\r\n"); // Remove everything after the host line (not needed)
	host_tmp += 6; // Remove "Host: " to get just host:port, e.g. localhost:8080
	if (*host != NULL) free(*host);
	*host = strdup(host_tmp);
	
	request_tmp = strtok(buffer, " "); // Get everything before the first space (i.e. "GET", "POST" etc)
	if (*request != NULL) free(*request);
	*request = strdup(request_tmp);
		
	resource_tmp = strtok(NULL, " "); // Get second word in request, e.g. / or /test.txt
	if (*resource != NULL) free(*resource);
	*resource = strdup(resource_tmp);
	return 0;
}

int service_client_socket(const int s, const char * const tag, FILE *fp, pthread_mutex_t mut) {
	log_fp = fp;
	log_mut = mut;

	int sfd = -1; // Connection info for php server
	char buffer[BUFFER_SIZE];
	memset(buffer, '\0', BUFFER_SIZE);
	size_t bytes;
	char *request = NULL; // e.g. GET
	char *resource = NULL; // e.g.  /  or  /test.txt
	char *host = NULL; // e.g. localhost:8080
	bool uploading = false; // Are we expecting multiple reads from one request?
	bool first_read = true; // Is this the first read for this request?
	// File info
	char *file_buffer;
	long length;
	// File info - Find if file is executable
	struct stat file_stats;
	// Current working directory
	char cwd[BUFFER_SIZE];
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
	//fprintf(stdout, "Up one: %s\n", cwd);

	printf("New connection from %s\n", tag);
	
	int bytes_read, bytes_total;

	// Repeatedly read data from the client
	while ((bytes = read(s, buffer, BUFFER_SIZE - 1)) > 0) {
		// Create a copy of the buffer for editing
		char *buffercopy = malloc(sizeof(buffer));
		memcpy(buffercopy, buffer, BUFFER_SIZE);
		if (!uploading) {
			connect_php(&sfd);
			first_read = true;
			// Parse HTTP request:
			if (parse_http_headers(buffercopy, &host, &request, &resource, &bytes_total) != 0) {
				perror("parse_http_headers");
				char tmp[BUFFER_SIZE * 2];
				sprintf(tmp, "parse_http_headers: host: %s, request: %s, resource: %s, bytes_total: %d\n\
					Buffer:\n==========\n%s\n==========", host, request, resource, bytes_total, buffer);
				write_log(tmp);
				close(s);
				return -1;
			}
			bytes_read = 0;
			bytes_read += bytes - (strstr(buffer, "\r\n\r\n") - buffer); // Bytes - distance from start of buffer to end of headers
		} else {
			first_read = false;
			bytes_read += bytes;
		}
		// Using cwd and the requested resource, find the path to the resource
		char *working_dir = malloc(strlen(cwd) + strlen(resource) + 1);
		strcpy(working_dir, cwd);
		strcat(working_dir, resource);

		// If the request is a file transfer
		if (!uploading && strcmp(request, "GET") == 0) { // GET request
			if (stat(working_dir, &file_stats) == 0 && !(file_stats.st_mode & S_IXUSR)) { // Do not show executable files
				// Call read_file to copy the file data into file_buffer
				if (read_file(working_dir, &length, &file_buffer) == -1) {
					// 500 Internal Server Error
					perror("Failed to read file");
					char tmp[200];
					sprintf(tmp, "read_file: Failed to read file: working_dir: %s", working_dir);
					write_log(tmp);
					if (send_response(s, error500, error500http) == -1) {
						perror("Failed to send fail response");
						write_log("send_response: Failed to send error500 response (read_file)");
					}
				} else {
					// Send the file
					send_file(s, status200, file_buffer, length);
				}
			} else { // Not a file, so must be a directory
				send_page(resource, working_dir, file_stats, s, false, "");
			}
		} else if (uploading || strcmp(request, "POST") == 0) { // POST request
			if (bytes_read < bytes_total) {
				uploading = true;
			} else {
				uploading = false;
			}
			run_php(buffer, sfd, bytes, uploading, s, resource, working_dir, file_stats, first_read);
		} else {
			// Error 501
			fprintf(stderr, "Invalid request: %s", request);
			if (send_response(s, error501, error501http) == -1) {
				perror("send_response err501");
				write_log("send_response: Failed to send error501 response");
				close(s);
				return -1;
			}
		}
		free(buffercopy);
		free(working_dir);
		memset(buffer, '\0', BUFFER_SIZE);
	}
	if (bytes != 0) {
		perror("read");
		return -1;
	}
	
	free(host);
	free(request);
	free(resource);
	
	printf("Connection from %s closed\n", tag);
	close(s);
	return 0;
}

