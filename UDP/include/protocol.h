#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>

/* ==================== CONSTANTES DEL PROTOCOLO ==================== */

// Puerto del servidor
#define SERVER_PORT 20252

// Tamaños
// CORREGIDO: 1470 = 1500 (Ethernet MTU) - 20 (IP header) - 8 (UDP header) - 2 (Type + SeqNum)
#define MAX_DATA_SIZE 1470          // Tamaño máximo de datos según aclaración del profesor
#define MAX_PDU_SIZE 1472           // Type(1) + SeqNum(1) + Data(1470)
#define MAX_CREDENTIALS_SIZE 256
#define MAX_CREDENTIALS_LEN 10      // Máximo 10 caracteres según aclaración del profesor
#define MIN_FILENAME_LEN 4
#define MAX_FILENAME_LEN 10

// Timeouts y reintentos
// CORREGIDO: 3 segundos según aclaración del profesor (para contemplar RTT lunar de 2.56s)
#define TIMEOUT_MS 3000             // 3 segundos
#define MAX_RETRIES 5

// Tipos de PDU
#define TYPE_HELLO 1
#define TYPE_WRQ 2
#define TYPE_DATA 3
#define TYPE_ACK 4
#define TYPE_FIN 5

// Fases del protocolo
#define PHASE_NONE 0
#define PHASE_AUTHENTICATED 1
#define PHASE_WRQ_OK 2
#define PHASE_TRANSFERRING 3
#define PHASE_COMPLETED 4

// Número máximo de clientes concurrentes (servidor)
#define MAX_CLIENTS 10

/* ==================== ESTRUCTURAS ==================== */

/**
 * Estructura de la PDU (Protocol Data Unit)
 * +----------------+-----------------+-----------------------+
 * | Type (1 byte)  | SeqNum (1 byte) | Data (variable)      |
 * +----------------+-----------------+-----------------------+
 */
typedef struct {
    uint8_t type;                   // Tipo de PDU (HELLO, WRQ, DATA, ACK, FIN)
    uint8_t seq_num;                // Número de secuencia (0 o 1)
    uint8_t data[MAX_DATA_SIZE];    // Datos variables
} PDU;

/**
 * Estado del cliente
 */
typedef struct {
    int sockfd;                     // Socket descriptor
    struct sockaddr_in server_addr; // Dirección del servidor
    uint8_t current_seq;            // Número de secuencia actual (0 o 1)
    char credentials[MAX_CREDENTIALS_SIZE]; // Credenciales de autenticación
    char filename[MAX_FILENAME_LEN + 1];    // Nombre del archivo (+ null terminator)
} ClientState;

/**
 * Sesión de un cliente en el servidor
 */
typedef struct {
    struct sockaddr_in addr;        // Dirección del cliente
    int active;                     // 1 si está activa, 0 si está libre
    int phase;                      // Fase actual del protocolo
    uint8_t expected_seq;           // Próximo seq_num esperado
    FILE *file;                     // Archivo siendo escrito
    char filename[MAX_FILENAME_LEN + 1]; // Nombre del archivo
    time_t last_activity;           // Timestamp de última actividad
} ClientSession;

/**
 * Estado del servidor
 */
typedef struct {
    int sockfd;                     // Socket descriptor
    ClientSession clients[MAX_CLIENTS]; // Array de sesiones de clientes
    char credentials[MAX_CREDENTIALS_SIZE]; // Credenciales válidas
} ServerState;

/* ==================== FUNCIONES AUXILIARES ==================== */

/**
 * Convierte el tipo de PDU a string para logging
 */
const char* pdu_type_to_string(uint8_t type);

/**
 * Convierte la fase a string para logging
 */
const char* phase_to_string(int phase);

/**
 * Imprime una PDU (para debugging)
 */
void print_pdu(PDU *pdu, int data_len, const char *prefix);

/**
 * Imprime dirección IP:Puerto
 */
void print_address(struct sockaddr_in *addr);

/**
 * Crea y configura un socket UDP
 */
int create_udp_socket(void);

/**
 * Construye una PDU con los parámetros dados
 */
void build_pdu(PDU *pdu, uint8_t type, uint8_t seq_num, 
               const void *data, int data_len);

/**
 * Envía una PDU por UDP
 */
int send_pdu(int sockfd, struct sockaddr_in *dest_addr, 
             PDU *pdu, int data_len);

/**
 * Recibe una PDU con timeout usando select()
 */
int recv_pdu_with_timeout(int sockfd, PDU *pdu, struct sockaddr_in *src_addr,
                          int timeout_ms);

/**
 * Valida que el filename tenga entre 4 y 10 caracteres ASCII
 */
int validate_filename(const char *filename);

/**
 * Valida que las credenciales sean válidas (máx 10 caracteres ASCII)
 */
int validate_credentials(const char *credentials);

/**
 * Obtiene el tamaño de un archivo
 */
long get_file_size(const char *filepath);

#endif // PROTOCOL_H