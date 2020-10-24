BIN = bin
CC = cc
CFLAGS = -g -Wall `pkg-config --cflags glib-2.0` -DLOGC_DEBUG
LDFLAGS = -lpthread -lrt -lglib-2.0
INCLUDE = -I /usr/include/glib-2.0
COMMON = ../common/bin/logc_utils.o ../common/bin/logc_buffer.o
OBJS = $(BIN)/logc_req_handler.o $(BIN)/logc_server_utils.o $(BIN)/logc_server.o $(COMMON)

all: clean build release

clean:
	rm -rf $(BIN)/*

build: logc-server-utils logc-req-handler logc-server

logc-server-utils: logc_server_utils.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $^ -o $(BIN)/logc_server_utils.o

logc-req-handler: logc_req_handler.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $^ -o $(BIN)/logc_req_handler.o 

logc-server: logc_server.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $^ -o $(BIN)/logc_server.o

release: $(OBJS)
	$(CC) $^ -o $(BIN)/logcserver $(LDFLAGS)