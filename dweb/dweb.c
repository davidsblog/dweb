#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "dwebsvr.h"

#define MAX_FORM_VALUES 10

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpg" }, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"ico", "image/ico" },  
	{"zip", "image/zip" },  
	{"gz",  "image/gz"  },  
	{"tar", "image/tar" },  
	{"htm", "text/html" },  
	{"html","text/html" },  
	{"js","text/javascript" },
	{0,0} };

void send_response(char*, char*, int, http_verb);
void send_api_response(char*, char*, int);
void send_file_response(char*, char*, int, int);

int main(int argc, char **argv)
{
	if (argc != 2 || !strcmp(argv[1], "-?"))
	{
		printf("hint: dweb [port number]\n");
		exit(0);
	}
    logger(LOG, "dweb server starting\nPress CTRL+C to quit", "", 0);
	dwebserver(atoi(argv[1]), &send_response);
}

// decide if we need to send an API response or a file...
void send_response(char *path, char *request_body, int socketfd, http_verb type)
{
    int path_length=(int)strlen(path);
    if (!strncmp(&path[path_length-3], "api", 3))
	{
		return send_api_response(path, request_body, socketfd);
	}
    if (path_length==0)
	{
        return send_file_response("index.html", request_body, socketfd, 10);
	}
    send_file_response(path, request_body, socketfd, path_length);
}

// a simple API, it receives a number, increments it and returns the response
void send_api_response(char *path, char *request_body, int socketfd)
{
	char *form_names[1], *form_values[1];
	char response[4];
	int i = get_form_values(request_body, form_names, form_values, 1);
	
	if (i==1 && !strncmp(form_names[0],"counter", strlen(form_names[0])))
	{
		int c = atoi(form_values[0]);
		if (c>998) c=0;
		sprintf(response, "%d", ++c);
		return ok_200(socketfd, response, path);
	}
	else
	{
		return forbidden_403(socketfd, "Bad request");
	}
}

void send_file_response(char *path, char *request_body, int socketfd, int path_length)
{
	int file_id, i;
	long len;
	char *content_type = NULL, response[BUFSIZE+1];
	char *form_names[MAX_FORM_VALUES], *form_values[MAX_FORM_VALUES];
	
	i = get_form_values(request_body, form_names, form_values, MAX_FORM_VALUES);
	if (i == 2)
	{
		sprintf(response, "<html><head><title>Response Page</title></head>"
			"<body><h1>Thanks...</h1>You entered:<br/>"
			"%s is %s<br/>"
			"%s is %s<br/>"
			"</body></html>", form_names[0], form_values[0], form_names[1], form_values[1]);
		
		return ok_200(socketfd, response, path);
	}
	
	// work out the file type and check we support it
	for (i=0; extensions[i].ext != 0; i++)
	{
		len = strlen(extensions[i].ext);
		if( !strncmp(&path[path_length-len], extensions[i].ext, len))
		{
			content_type = extensions[i].filetype;
			break;
		}
	}
	if (content_type==NULL)
	{
		forbidden_403(socketfd, "file extension type not supported");
		exit(3);
	}
	
	if (file_id = open(path, O_RDONLY), file_id == -1)
	{
		notfound_404(socketfd, "failed to open file");
		exit(3);
	}
	
	// open the file for reading
	len = (long)lseek(file_id, (off_t)0, SEEK_END); // lseek to the file end to find the length
	lseek(file_id, (off_t)0, SEEK_SET); // lseek back to the file start ready for reading
    sprintf(response, "HTTP/1.1 200 OK\nServer: dweb\nContent-Length: %ld\nConnection: close\n"
            "Content-Type: %s\r\n\r\n", len, content_type); // headers
	write(socketfd, response, strlen(response));

	// send file in blocks (the last block could be smaller)
	while ((len = read(file_id, response, BUFSIZE)) > 0)
	{
		write(socketfd, response, len);
	}
    // allow socket to drain before signalling the socket is closed
	sleep(1);
	close(socketfd);
}