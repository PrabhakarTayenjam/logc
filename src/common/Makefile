BIN = bin
CC = gcc
CFLAGS = -g -Wall -DLOGC_DEBUG

all: clean build

clean:
	rm -rf $(BIN)/*

build: logc_utils logc_buffer

logc_utils: logc_utils.c
	$(CC) -c $^ $(CFLAGS) -o $(BIN)/logc_utils.o

logc_buffer: logc_buffer.c
	$(CC) -c $^ $(CFLAGS) -o $(BIN)/logc_buffer.o