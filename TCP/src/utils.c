#include "../include/protocol.h"

/* ==================== FUNCIONES DE UTILIDAD ==================== */

/**
 * Convierte el tipo de PDU a string para logging
 */
const char* pdu_type_to_string(uint8_t type) {
    switch(type) {
        case TYPE_HELLO: return "HELLO";
        case TYPE_WRQ:   return "WRQ";
        case TYPE_DATA:  return "DATA";
        case TYPE_ACK:   return "ACK";
        case TYPE_FIN:   return "FIN";
        default:         return "UNKNOWN";
    }
}

/**
 * Convierte la fase a string para logging
 */
const char* phase_to_string(int phase) {
    switch(phase) {
        case PHASE_NONE:          return "NONE";
        case PHASE_AUTHENTICATED: return "AUTHENTICATED";
        case PHASE_WRQ_OK:        return "WRQ_OK";
        case PHASE_TRANSFERRING:  return "TRANSFERRING";
        case PHASE_COMPLETED:     return "COMPLETED";
        default:                  return "UNKNOWN";
    }
}

/**
 * Imprime una PDU (para debugging)
 */
void print_pdu(PDU *pdu, int data_len, const char *prefix) {
    printf("%s PDU [Type=%s(%d), SeqNum=%d, DataLen=%d]\n",
           prefix,
           pdu_type_to_string(pdu->type),
           pdu->type,
           pdu->seq_num,
           data_len);
}

/**
 * Imprime dirección IP:Puerto
 */
void print_address(struct sockaddr_in *addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
    printf("%s:%d", ip, ntohs(addr->sin_port));
}

/**
 * Crea y configura un socket UDP
 */
int create_udp_socket(void) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creando socket");
        return -1;
    }
    return sockfd;
}

/**
 * Construye una PDU con los parámetros dados
 */
void build_pdu(PDU *pdu, uint8_t type, uint8_t seq_num, 
               const void *data, int data_len) {
    memset(pdu, 0, sizeof(PDU));
    pdu->type = type;
    pdu->seq_num = seq_num;
    
    if (data && data_len > 0) {
        if (data_len > MAX_DATA_SIZE) {
            data_len = MAX_DATA_SIZE;
        }
        memcpy(pdu->data, data, data_len);
    }
}

/**
 * Envía una PDU por UDP
 * Retorna: número de bytes enviados, o -1 en error
 */
int send_pdu(int sockfd, struct sockaddr_in *dest_addr, 
             PDU *pdu, int data_len) {
    // Calcular tamaño total: type(1) + seq_num(1) + data
    int total_size = 2 + data_len;
    
    int sent = sendto(sockfd, pdu, total_size, 0,
                     (struct sockaddr*)dest_addr, sizeof(*dest_addr));
    
    if (sent < 0) {
        perror("Error en sendto");
        return -1;
    }
    
    return sent;
}

/**
 * Recibe una PDU con timeout usando select()
 * Retorna: número de bytes recibidos, 0 si timeout, -1 si error
 */
int recv_pdu_with_timeout(int sockfd, PDU *pdu, struct sockaddr_in *src_addr,
                          int timeout_ms) {
    fd_set readfds;
    struct timeval tv;
    
    // Configurar timeout
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    // Configurar file descriptor set
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    
    // Esperar datos o timeout
    int ready = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    
    if (ready < 0) {
        perror("Error en select");
        return -1;
    }
    
    if (ready == 0) {
        // Timeout
        return 0;
    }
    
    // Hay datos disponibles
    socklen_t addr_len = sizeof(*src_addr);
    int recv_len = recvfrom(sockfd, pdu, sizeof(PDU), 0,
                           (struct sockaddr*)src_addr, &addr_len);
    
    if (recv_len < 0) {
        perror("Error en recvfrom");
        return -1;
    }
    
    return recv_len;
}

/**
 * Valida que el filename tenga entre 4 y 10 caracteres ASCII
 */
int validate_filename(const char *filename) {
    if (!filename) return 0;
    
    int len = strlen(filename);
    
    // Verificar longitud
    if (len < MIN_FILENAME_LEN || len > MAX_FILENAME_LEN) {
        printf("Error: filename debe tener entre %d y %d caracteres (tiene %d)\n",
               MIN_FILENAME_LEN, MAX_FILENAME_LEN, len);
        return 0;
    }
    
    // Verificar que solo tenga caracteres ASCII imprimibles
    for (int i = 0; i < len; i++) {
        if (filename[i] < 32 || filename[i] > 126) {
            printf("Error: filename contiene caracteres no-ASCII\n");
            return 0;
        }
    }
    
    return 1;
}

/**
 * Obtiene el tamaño de un archivo
 */
long get_file_size(const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return -1;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    
    return size;
}