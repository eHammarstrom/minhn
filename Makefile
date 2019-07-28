CC     = gcc
CFLAGS = -Wall
SRC    = $(wildcard *.c)
LIBS   = libcurl jansson

minhn: $(SRC)
	$(CC) *.c -o $@ $(CFLAGS) `pkg-config --libs libcurl jansson`

check-macro:
	$(CC) -E main.c

