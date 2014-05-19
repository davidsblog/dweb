#ifndef _DWEBSVR_H

#define _DWEBSVR_H

#define ERROR    42
#define LOG      43

#define HTTP_NOT_SUPPORTED  100
#define HTTP_GET  101
#define HTTP_POST  102

#define http_verb int
#define log_type int

struct http_header
{
    char name[50];
    char value[255];
};

int dwebserver(int port,
    void (*responder_func)(char*, char*, int, http_verb),
    void (*logger_func)(log_type, char*, char*, int));

struct http_header get_header(const char *name, char *request);

void write_header(int socket_fd, char *head, long content_len);
void write_html(int socket_fd, char *head, char *html);
void forbidden_403(int socket_fd, char *info);
void notfound_404(int socket_fd, char *info);
void ok_200(int socket_fd, char *html, char *path);
void logger(log_type type, char *s1, char *s2, int socket_fd);
void webhit(int socketfd, int hit, void (*responder_func)(char*, char*, int, http_verb));

int form_value_count();
char* form_value(int i);
char* form_name(int i);

void url_decode(char *s);

/* ---------- Memory allocation helper stuff ---------- */

typedef struct
{
    void *ptr;		// the pointer to the data
    int alloc_bytes;// the number of bytes allocated
    int used_bytes;	// the number of bytes used
    int elem_bytes;	// the number of bytes per element
    int chunk_size;	// the number of elements to increase space by
} blk;

typedef blk STRING;

STRING* new_string(int increments);
void string_add(STRING *s, char *char_array);
char* string_chars(STRING *s);
void string_free(STRING *s);

/* ---------- End of memory allocation helper stuff ---------- */

typedef struct
{
    char *name, *value;
    char *data;
} FORM_VALUE;

#endif