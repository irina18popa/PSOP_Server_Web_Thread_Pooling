CC := gcc
CFLAGS := -Wall

all: server

server: server.o
	$(CC) $^ -o $@ -lpthread

.PHONY: clean all

clean:
	rm *.o server