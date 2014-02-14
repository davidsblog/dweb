CC=gcc

dwebmake: dweb.c dwebsvr.c
	$(CC) -o dweb dweb.c dwebsvr.c

simple: simple.c dwebsvr.c
	$(CC) -o simple simple.c dwebsvr.c

.PHONY: clean

clean:
	rm -f *.o *.exe dweb simple

