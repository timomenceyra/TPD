# Makefile para compilar la parte TCP ubicada en src/

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2

SRC_DIR := src
BIN_DIR := .

CLIENT_SRC := $(SRC_DIR)/client_tcp.c
SERVER_SRC := $(SRC_DIR)/server_tcp.c

CLIENT_BIN := $(BIN_DIR)/client_tcp
SERVER_BIN := $(BIN_DIR)/server_tcp

.PHONY: all clean run-server run-client dirs

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_BIN)

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN) -d 50 -N 3

clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN) tcp_delays.csv