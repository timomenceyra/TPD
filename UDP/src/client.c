#include "../include/protocol.h"

// Funciones del cliente UDP

// Inicializa el estado del cliente
int init_client(ClientState *state, const char *server_ip, 
                const char *credentials, const char *filename) {
    // Validar credenciales ANTES de inicializar
    if (!validate_credentials(credentials)) {
        return -1;
    }
    
    // Validar filename ANTES de inicializar
    if (!validate_filename(filename)) {
        return -1;
    }
    
    // Crear socket
    state->sockfd = create_udp_socket();
    if (state->sockfd < 0) {
        return -1;
    }
    
    // Configurar dirección del servidor
    memset(&state->server_addr, 0, sizeof(state->server_addr));
    state->server_addr.sin_family = AF_INET;
    state->server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, server_ip, &state->server_addr.sin_addr) <= 0) {
        perror("Error en dirección IP del servidor");
        close(state->sockfd);
        return -1;
    }
    
    // Copiar credenciales y filename
    strncpy(state->credentials, credentials, MAX_CREDENTIALS_SIZE - 1);
    strncpy(state->filename, filename, MAX_FILENAME_LEN);
    state->filename[MAX_FILENAME_LEN] = '\0';
    
    // Inicializar seq_num
    state->current_seq = 0;
    
    printf("Cliente inicializado\n");
    printf("  Servidor: %s:%d\n", server_ip, SERVER_PORT);
    printf("  Credenciales: %s\n", credentials);
    printf("  Archivo: %s\n", filename);
    
    return 0;
}

// FASE 1: Autenticación (HELLO)
int send_hello(ClientState *state) {
    PDU pdu, ack;
    int retries = 0;
    int cred_len = strlen(state->credentials);
    
    printf("\n=== FASE 1: AUTENTICACION (HELLO) ===\n");
    
    // Construir HELLO PDU con seq_num = 0
    build_pdu(&pdu, TYPE_HELLO, 0, state->credentials, cred_len);
    
    while (retries < MAX_RETRIES) {
        printf("Enviando HELLO (intento %d/%d)...\n", retries + 1, MAX_RETRIES);
        
        // Enviar HELLO
        int sent = send_pdu(state->sockfd, &state->server_addr, &pdu, cred_len);
        if (sent < 0) {
            return -1;
        }
        
        print_pdu(&pdu, cred_len, "  TX:");
        
        // Esperar ACK con timeout
        struct sockaddr_in from_addr;
        int recv_len = recv_pdu_with_timeout(state->sockfd, &ack, 
                                            &from_addr, TIMEOUT_MS);
        
        if (recv_len > 0) {
            print_pdu(&ack, recv_len - 2, "  RX:");
            
            // Verificar que sea ACK con seq_num = 0
            if (ack.type == TYPE_ACK && ack.seq_num == 0) {
                // Verificar si hay mensaje de error en el payload
                if (recv_len > 2) {
                    printf("Error del servidor: %s\n", ack.data);
                    return -1;
                }
                
                printf("Autenticacion exitosa\n");
                return 0;
            } else {
                printf("  Respuesta incorrecta, ignorando...\n");
            }
        } else if (recv_len == 0) {
            printf("  Timeout, reintentando...\n");
        } else {
            printf("  Error en recepcion\n");
            return -1;
        }
        
        retries++;
    }
    
    printf("Fallo autenticacion despues de %d intentos\n", MAX_RETRIES);
    return -1;
}

