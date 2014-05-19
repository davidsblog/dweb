#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "dwebsvr.h"

// uncomment the following line for a single-threaded web server, which
// might be useful for debugging or embedded use
//#define SINGLE_THREADED

STRING *buffer = NULL;
FORM_VALUE *form_values = NULL;
int form_value_counter=0;

void (*logger_function)(log_type, char*, char*, int);

#define FORM_VALUE_BLOCK 10

// assumes a content type of "application/x-www-form-urlencoded" (the default type)
void get_form_values(char *body)
{
    int t=0, i, alloc = FORM_VALUE_BLOCK;
	char *tmp, *token = strtok(body, "&");
    
    form_values = malloc(alloc * sizeof(FORM_VALUE));
    memset(form_values, 0, alloc * sizeof(FORM_VALUE));
    
    while(token != NULL)
    {
        tmp = malloc(strlen(token)+1);
        strcpy(tmp, token);
        url_decode(tmp);
        
		for (i=0; i<strlen(tmp); i++)
			if (tmp[i]=='=') break;
        
        if (alloc<=t)
        {
            int newsize = alloc+FORM_VALUE_BLOCK;
            form_values = realloc(form_values, newsize * sizeof(FORM_VALUE));
            memset(form_values+alloc, 0, FORM_VALUE_BLOCK * sizeof(FORM_VALUE));
            alloc = newsize;
        }
        
        form_values[t].data = malloc(strlen(tmp)+1);
        strcpy(form_values[t].data, tmp);
        form_values[t].name = form_values[t].data;
        form_values[t].value = form_values[t].data+1+i;
		form_values[t++].data[i] = 0;
        
		token = strtok(NULL, "&");
        free (tmp);
    }
    form_value_counter = t;
}

void clear_form_values()
{
    if (form_values == NULL) return;
    for (form_value_counter--; form_value_counter>=0; form_value_counter--)
    {
        free(form_values[form_value_counter].data);
    }
    free(form_values);
}

void finish_hit(int socket_fd, int exit_code)
{
    close(socket_fd);
    if (buffer)
    {
        string_free(buffer);
    }
    clear_form_values();
    
#ifndef SINGLE_THREADED
    exit(exit_code);
#endif
}

// writes the specified header and sets the Content-Length
void write_header(int socket_fd, char *head, long content_len)
{
    STRING *header = new_string(255);
    string_add(header, head);
    string_add(header, "\nContent-Length: ");
    char cl[10]; // 100Mb = 104,857,600 bytes
    snprintf(cl, 10, "%ld", content_len);
    string_add(header, cl);
    string_add(header, "\r\n\r\n");
    write(socket_fd, string_chars(header), header->used_bytes-1);
    string_free(header);
}

void write_html(int socket_fd, char *head, char *html)
{
    write_header(socket_fd, head, strlen(html));
	write(socket_fd, html, strlen(html));
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
	write_html(socket_fd, "HTTP/1.1 200 OK\nServer: dweb\nCache-Control: no-cache\nPragma: no-cache\nContent-Type: text/html", html);
	
	logger_function(LOG, "200 OK", path, socket_fd);
}

void default_logger(log_type type, char *title, char *description, int socket_fd)
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

struct http_header get_header(const char *name, char *request)
{
    struct http_header retval;
    int x=0;
    char *ptr = strstr(request, name);
    strncpy(retval.name, name, sizeof(retval.name)-1);
    retval.name[sizeof(retval.name)-1] = 0;
    
    if (ptr == NULL)
    {
        retval.value[0] = 0;
        return retval;
    }
    
    while (*ptr++!=':') ;
    while (isblank(*++ptr)) ;
    while (x<sizeof(retval.value)-1 && *ptr!='\r' && *ptr!='\n')
    {
        retval.value[x++] = *ptr++;
    }
    retval.value[x]=0;
    return retval;
}

