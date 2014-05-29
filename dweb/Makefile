CC=gcc

dwebmake: dweb.c dwebsvr.c
	$(CC) -o dweb dweb.c dwebsvr.c -pthread

simple: simple.c dwebsvr.c
	$(CC) -o simple simple.c dwebsvr.c -pthread

.PHONY: clean

clean:
	rm -f *.o *.exe dweb simple

