#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dwebsvr.h"

void (*logger_function)(int, char*, char*, int);

void write_html(int socket_fd, char *head, char *html)
{
	char headbuf[255];
	sprintf(headbuf, "%s\nContent-Length: %d\r\n\r\n", head, (int)strlen(html)+1);
	write(socket_fd, headbuf, strlen(headbuf));
	write(socket_fd, html, strlen(html));
	write(socket_fd, "\n", 1);
}

void forbidden_403(int socket_fd, char *info)
{
	write_html(socket_fd, "HTTP/1.1 403 Forbidden\nServer: dweb\nConnection: close\nContent-Type: text/html", 
		"<html><head>\n<title>403 Forbidden</title>\n"
		"</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple webserver.\n</body>"
		"</html>");
	logger_function(LOG, "403 FORBIDDEN", info, socket_fd);
}

void notfound_404(int socket_fd, char *info)
{
	write_html(socket_fd, "HTTP/1.1 404 Not Found\nServer: dweb\nConnection: close\nContent-Type: text/html",
		"<html><head>\n<title>404 Not Found</title>\n"
		"</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>");

	logger_function(LOG, "404 NOT FOUND", info, socket_fd);
}

void ok_200(int socket_fd, char *html, char *path)
{
	write_html(socket_fd, "HTTP/1.1 200 OK\nServer: dweb\nConnection: close\nContent-Type: text/html", html);
	
	logger_function(LOG, "200 OK", path, socket_fd);
}

void default_logger(int type, char *title, char *description, int socket_fd)
{
	switch (type)
	{
		case ERROR:
			printf("ERROR: %s: %s (errno=%d pid=%d socket=%d)\n",title, description, errno, getpid(), socket_fd);
			break;
		default:
			printf("INFO: %s: %s (pid=%d socket=%d)\n",title, description, getpid(), socket_fd);
			break;
	}
	fflush(stdout);
}

http_verb request_type(char *request)
{
	if (strncmp(request, "GET ", 4)==0 || strncmp(request, "get ", 4)==0)
	{
		return HTTP_GET;
	}
	if (strncmp(request, "POST ", 4)==0 || strncmp(request, "post ", 4)==0)
	{
		return HTTP_POST;
	}
	
	return HTTP_NOT_SUPPORTED;
}

// this is a child web server process, we can safely exit on errors
void webhit(int socketfd, int hit, void (*responder_func)(char*, char*, int, http_verb))
{
	int j;
	http_verb type;
	long i, ret;
	static char buffer[BUFSIZE+1];	// static, filled with zeroes
	char *body;

	ret = read(socketfd, buffer, BUFSIZE); // read whole web request
	if (ret == 0 || ret == -1)
	{
		// cannot read request, so we'll stop
		forbidden_403(socketfd, "failed to read http request");
		exit(3);
	}
	if (ret > 0 && ret < BUFSIZE)
	{
		buffer[ret] = 0; // null terminate after chars
	}
	else
	{
		buffer[0] = 0;
	}
	for (i=0; i<ret; i++)
	{
		// replace CF and LF with asterisks
		if(buffer[i] == '\r' || buffer[i] == '\n')
		{
			buffer[i]='*';
		}
	}
	logger_function(LOG, "request", buffer, hit);
	
	if (type = request_type(buffer), type == HTTP_NOT_SUPPORTED)
	{
		forbidden_403(socketfd, "Only simple GET and POST operations are supported");
		exit(3);
	}
	
	// get a pointer to the request body (or NULL if it's not there)
	body = strstr(buffer, "****") + 4;
	
	// the request will be "GET URL " or "POST URL " followed by other details
	// we will terminate after the second space, to ignore everything else
	for (i = (type==HTTP_GET) ? 4 : 5; i<BUFSIZE; i++)
	{
		if(buffer[i] == ' ')
		{
			buffer[i] = 0; // second space, terminate string here
			break;
		}
	}

	for (j=0; j<i-1; j++)
	{
		// check for parent directory use
		if(buffer[j] == '.' && buffer[j+1] == '.')
		{
			forbidden_403(socketfd, "Parent paths (..) are not supported");
			exit(3);
		}
	}
	
	// call the "responder function" which has been provided to do the rest
	responder_func((type==HTTP_GET) ? &buffer[5] : &buffer[6], body, socketfd, type);
	exit(1);
}

int dwebserver(int port,
    void (*responder_func)(char*, char*, int, http_verb),
    void (*logger_func)(int, char*, char*, int) )
{
	int pid, listenfd, socketfd, hit;
	socklen_t length;
    // static = initialised to zeros
	static struct sockaddr_in cli_addr;
	static struct sockaddr_in serv_addr;
    
    if (logger_func != NULL)
    {
        logger_function = logger_func;
    }
    else
    {
        logger_function = &default_logger;
    }
    
	if (port <= 0 || port > 60000)
	{
		logger_function(ERROR, "Invalid port number (try 1 - 60000)", "", 0);
		exit(3);
	}
	
     // ignore child death and terminal hangups
#ifndef SIGCLD
	signal(SIGCHLD, SIG_IGN);
#else
	signal(SIGCLD, SIG_IGN);
#endif
    signal(SIGHUP, SIG_IGN);
	
    if ((listenfd = socket(AF_INET, SOCK_STREAM,0)) < 0)
	{
		logger_function(ERROR, "system call", "socket", 0);
		exit(3);
	}
    
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
    
	if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) <0)
	{
		logger_function(ERROR, "system call", "bind", 0);
		exit(3);
	}
    
	if (listen(listenfd, 64) <0)
	{
		logger_function(ERROR, "system call", "listen", 0);
		exit(3);
	}

	for (hit=1; ; hit++)
	{
		length = sizeof(cli_addr);
		if ((socketfd = accept(listenfd, (struct sockaddr*)&cli_addr, &length)) < 0)
		{
			logger_function(ERROR, "system call", "accept", 0);
			exit(3);
		}
		if ((pid = fork()) < 0)
		{
			logger_function(ERROR, "system call", "fork", 0);
			exit(3);
		}
		else
		{
			if (pid == 0) 
			{ 	
				// child
				close(listenfd);
				webhit(socketfd, hit, responder_func); // never returns
			}
			else
			{
				// parent
				close(socketfd);
			}
		}
	}
}

// The same algorithm as found here:
// http://spskhokhar.blogspot.co.uk/2012/09/url-decode-http-query-string.html
void url_decode(char *s)
{
    int i, len = (int)strlen(s);
    char s_copy[len+1];
    char *ptr = s_copy;
    memset(s_copy, 0, sizeof(s_copy));

    for (i=0; i < len; i++)
    {
        if (s[i]=='+')
        {
            *ptr++ = ' ';
        }
        else if ( (s[i] != '%') || (!isdigit(s[i+1]) || !isdigit(s[i+2])) )
        {
            *ptr++ = s[i];
        }
		else
		{
			*ptr++ = ((s[i+1] - '0') << 4) | (s[i+2] - '0');
			i += 2;
		}
    }
    *ptr = 0;
    strcpy(s, s_copy);
}

// assumes a content type of "application/x-www-form-urlencoded" (the default type)
int get_form_values(char *body, char *names[], char *values[], int max_values)
{
	int t=0, i;
	char *token = strtok(body, "&");
	
    while(token != NULL && t < max_values)
    {
        names[t] = token;
		for (i=0; i<strlen(token); i++)
		{
			if (token[i]=='=')
			{
				token[i] = 0;
				values[t] = token+i+1;
				break;
			}
		}
		url_decode(names[t]);
		url_decode(values[t++]);
		token = strtok(NULL, "&");
    }
	return t;
}