long get_body_start(char *request)
{
    // return the starting index of the request body
    // so ... just find the end of the HTTP headers
    char *ptr = strstr(request, "\r\n\r\n");
    return (ptr==NULL) ? -1 : (ptr+4) - request;
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

// We will read data from the socket in chunks of this size
#define READ_BUF_LEN 255

// this is a child web server process, we can safely exit on errors
void webhit(int socketfd, int hit, void (*responder_func)(char*, char*, int, http_verb))
{
	int j;
	http_verb type;
	long i, body_size = 0, body_expected, request_size = 0, body_start;
    char tmp_buf[READ_BUF_LEN+1];
	char *body;
    struct http_header content_length;
    buffer = new_string(READ_BUF_LEN);
    
    // we need to read the HTTP headers first...
    // so loop until we receive "\r\n\r\n"
    while (get_body_start(string_chars(buffer))<0)
    {
        memset(tmp_buf, 0, READ_BUF_LEN+1);
        request_size += read(socketfd, tmp_buf, READ_BUF_LEN);
        string_add(buffer, tmp_buf);
    }
    
    content_length = get_header("Content-Length", string_chars(buffer));
    body_expected = atoi(content_length.value);
    body_start = get_body_start(string_chars(buffer));
    if (body_start>=0) body_size = request_size - body_start;
    
    // safari seems to send the headers, and then the body slightly later
    while (body_size < body_expected)
    {
        memset(tmp_buf, 0, READ_BUF_LEN+1);
        i = read(socketfd, tmp_buf, READ_BUF_LEN);
        if (i>0)
        {
            request_size += i;
            string_add(buffer, tmp_buf);
        }
        body_size = request_size - body_start;
    }
    
    if (request_size <= 0)
	{
		// cannot read request, so we'll stop
		forbidden_403(socketfd, "failed to read http request");
        finish_hit(socketfd, 3);
	}
    
    logger_function(LOG, "request", string_chars(buffer), hit);
    
	for (i=0; i<request_size; i++)
	{
		// replace CF and LF with asterisks
		if(string_chars(buffer)[i] == '\r' || string_chars(buffer)[i] == '\n')
		{
			string_chars(buffer)[i]='*';
		}
	}
	
	if (type = request_type(string_chars(buffer)), type == HTTP_NOT_SUPPORTED)
	{
		forbidden_403(socketfd, "Only simple GET and POST operations are supported");
        finish_hit(socketfd, 3);
	}
	
	// get a pointer to the request body (or NULL if it's not there)
	body = strstr(string_chars(buffer), "****") + 4;
	
	// the request will be "GET URL " or "POST URL " followed by other details
	// we will terminate after the second space, to ignore everything else
	for (i = (type==HTTP_GET) ? 4 : 5; i < buffer->used_bytes; i++)
	{
		if(string_chars(buffer)[i] == ' ')
		{
			string_chars(buffer)[i] = 0; // second space, terminate string here
			break;
		}
	}

	for (j=0; j<i-1; j++)
	{
		// check for parent directory use
		if (string_chars(buffer)[j] == '.' && string_chars(buffer)[j+1] == '.')
		{
			forbidden_403(socketfd, "Parent paths (..) are not supported");
            finish_hit(socketfd, 3);
		}
	}
    
    get_form_values(body);
	
	// call the "responder function" which has been provided to do the rest
	responder_func((type==HTTP_GET) ? string_chars(buffer)+5 : string_chars(buffer)+6, body, socketfd, type);
    finish_hit(socketfd, 1);
}

int dwebserver(int port,
    void (*responder_func)(char*, char*, int, http_verb),  // pointer to responder function
    void (*logger_func)(log_type, char*, char*, int) )     // pointer to logger (or NULL)
{
#ifndef SINGLE_THREADED
    int pid;
#endif
    int listenfd, socketfd, hit;
	socklen_t length;
    // *static* means the compiler will make them initialised to zeros
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
        
#ifdef SINGLE_THREADED
        webhit(socketfd, hit, responder_func);
#else
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
                close(socketfd);
            }
		}
#endif

	}
    
#ifdef SINGLE_THREADED
    close(listenfd);
#endif
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

int form_value_count()
{
    return form_value_counter;
}

char* form_value(int i)
{
    if (i>=form_value_counter) return NULL;
    return form_values[i].value;
}

char* form_name(int i)
{
    if (i>=form_value_counter) return NULL;
    return form_values[i].name;
}

/* ---------- Memory allocation helpers ---------- */

void bcreate(blk *b, int elem_size, int inc)
{
    b->elem_bytes = elem_size;
    b->chunk_size = inc;
    b->ptr = calloc(b->chunk_size, b->elem_bytes);
    b->alloc_bytes = b->chunk_size * b->elem_bytes;
    b->used_bytes = 0;
}

void badd(blk *b, void *data, int len)
{
    if (b->alloc_bytes - b->used_bytes < len)
    {
        while (b->alloc_bytes - b->used_bytes < len)
        {
			b->alloc_bytes+= (b->chunk_size * b->elem_bytes);
        }
        b->ptr=realloc(b->ptr, b->alloc_bytes);
    }
    memcpy(b->ptr + b->used_bytes, data, len);
    b->used_bytes += len;
    memset(b->ptr + b->used_bytes, 0, b->alloc_bytes - b->used_bytes);
}

void bfree(blk *b)
{
    free(b->ptr);
    b->used_bytes = 0;
    b->alloc_bytes = 0;
}

STRING* new_string(int increments)
{
	STRING *s = malloc(sizeof(STRING));
	bcreate(s, 1, increments);
	badd(s, "\0", 1);
	return s;
}

void string_add(STRING *s, char *char_array)
{
    s->used_bytes--;
    badd(s, char_array, (int)strlen(char_array)+1);
}

char* string_chars(STRING *s)
{
	return s->ptr;
}

void string_free(STRING *s)
{
	bfree(s);
	free(s);
}

/* ---------- End of memory allocation helpers ---------- */
