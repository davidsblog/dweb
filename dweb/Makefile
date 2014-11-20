CC=gcc

dwebmake: dweb.c dwebsvr.c
	$(CC) -o dweb dweb.c dwebsvr.c -pthread -std=gnu99

simple: simple.c dwebsvr.c
	$(CC) -o simple simple.c dwebsvr.c -pthread -std=gnu99

.PHONY: clean

clean:
	rm -f *.o *.exe dweb simple

