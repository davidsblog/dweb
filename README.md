dweb
====

A lightweight webserver for C programs, which should work on Linux, Unix, Mac OS, etc.  
I'm planning to use it as a very small Web API, most likely hosted on a Raspberry Pi.

The idea is to be able to serve dynamic web content from simple C programs, without
having to add too much code.  So the trivial example looks like this:
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

void test_response(char *path, char *body, int socketfd, http_verb type)
{
	ok_200(socketfd,
		"<html><head><title>Test Page</title></head>"
		"<body><h1>Testing...</h1>This is a test response.</body>"
		"</html>", request);
}
```
I owe a lot to nweb: http://www.ibm.com/developerworks/systems/library/es-nweb/index.html 
which was my starting point.  But I am adding support for things like HTTP POST and 
serving up dynamic content.  Unlike nweb, this code does not run as a daemon, and logging
goes to the console.


Building
========

To build the example program, which uses jQuery, allows HTML Form values to be posted back, 
and gives dynamic responses, just type ```make``` ... you can then run ```dweb``` from the 
command line (you need to specify the port number as the first parameter).

To build the trivial example (as shown above) you can type ```make simple``` and then run 
```simple``` from the command line.

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
