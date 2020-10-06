BIN = bin
CC = gcc
CFLAGS = -g -Wall `pkg-config --cflags glib-2.0`
LDFLAGS = -lpthread -lrt -lglib-2.0
INCLUDE = -I /usr/include/glib-2.0
OBJS = $(BIN)/logc_req_handler.o $(BIN)/logc_server.o $(BIN)/logc_utils.o ../bin/logc_buffer.o

all: clean build release

clean:
	rm -rf $(BIN)/*

build: logc_utils logc_req_handler logc_server

logc_utils: logc_utils.c
	$(CC) -c $^ $(CFLAGS) $(INCLUDE) -o $(BIN)/logc_utils.o

logc_req_handler: logc_req_handler.c
	$(CC) -c $^ $(CFLAGS) $(INCLUDE) -o $(BIN)/logc_req_handler.o 

logc_server: logc_server.c
	$(CC) -c $^ $(CFLAGS) $(INCLUDE) -o $(BIN)/logc_server.o

release: $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $(BIN)/logcserver