all: sclient sserver

CFLAGS = -Wall -g -fsanitize=address # -Werror

sclient: sclient.c macro.h
	gcc ${CFLAGS} -o sclient sclient.c

sserver: sserver.c macro.h
	gcc ${CFLAGS} -o sserver sserver.c

clean:
	rm sserver sclient
