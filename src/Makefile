BIN = bin
CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lrt
OBJS = $(BIN)/logc_buffer.o $(BIN)/logc.o

all: clean build release

clean:
	rm -rf $(BIN)/*

build: logc_buffer logc

logc_buffer: logc_buffer.c
	$(CC) -c $^ $(CFLAGS) -o $(BIN)/logc_buffer.o

logc: logc.c
	$(CC) -c $^ $(CFLAGS) -o $(BIN)/logc.o

release: $(OBJS)
	ar rcs -o $(BIN)/logc.a $^