#ifndef _DWEBSVR_H

#define _DWEBSVR_H

#define BUFSIZE  8096

#define ERROR    42
#define LOG      43

#define HTTP_NOT_SUPPORTED  100
#define HTTP_GET  101
#define HTTP_POST  102

#define http_verb int

void write_html(int socket_fd, char *head, char *html);
void forbidden_403(int socket_fd, char *info);
void notfound_404(int socket_fd, char *info);
void ok_200(int socket_fd, char *html, char *path);
void logger(int type, char *s1, char *s2, int socket_fd);
void webhit(int socketfd, int hit, void (*responder_func)(char*, char*, int, http_verb));
int dwebserver(int port, void (*responder_func)(char*, char*, int, http_verb));
int get_form_values(char *body, char *names[], char *values[], int max_values);
void url_decode(char *s);

#endif
