dweb
====

A lightweight webserver for C programs, which should work on Linux, Unix, Mac OS, etc.  
I'm planning to use it as a very small WebAPI, most likely hosted on a Raspberry Pi.

The idea is to be able to serve content from simple C programs like this:
```
void test_response(char*, char*, int, http_verb);

int main(int argc, char **argv)
{
	if (argc != 2 || !strcmp(argv[1], "-?"))
	{
		printf("hint: dweb [port number]\n");
		exit(0);
	}
	dwebserver(atoi(argv[1]), &test_response);
}

void test_response(char *request, char *body, int socketfd, http_verb type)
{
	ok_200(socketfd,
		"<html><head>\n<title>Test Page</title>\n"
		"</head><body>\n<h1>Testing...</h1>\nThis is a test response.\n</body>"
		"</html>", request);
}
```
I owe a lot to Nweb: http://www.ibm.com/developerworks/systems/library/es-nweb/index.html 
which was my starting point.  But I am adding support for things like HTTP POST and 
serving dynamic content.  Unlike Nweb, this code does not run as a daemon.


License
=======

The MIT License (MIT)

Copyright (c) 2014 David's Blog - www.codehosting.net

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
