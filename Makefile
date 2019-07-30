CC     = gcc
DEBUG  = -g -fsanitize=address -fno-omit-frame-pointer
CFLAGS = -Wall
SRC    = $(wildcard *.c)
LIBS   = libcurl jansson

minhn: $(SRC)
	$(CC) *.c -o $@ $(CFLAGS) `pkg-config --libs $(LIBS)`

check-macro:
	$(CC) -E main.c

install:
	mkdir -p /usr/bin
	mv ./minhn /usr/bin

clean:
	rm -f minhn

