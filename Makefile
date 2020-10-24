SRC=src
COMMON = $(SRC)/common
LOGC_CLIENT = $(SRC)/logc-client
LOGC_SERVER = $(SRC)/logc-server

all: common logc-client logc-server

common:
	cd $(COMMON); make

logc-client:
	cd $(LOGC_CLIENT); make

logc-server:
	cd $(LOGC_SERVER); make