#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "dwebsvr.h"

#define FILE_CHUNK_SIZE 1024
#define BIGGEST_FILE 104857600 // 100 Mb

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
void log_filter(log_type, char*, char*, int);
void send_api_response(char*, char*, int);
void send_file_response(char*, char*, int, int);

int main(int argc, char **argv)
{
    if (argc != 2 || !strcmp(argv[1], "-?"))
	{
		printf("hint: dweb [port number]\n");
		return 0;
	}
    puts("dweb server starting\nPress CTRL+C to quit");
	dwebserver(atoi(argv[1]), &send_response, &log_filter);
}

void log_filter(log_type type, char *s1, char *s2, int socket_fd)
{
    if (type!=ERROR) return;
    printf("ERROR: %s: %s (errno=%d pid=%d socket=%d)\n",s1, s2, errno, getpid(), socket_fd);
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
	char response[4];
	
	if (form_value_count()==1 && !strncmp(form_name(0), "counter", strlen(form_name(0))))
	{
		int c = atoi(form_value(0));
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
	char *content_type = NULL;
    STRING *response = new_string(FILE_CHUNK_SIZE);
	
	if (form_value_count() == 2)
	{
        string_add(response, "<html><head><title>Response Page</title></head>");
        string_add(response, "<body><h1>Thanks...</h1>You entered:<br/>");
        string_add(response, form_name(0));
        string_add(response, " is ");
        string_add(response, form_value(0));
        string_add(response, "<br/>");
        string_add(response, form_name(1));
        string_add(response, " is ");
        string_add(response, form_value(1));
        string_add(response, "<br/>");
        string_add(response, "</body></html>");
		
		ok_200(socketfd, string_chars(response), path);
        string_free(response);
        return;
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
	if (content_type == NULL)
	{
		string_free(response);
        return forbidden_403(socketfd, "file extension type not supported");
	}
	
	if (file_id = open(path, O_RDONLY), file_id == -1)
	{
		string_free(response);
        return notfound_404(socketfd, "failed to open file");
	}
	
	// open the file for reading
	len = (long)lseek(file_id, (off_t)0, SEEK_END); // lseek to the file end to find the length
	lseek(file_id, (off_t)0, SEEK_SET); // lseek back to the file start
    
    if (len > BIGGEST_FILE)
    {
        string_free(response);
        return forbidden_403(socketfd, "files this large are not supported");
    }
    
    string_add(response, "HTTP/1.1 200 OK\nServer: dweb\n");
    string_add(response, "Connection: close\n");
    string_add(response, "Content-Type: ");
    string_add(response, content_type);
    write_header(socketfd, string_chars(response), len);
    
	// send file in blocks
	while ((len = read(file_id, response->ptr, FILE_CHUNK_SIZE)) > 0)
	{
		write(socketfd, response->ptr, len);
	}
    string_free(response);
    
    // allow socket to drain before closing
	sleep(1);
}