// FASE 2: Parametrización (WRQ)
int send_wrq(ClientState *state) {
    PDU pdu, ack;
    int retries = 0;
    int filename_len = strlen(state->filename) + 1; 
    
    printf("\n=== FASE 2: PARAMETRIZACION (WRQ) ===\n");
    
    // Construir WRQ PDU con seq_num = 1
    build_pdu(&pdu, TYPE_WRQ, 1, state->filename, filename_len);
    
    while (retries < MAX_RETRIES) {
        printf("Enviando WRQ para '%s' (intento %d/%d)...\n", 
               state->filename, retries + 1, MAX_RETRIES);
        
        // Enviar WRQ
        int sent = send_pdu(state->sockfd, &state->server_addr, &pdu, filename_len);
        if (sent < 0) {
            return -1;
        }
        
        print_pdu(&pdu, filename_len, "  TX:");
        
        // Esperar ACK con timeout
        struct sockaddr_in from_addr;
        int recv_len = recv_pdu_with_timeout(state->sockfd, &ack, 
                                            &from_addr, TIMEOUT_MS);
        
        if (recv_len > 0) {
            print_pdu(&ack, recv_len - 2, "  RX:");
            
            // Verificar que sea ACK con seq_num = 1
            if (ack.type == TYPE_ACK && ack.seq_num == 1) {
                // Verificar si hay mensaje de error
                if (recv_len > 2) {
                    printf("Error del servidor: %s\n", ack.data);
                    return -1;
                }
                
                printf("WRQ aceptado\n");
                
                // Preparar para fase DATA (empezará con seq_num = 0)
                state->current_seq = 0;
                
                return 0;
            } else {
                printf("  Respuesta incorrecta, ignorando...\n");
            }
        } else if (recv_len == 0) {
            printf("  Timeout, reintentando...\n");
        } else {
            return -1;
        }
        
        retries++;
    }
    
    printf("Fallo WRQ despues de %d intentos\n", MAX_RETRIES);
    return -1;
}

