SRC=src
COMMON = $(SRC)/common
LOGC_CLIENT = $(SRC)/logc-client
LOGC_SERVER = $(SRC)/logc-server

all: common logc-client logc-server

common:
	cd $(COMMON); mkdir -p bin; make

logc-client:
	cd $(LOGC_CLIENT); mkdir -p bin; make

logc-server:
	cd $(LOGC_SERVER); mkdir -p bin; make
