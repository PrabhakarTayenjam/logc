BIN = bin
CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread -lrt
COMMON = ../common/bin/logc_utils.o ../common/bin/logc_buffer.o
OBJS = $(BIN)/logc_req_handler.o $(BIN)/logc_server_utils.o $(BIN)/logc_server.o $(COMMON)

all: clean mkbin build release

clean:
	rm -rf $(BIN)/*

mkbin:
	mkdir -p $(BIN)

build: logc-server-utils logc-req-handler logc-server

logc-server-utils: logc_server_utils.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $^ -o $(BIN)/logc_server_utils.o

logc-req-handler: logc_req_handler.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $^ -o $(BIN)/logc_req_handler.o 

logc-server: logc_server.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $^ -o $(BIN)/logc_server.o

release: $(OBJS)
	$(CC) $^ -o $(BIN)/logcserver $(LDFLAGS)
