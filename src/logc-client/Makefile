BIN = bin
CC = gcc
CFLAGS = -g -Wall -DLOGC_DEBUG
LDFLAGS = -lrt
COMM = ../common
OBJS = $(COMM)/$(BIN)/logc_utils.o $(COMM)/$(BIN)/logc_buffer.o $(BIN)/logc.o

all: clean build release

clean:
	rm -rf $(BIN)/*

build: logc

logc: logc.c
	$(CC) -c $^ $(CFLAGS) -o $(BIN)/logc.o

release: $(OBJS)
	ar rcs -o $(BIN)/logc.a $^