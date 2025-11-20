# Makefile para UDP File Transfer

CC = gcc
CFLAGS = -Wall -Wextra -g -I./include
LDFLAGS = 

# Directorios
SRC_DIR = src
INCLUDE_DIR = include
BIN_DIR = bin

# Archivos fuente
CLIENT_SRC = $(SRC_DIR)/client.c $(SRC_DIR)/utils.c
SERVER_SRC = $(SRC_DIR)/server.c $(SRC_DIR)/utils.c

# Ejecutables
CLIENT_BIN = $(BIN_DIR)/client
SERVER_BIN = $(BIN_DIR)/server

# Regla por defecto
all: directories $(CLIENT_BIN) $(SERVER_BIN)

# Crear directorios si no existen
directories:
	@mkdir -p $(BIN_DIR)

# Compilar cliente
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Cliente compilado: $(CLIENT_BIN)"

# Compilar servidor
$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Servidor compilado: $(SERVER_BIN)"

# Solo cliente
client: directories $(CLIENT_BIN)

# Solo servidor
server: directories $(SERVER_BIN)

# Limpiar archivos compilados
clean:
	rm -rf $(BIN_DIR)
	@echo "Archivos compilados eliminados"

# Limpiar todo (incluyendo archivos de prueba)
distclean: clean
	rm -rf test_files/*.received
	@echo "Limpieza completa"

# Ayuda
help:
	@echo "Makefile para UDP File Transfer"
	@echo ""
	@echo "Uso:"
	@echo "  make          - Compilar cliente y servidor"
	@echo "  make client   - Compilar solo cliente"
	@echo "  make server   - Compilar solo servidor"
	@echo "  make clean    - Eliminar archivos compilados"
	@echo "  make distclean- Limpieza completa"
	@echo "  make help     - Mostrar esta ayuda"

.PHONY: all directories client server clean distclean help
