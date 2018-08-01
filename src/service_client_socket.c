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

#define BUFFER_SIZE 2048
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
int list_directory(DIR *directory, char *resource, char *resource_dir, struct stat file_stats, const int s, bool update, char *response) {
	// Directory info
	struct dirent *entry;
	
	if ((directory = opendir(resource_dir)) == NULL) {
		// Error 500
		fprintf(stderr, "Cannot find file/directory1 %s\n", resource);
		if (send_response(s, error500, error500http) != 0) {
			perror("send_response error500");
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
	
	//printf("num_dirs: %d, num_files: %d\n", num_dirs, num_files);
	
	//char *dirs[num_dirs];
	char **dirs = malloc(num_dirs * sizeof(char*));
	//char *files[num_files];
	char **files = malloc(num_files * sizeof(char*));
	
	char *css = malloc(BUFFER_SIZE + strlen(response));
	char *send_dir = malloc(BUFFER_SIZE + 256 * num_dirs);
	char *send_files = malloc(BUFFER_SIZE + 256 * num_files);
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
	
	//printf("RESOURCE: %s\n", resource);
	
	// Create the path to the current directory next to the Directory heading
	char *resource_copy = strdup(resource);
	char path_to_dir[BUFFER_SIZE];
	char *token;
	char path[200];
	strcpy(path, "");
	strcpy(path_to_dir, "&#160;&#160;&#160;<a href=\"/\">home</a>");
	token = strtok(resource_copy, "/");
	// Check whether we are in the uploads folder 
	//printf("TOK: %s\n", token);
	bool uploads = false;
	if (token != NULL) {
		uploads = (strcmp(token, "uploads") == 0);
	}
	//printf("Uploads: %s\n", uploads ? "true" : "false");
	while (token != NULL) {
		strcat(path, "/");
		strcat(path, token);
		
		//printf("Dir: %s\n", token);
		//printf("Path: %s\n", path);
		
		sprintf(tmp, "&#160;&#160;&#160;>&#160;&#160;&#160;<a href=\"%s\">%s</a>", path, token);
		//printf("%s\n", tmp);
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
		if (send_response(s, error500, error500http) != 0) {
			perror("send_response error500");
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
		// printf("name: %s, type: %s, size: %ld, links: %lu\n", file_name, entry->d_type == DT_DIR ? "dir" : "file", file_stats.st_size, file_stats.st_nlink);
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
	/*
	printf("\nBefore: \n");
	for (int i = 0; i < num_dirs; i++) {
		printf("%s\n", dirs[i]);
	}
	*/
	qsort(dirs, num_dirs, sizeof(char*), cmpstr);
	/*
	printf("\nAfter: \n");
	for (int i = 0; i < num_dirs; i++) {
		printf("%s\n", dirs[i]);
	}
	*/
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
	//printf("UPDATE: %s\n", update ? "true" : "false");
	if (update) {
		printf("RESP: %s\n", response);
		//strcat(upload_files, response);
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
	// Not sure if needed
	free(entry);
	return 0;
}

int send_page(char *resource, char *working_dir, struct stat file_stats, const int s, bool update, char *response) {
	DIR *directory;
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
	if (list_directory(directory, resource, resource_dir, file_stats, s, update, response) != 0) {
		perror("list_directory");
		return -1;
	}
	return 0;
}

// You must free the result if result is non-NULL.
// https://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c
char *str_replace(char *orig, char *rep, char *with) {
	char *result; // the return string
	char *ins;    // the next insert point
	char *tmp;    // varies
	int len_rep;  // length of rep (the string to remove)
	int len_with; // length of with (the string to replace rep with)
	int len_front; // distance between rep and end of last rep
	int count;    // number of replacements

	// sanity checks and initialization
	if (!orig || !rep)
		return NULL;
	len_rep = strlen(rep);
	if (len_rep == 0)
		return NULL; // empty rep causes infinite loop during count
	if (!with)
	with = "";
	len_with = strlen(with);

	// count the number of replacements needed
	ins = orig;
	for (count = 0; tmp = strstr(ins, rep); ++count) {
		ins = tmp + len_rep;
	}

	tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

	if (!result)
		return NULL;

	// first time through the loop, all the variable are set correctly
	// from here on,
	//    tmp points to the end of the result string
	//    ins points to the next occurrence of rep in orig
	//    orig points to the remainder of orig after "end of rep"
	while (count--) {
		ins = strstr(orig, rep);
		len_front = ins - orig;
		tmp = strncpy(tmp, orig, len_front) + len_front;
		tmp = strcpy(tmp, with) + len_with;
		orig += len_front + len_rep; // move to next "end of rep"
	}
	strcpy(tmp, orig);
	return result;
}

// http://man7.org/linux/man-pages/man3/getaddrinfo.3.html
int run_php(char *msg, int sfd, size_t bytes, int uploading, const int s, char *resource, char *working_dir, struct stat file_stats, bool first_read) {
	int j;
	size_t len;
	ssize_t nread;
	char buffer[BUFFER_SIZE * 2];
	
	/* Send remaining command-line arguments as separate
	datagrams, and read responses from server */
	
	//msg = str_replace(msg, "localhost:8080", "localhost:8000");

	len = bytes;//strlen(msg);
	//printf("len: %ld\n", len);

	if (len + 1 > BUFFER_SIZE) {
		fprintf(stderr, "Can't send to php server; request too large\n");
		return -1;
	}
	//msg[bytes] = '\0';
	//printf("\n----------\n%s\n----------\n", msg);
	
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
		//printf("\nHEADERS\n\n");
		char path[BUFFER_SIZE];
		char *ptr = NULL;
		
	
		// Find the correct path, e.g. /src/upload.php --> /src/
		ptr = strstr(msg, "/");
		strcpy(path, ptr);
		strtok(path, "\n");
		//printf("PHP: %s\n", php);
		//printf("PATH: \n%s\n", path);
		ptr = strstr(path, php);
		*ptr = '\0';
		//printf("PATH: \n%s\n", path);
		strcpy(resource, path);
		//printf("RESOURCE: \n%s\n", resource);
		
		// Remove the /src/ part from the message being sent to server
		//printf("MSG: \n%s\n", msg);
		ptr = strstr(msg, path);
		//printf("ptr: %c path: %c\n", ptr[0], path[0]);
		int i = 0;
		while (*ptr == path[i]) {
			i++;
			ptr++;
			//printf("ptr: %c path: %c\n", *ptr, path[i]);
		}
		//printf("\n\nPTR: \n%s\n%c\n", ptr, *ptr);
		char tmp[BUFFER_SIZE];
		strcpy(tmp, msg);
		strtok(msg, "/");
		//printf("MSG: \n%s\n", msg);
		sprintf(tmp, "%s/%s", msg, ptr);
		strcpy(msg, tmp);
		//printf("\n\nPTR: \n%s\n", ptr);
		//printf("MSG: \n%s\n", msg);
		
		len -= strlen(path) - 1;
	}
	
	//printf("\n----------\n%s\n----------\n", msg);

	if (write(sfd, msg, len) != len) {
		fprintf(stderr, "partial/failed write\n");
		return -1;
	}
	if (!uploading) {
		nread = read(sfd, buffer, BUFFER_SIZE * 2);
		if (nread == -1) {
			perror("read");
			return -1;
		}
		buffer[nread] = '\0';
		//printf("Received %zd bytes: %s\n", nread, buffer);
		// Check response from php server
		
		// Remove headers from response
		//printf("strstr response\n");
		char *response;
		if (nread != 0 && (response = strstr(buffer, "\r\n\r\n") + 4) != NULL) {
			// strtok(strstr(response, "\r\n\r\n"), "\r\n");
			// printf("RESPONSE: ---\n%s\n---\n", response);
			// Remove double line at end
			//printf("strstr end\n");
			//printf("len buf: %ld\n", strlen(buffer));
			//printf("len resp: %ld\n", strlen(response));
			char *end;
			if ((end = strstr(response, "\n\n")) != NULL) {
				*end = '\0';
			}
			//printf("strstr end2\n");
			//printf("RESPONSE: ---\n%s\n---\n", response);
		} else {
			response = "There was a problem uploading your file";
		}
		// Send back page with added popup with response message
		//printf("SEND_PAGE\n");
		if (send_page(resource, working_dir, file_stats, s, true, strdup(response)) != 0) {
			perror("list_directory");
		}
		//printf("SEND_PAGE_DONE\n");
	}
	return 0;
}

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

	s = getaddrinfo("127.0.0.1", "8000", &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		perror("Could not connect to php server");
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
			return -1;
		} else {
			//printf("Content-Length: %d\n", *clen);
		}
	} else {
		//printf("No Content-Length\n");
		*clen = -1;
	}
	
	host_tmp = strstr(buffer, "Host: "); // Get pointer to start of host line
	host_tmp = strtok(host_tmp, "\r\n"); // Remove everything after the host line (not needed)
	host_tmp += 6; // Remove "Host: " to get just host:port, e.g. localhost:8080
	//printf("Host: %s\n", *host_tmp);
	if (*host != NULL) free(*host);
	*host = strdup(host_tmp);
	
	request_tmp = strtok(buffer, " "); // Get everything before the first space (i.e. "GET", "POST" etc)
	//printf("Request: %s\n", *request_tmp);
	if (*request != NULL) free(*request);
	*request = strdup(request_tmp);
		
	resource_tmp = strtok(NULL, " "); // Get second word in request, e.g. / or /test.txt
	//printf("Resource: %s\n", *resource_tmp);
	if (*resource != NULL) free(*resource);
	*resource = strdup(resource_tmp);
	
	return 0;
}

int service_client_socket(const int s, const char * const tag) {
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
		// printf("===== BUFFER =====\n%s\n=================\n", buffer);
		// Create a copy of the buffer for editing
		char *buffercopy = malloc(sizeof(buffer));
		memcpy(buffercopy, buffer, BUFFER_SIZE);
		if (!uploading) {
			connect_php(&sfd);
			first_read = true;
			// Parse HTTP request:
			if (parse_http_headers(buffercopy, &host, &request, &resource, &bytes_total) != 0) {
				perror("parse_http_headers");
				//close(s);
				return -1;
			}
			//printf("Host: %s, Request: %s, Resource: %s, Content-Length: %d\n", host, request, resource, bytes_total);
			
			bytes_read = 0;
			//bytes_read += strlen(strstr(buffer, "\r\n\r\n"));
			bytes_read += bytes - (strstr(buffer, "\r\n\r\n") - buffer); // Bytes - distance from start of buffer to end of headers
			//printf("BYTES_READ: %d, BYTES: %ld, HEADERS: %ld\n", bytes_read, bytes, (strstr(buffer, "\r\n\r\n") - buffer));
			//printf("@@@@@\n%s@@@@@\n%ld\n", strstr(buffer, "\r\n\r\n"), strlen(strstr(buffer, "\r\n\r\n")));
			
			if (bytes_total != -1 && bytes_read - 4 > bytes_total) {
				printf("===== BUFFER =====\n%s\n=================\n", buffer);
				fprintf(stderr, "Incorrect Content-Length:\nbytes: %ld, bytes_read: %d, bytes_total: %d\n", bytes, bytes_read, bytes_total);
				fprintf(stderr, "CLOSING CONNECTION\n");
				//close(s);
				//return -1;
			}
		} else {
			first_read = false;
			bytes_read += bytes;
		}
		//printf("bytes_read: %d, bytes_total: %d, bytes: %ld\n", bytes_read, bytes_total, bytes);
		// Using cwd and the requested resource, find the path to the resource
		char *working_dir = malloc(strlen(cwd) + strlen(resource) + 1);
		strcpy(working_dir, cwd);
		strcat(working_dir, resource);

		//printf("resource: %ld, working_dir: %ld\n", strlen(resource), strlen(working_dir));

		//printf("RESOURCE: %s\n", resource);
		//printf("WORKING_DIR: %s\n", working_dir);

		// If the request is a file transfer
		// printf("%s\n", resource);
		
		if (!uploading && strcmp(request, "GET") == 0) { // GET request
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
					free(file_buffer);
				}
			} else { // Not a file, so must be a directory
				send_page(resource, working_dir, file_stats, s, false, "");
			}
		} else if (uploading || strcmp(request, "POST") == 0) { // POST request
			// printf("bytes_read: %d, bytes_total: %d\n", bytes_read, bytes_total);
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
				//close(s);
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