// FASE 3: Transferencia de Datos (DATA)
int send_file_data(ClientState *state, const char *filepath) {
    FILE *file;
    PDU pdu, ack;
    uint8_t buffer[MAX_DATA_SIZE];
    int bytes_read;
    int chunk_num = 0;
    int total_sent = 0;
    
    printf("\n=== FASE 3: TRANSFERENCIA DE DATOS ===\n");
    
    // Abrir archivo
    file = fopen(filepath, "rb");
    if (!file) {
        perror("Error abriendo archivo");
        return -1;
    }
    
    // Obtener tamaño del archivo
    long file_size = get_file_size(filepath);
    printf("Tamanio del archivo: %ld bytes\n", file_size);
    printf("Chunks estimados: %ld\n", (file_size + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE);
    
    // Leer y enviar el archivo por chunks
    while ((bytes_read = fread(buffer, 1, MAX_DATA_SIZE, file)) > 0) {
        int retries = 0;
        int ack_received = 0;
        
        chunk_num++;
        
        // Construir DATA PDU con seq_num alternado (0, 1, 0, 1, ...)
        build_pdu(&pdu, TYPE_DATA, state->current_seq, buffer, bytes_read);
        
        printf("\nChunk #%d [%d bytes, seq=%d]\n", 
               chunk_num, bytes_read, state->current_seq);
        
        while (retries < MAX_RETRIES && !ack_received) {
            // Enviar DATA
            int sent = send_pdu(state->sockfd, &state->server_addr, 
                               &pdu, bytes_read);
            if (sent < 0) {
                fclose(file);
                return -1;
            }
            
            if (retries == 0) {
                printf("  TX: DATA seq=%d\n", state->current_seq);
            } else {
                printf("  TX: DATA seq=%d (retransmision %d)\n", 
                       state->current_seq, retries);
            }
            
            // Esperar ACK
            struct sockaddr_in from_addr;
            int recv_len = recv_pdu_with_timeout(state->sockfd, &ack, 
                                                &from_addr, TIMEOUT_MS);
            
            if (recv_len > 0) {
                // Verificar ACK correcto
                if (ack.type == TYPE_ACK && ack.seq_num == state->current_seq) {
                    printf("  RX: ACK seq=%d OK\n", ack.seq_num);
                    ack_received = 1;
                    total_sent += bytes_read;
                    
                    // Alternar seq_num: 0 -> 1, 1 -> 0
                    state->current_seq = 1 - state->current_seq;
                } else {
                    printf("  RX: ACK incorrecto (esperaba seq=%d, recibio type=%d seq=%d), ignorando...\n",
                           state->current_seq, ack.type, ack.seq_num);
                }
            } else if (recv_len == 0) {
                printf("  Timeout esperando ACK...\n");
            }
            
            if (!ack_received) {
                retries++;
            }
        }
        
        if (!ack_received) {
            printf("Fallo envio del chunk #%d despues de %d intentos\n", 
                   chunk_num, MAX_RETRIES);
            fclose(file);
            return -1;
        }
        
        // Mostrar progreso
        printf("  Progreso: %d / %ld bytes (%.1f%%)\n", 
               total_sent, file_size, 
               (total_sent * 100.0) / file_size);
    }
    
    fclose(file);
    printf("\nTransferencia completa: %d bytes en %d chunks\n", 
           total_sent, chunk_num);
    
    return 0;
}

// FASE 4: Finalización (FIN)
int send_fin(ClientState *state) {
    PDU pdu, ack;
    int retries = 0;
    
    printf("\n=== FASE 4: FINALIZACION (FIN) ===\n");
    
    // Construir FIN PDU con el seq_num actual
    build_pdu(&pdu, TYPE_FIN, state->current_seq, NULL, 0);
    
    while (retries < MAX_RETRIES) {
        printf("Enviando FIN con seq=%d (intento %d/%d)...\n", 
               state->current_seq, retries + 1, MAX_RETRIES);
        
        // Enviar FIN con payload vacío (data_len = 0)
        int sent = send_pdu(state->sockfd, &state->server_addr, &pdu, 0);
        if (sent < 0) {
            return -1;
        }
        
        print_pdu(&pdu, 0, "  TX:");
        
        // Esperar ACK
        struct sockaddr_in from_addr;
        int recv_len = recv_pdu_with_timeout(state->sockfd, &ack, 
                                            &from_addr, TIMEOUT_MS);
        
        if (recv_len > 0) {
            print_pdu(&ack, recv_len - 2, "  RX:");
            
            // Verificar ACK con seq_num correcto
            if (ack.type == TYPE_ACK && ack.seq_num == state->current_seq) {
                printf("Sesion finalizada correctamente\n");
                return 0;
            } else {
                printf("  Respuesta incorrecta, ignorando...\n");
            }
        } else if (recv_len == 0) {
            printf("  Timeout, reintentando...\n");
        }
        
        retries++;
    }
    
    printf("Fallo finalizacion despues de %d intentos\n", MAX_RETRIES);
    return -1;
}

// Programa principal del cliente UDP
int main(int argc, char *argv[]) {
    ClientState state;
    
    // Verificar argumentos
    if (argc != 5) {
        printf("Uso: %s <server_ip> <credentials> <filepath> <filename>\n", argv[0]);
        printf("Ejemplo: %s 127.0.0.1 g14-978e ./test_files/archivo_20kB testfile\n", argv[0]);
        return 1;
    }
    
    const char *server_ip = argv[1];
    const char *credentials = argv[2];
    const char *filepath = argv[3];   
    const char *filename = argv[4];    
    
    printf("========================================\n");
    printf("  CLIENTE UDP FILE TRANSFER\n");
    printf("========================================\n");
    
    // Inicializar cliente
    if (init_client(&state, server_ip, credentials, filename) < 0) {
        return 1;
    }
    
    // FASE 1: HELLO
    if (send_hello(&state) < 0) {
        close(state.sockfd);
        return 1;
    }
    
    // FASE 2: WRQ
    if (send_wrq(&state) < 0) {
        close(state.sockfd);
        return 1;
    }
    
    // FASE 3: DATA
    if (send_file_data(&state, filepath) < 0) {
        close(state.sockfd);
        return 1;
    }
    
    // FASE 4: FIN
    if (send_fin(&state) < 0) {
        close(state.sockfd);
        return 1;
    }
    
    // Cerrar socket
    close(state.sockfd);
    
    printf("\n========================================\n");
    printf("  TRANSFERENCIA EXITOSA\n");
    printf("========================================\n");
    
    return 0;
}