#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "dwebsvr.h"

void simple_response(char*, char*, int, http_verb);

int main(int argc, char **argv)
{
	if (argc != 2 || !strcmp(argv[1], "-?"))
	{
		printf("hint: simple [port number]\n");
		exit(0);
	}
	dwebserver(atoi(argv[1]), &simple_response, NULL);
}

void simple_response(char *path, char *body, int socketfd, http_verb type)
{
	ok_200(socketfd,
		"<html><head><title>Test Page</title></head>"
		"<body><h1>Testing...</h1>This is a test response.</body>"
		"</html>", path);
}
