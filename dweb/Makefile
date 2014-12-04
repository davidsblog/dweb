CC=gcc

dwebmake: dweb.c dwebsvr.c
	$(CC) -o dweb dweb.c dwebsvr.c -pthread -std=gnu99

openwrt: dweb.c dwebsvr.c
	export STAGING_DIR=$$PWD; \
`find /home -name mipsel-openwrt-linux-gcc -print -quit -print 2>/dev/null` -o dweb -Os -s dweb.c dwebsvr.c -pthread

simple: simple.c dwebsvr.c
	$(CC) -o simple simple.c dwebsvr.c -pthread -std=gnu99

.PHONY: clean

clean:
	rm -f *.o *.exe dweb simple
