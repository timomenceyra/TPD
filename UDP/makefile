# Makefile para TP D - UDP Stop & Wait File Transfer
# Grupo 14

# Compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I./include

# Directorios
SRC_DIR = src
INC_DIR = include
BIN_DIR = bin
TEST_DIR = test_files

# Archivos
UTILS = $(SRC_DIR)/utils.c
CLIENT = $(SRC_DIR)/client.c
SERVER = $(SRC_DIR)/server.c
HEADER = $(INC_DIR)/protocol.h

# Ejecutables
CLIENT_BIN = $(BIN_DIR)/client
SERVER_BIN = $(BIN_DIR)/server

# Regla principal: compila todo
all: directories $(CLIENT_BIN) $(SERVER_BIN)
	@echo ""
	@echo "✓ Compilación exitosa"
	@echo ""
	@echo "Ejecutables:"
	@echo "  $(CLIENT_BIN)"
	@echo "  $(SERVER_BIN)"
	@echo ""

# Crear directorios si no existen
directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(TEST_DIR)

# Compilar cliente
$(CLIENT_BIN): $(CLIENT) $(UTILS) $(HEADER)
	@echo "Compilando cliente..."
	$(CC) $(CFLAGS) $(CLIENT) $(UTILS) -o $(CLIENT_BIN)

# Compilar servidor
$(SERVER_BIN): $(SERVER) $(UTILS) $(HEADER)
	@echo "Compilando servidor..."
	$(CC) $(CFLAGS) $(SERVER) $(UTILS) -o $(SERVER_BIN)

# Limpiar binarios
clean:
	@echo "Limpiando..."
	rm -rf $(BIN_DIR)
	rm -f $(TEST_DIR)/*.received
	@echo "✓ Limpieza completa"

# Crear archivo de prueba de 20kB
test-file:
	@echo "Creando archivo de prueba de 20kB..."
	dd if=/dev/zero of=$(TEST_DIR)/archivo_20kB bs=1024 count=20 2>/dev/null
	@echo "✓ Archivo creado: $(TEST_DIR)/archivo_20kB"

# Verificar MD5
check-md5:
	@echo "MD5 de archivos recibidos:"
	@ls $(TEST_DIR)/*.received 2>/dev/null | xargs md5sum 2>/dev/null || echo "No hay archivos .received"

# Ayuda
help:
	@echo "Targets disponibles:"
	@echo "  make          - Compila cliente y servidor"
	@echo "  make clean    - Elimina binarios"
	@echo "  make test-file- Crea archivo de 20kB para pruebas"
	@echo "  make check-md5- Verifica MD5 de archivos recibidos"

.PHONY: all clean directories test-file check-md5 help