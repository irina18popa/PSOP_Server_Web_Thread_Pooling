CC := gcc
CFLAGS := -Wall

all: miniServerWeb


miniServerWeb: miniServerWeb.o
	$(CC) $^ -o $@

.PHONY: clean all

clean:
	rm *.o miniServerWeb