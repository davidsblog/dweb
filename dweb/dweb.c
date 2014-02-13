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

void send_file_response(char*, char*, int, http_verb);
void test_response(char*, char*, int, http_verb);

int main(int argc, char **argv)
{
	if (argc != 2 || !strcmp(argv[1], "-?"))
	{
		printf("hint: dweb [port number]\n");
		exit(0);
	}
	dwebserver(atoi(argv[1]), &send_file_response);
	//dwebserver(atoi(argv[1]), &test_response);
}

void test_response(char *request, char *body, int socketfd, http_verb type)
{
	ok_200(socketfd,
		"<html><head><title>Test Page</title></head>"
		"<body><h1>Testing...</h1>This is a test response.</body>"
		"</html>", request);
}

void send_file_response(char *request, char *body, int socketfd, http_verb type)
{
	int file_id, path_length, i = 0;
	long len;
	char *content_type = NULL, response[BUFSIZE+1];
	char *form_names[MAX_FORM_VALUES], *form_values[MAX_FORM_VALUES];
	
	path_length=(int)strlen(request);
	if (path_length==0)
	{
		return send_file_response("index.html", body, socketfd, type);
	}
	
	i = get_form_values(body, form_names, form_values, MAX_FORM_VALUES);
	if (i == 2)
	{
		sprintf(response, "<html><head><title>Response Page</title></head>"
			"<body><h1>Thanks...</h1>You entered:<br/>"
			"%s is %s<br/>"
			"%s is %s<br/>"
			"</body></html>", form_names[0], form_values[0], form_names[1], form_values[1]);
		
		return ok_200(socketfd, response, request);
	}
	
	// work out the file type and check we support it
	for (i=0; extensions[i].ext != 0; i++)
	{
		len = strlen(extensions[i].ext);
		if( !strncmp(&request[path_length-len], extensions[i].ext, len))
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
	
	if (file_id = open(request, O_RDONLY), file_id == -1)
	{
		notfound_404(socketfd, "failed to open file");
		exit(3);
	}
	
	// open the file for reading
	len = (long)lseek(file_id, (off_t)0, SEEK_END); // lseek to the file end to find the length
	lseek(file_id, (off_t)0, SEEK_SET); // lseek back to the file start ready for reading
    sprintf(response, "HTTP/1.1 200 OK\nServer: dweb\nContent-Length: %ld\nConnection: close\nContent-Type: %s\r\n\r\n", len, content_type); // headers
	write(socketfd, response, strlen(response));

	// send file in blocks (the last block could be smaller)
	while ((len = read(file_id, response, BUFSIZE)) > 0)
	{
		write(socketfd, response, len);
	}
	sleep(1);	// allow socket to drain before signalling the socket is closed
	close(socketfd);
}

