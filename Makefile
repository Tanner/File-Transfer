CC = gcc
CFLAGS = -Wall -Werror -ansi -pedantic -std=c99
LIBS = -lm

RELEASE_FLAGS = -O2
DEBUG_FLAGS = -g -DDEBUG

CLIENT_OBJ = client.o dropper.o arq.o utilities.o message.o
CLIENT_TARGET = kftclient

SERVER_OBJ = server.o dropper.o arq.o utilities.o message.o
SERVER_TARGET = kftserver

all: CFLAGS += $(RELEASE_FLAGS)
all: $(CLIENT_TARGET) $(SERVER_TARGET)

debug-client: CFLAGS += $(DEBUG_FLAGS)
debug-client: $(CLIENT_TARGET)

debug-server: CFLAGS += $(DEBUG_FLAGS)
debug-server: $(SERVER_TARGET)

$(CLIENT_TARGET) : $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) $(CLIENT_OBJ) $(LIBS)

$(SERVER_TARGET) : $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $(SERVER_TARGET) $(SERVER_OBJ) $(LIBS)

%.o : %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean: clean-client clean-server

clean-client:
	rm -f $(CLIENT_OBJ) $(CLIENT_TARGET)

clean-server:
	rm -f $(SERVER_OBJ) $(SERVER_TARGET)
