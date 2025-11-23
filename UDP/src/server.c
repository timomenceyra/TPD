#include "../include/protocol.h"

/* ==================== FUNCIONES DEL SERVIDOR ==================== */

/**
 * Compara dos direcciones sockaddr_in
 * Retorna 1 si son iguales, 0 si no
 */
int addr_equal(struct sockaddr_in *addr1, struct sockaddr_in *addr2) {
    return (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr &&
            addr1->sin_port == addr2->sin_port);
}

/**
 * Busca una sesión de cliente existente por su dirección
 * Retorna: puntero a la sesión o NULL si no existe
 */
ClientSession* find_session(ServerState *state, struct sockaddr_in *client_addr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (state->clients[i].active && 
            addr_equal(&state->clients[i].addr, client_addr)) {
            return &state->clients[i];
        }
    }
    return NULL;
}

/**
 * Crea una nueva sesión de cliente
 * Retorna: puntero a la nueva sesión o NULL si no hay espacio
 */
ClientSession* create_session(ServerState *state, struct sockaddr_in *client_addr) {
    // Buscar slot libre
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!state->clients[i].active) {
            // Inicializar sesión
            memset(&state->clients[i], 0, sizeof(ClientSession));
            memcpy(&state->clients[i].addr, client_addr, sizeof(struct sockaddr_in));
            state->clients[i].active = 1;
            state->clients[i].phase = PHASE_NONE;
            state->clients[i].expected_seq = 0;
            state->clients[i].file = NULL;
            state->clients[i].last_activity = time(NULL);
            
            printf("\n[NUEVA SESION] Cliente ");
            print_address(client_addr);
            printf(" (slot %d)\n", i);
            
            return &state->clients[i];
        }
    }
    
    printf("[ERROR] No hay slots disponibles para nuevo cliente\n");
    return NULL;
}

/**
 * Libera una sesión de cliente
 */
void free_session(ClientSession *session) {
    if (session->file) {
        fclose(session->file);
        session->file = NULL;
    }
    
    printf("\n[SESION CERRADA] Cliente ");
    print_address(&session->addr);
    printf(" (fase: %s)\n", phase_to_string(session->phase));
    
    session->active = 0;
    session->phase = PHASE_NONE;
}

/**
 * Envía un ACK al cliente
 */
int send_ack(ServerState *state, struct sockaddr_in *client_addr, 
             uint8_t seq_num, const char *error_msg) {
    PDU ack;
    int data_len = 0;
    
    if (error_msg && strlen(error_msg) > 0) {
        data_len = strlen(error_msg);
        build_pdu(&ack, TYPE_ACK, seq_num, error_msg, data_len);
    } else {
        build_pdu(&ack, TYPE_ACK, seq_num, NULL, 0);
    }
    
    return send_pdu(state->sockfd, client_addr, &ack, data_len);
}

/**
 * Handler para HELLO (Fase 1: Autenticación)
 * CORREGIDO: Validar longitud máxima de credenciales (10 chars) y solo ASCII
 */
void handle_hello(ServerState *state, ClientSession *session, 
                  PDU *pdu, struct sockaddr_in *client_addr, int data_len) {
    printf("[HELLO] Cliente ");
    print_address(client_addr);
    printf(" - Fase actual: %s\n", phase_to_string(session->phase));
    
    // Verificar que sea seq_num = 0
    if (pdu->seq_num != 0) {
        printf("[ERROR] HELLO con seq_num incorrecto (%d), descartando\n", pdu->seq_num);
        return;
    }
    
    // CORREGIDO: Validar longitud máxima (10 caracteres según aclaración del profesor)
    if (data_len > MAX_CREDENTIALS_LEN) {
        printf("[ERROR] Credenciales muy largas (%d caracteres, max %d)\n", 
               data_len, MAX_CREDENTIALS_LEN);
        send_ack(state, client_addr, 0, "Credencial invalida (max 10 chars)");
        return;
    }
    
    // CORREGIDO: Validar que solo tenga caracteres ASCII imprimibles
    for (int i = 0; i < data_len; i++) {
        if (pdu->data[i] < 32 || pdu->data[i] > 126) {
            printf("[ERROR] Credenciales con caracteres no-ASCII\n");
            send_ack(state, client_addr, 0, "Credencial invalida (solo ASCII)");
            return;
        }
    }
    
    // Extraer credenciales
    char credentials[MAX_CREDENTIALS_SIZE];
    memset(credentials, 0, sizeof(credentials));
    memcpy(credentials, pdu->data, data_len);
    
    // Verificar credenciales
    if (strcmp(credentials, state->credentials) != 0) {
        printf("[ERROR] Credenciales invalidas: '%s'\n", credentials);
        send_ack(state, client_addr, 0, "Credenciales invalidas");
        return;
    }
    
    printf("[OK] Credenciales validas: '%s'\n", credentials);
    
    // Actualizar estado
    session->phase = PHASE_AUTHENTICATED;
    session->expected_seq = 1; // Próximo será WRQ con seq=1
    session->last_activity = time(NULL);
    
    // Enviar ACK
    send_ack(state, client_addr, 0, NULL);
    printf("  TX: ACK seq=0\n");
}

/**
 * Handler para WRQ (Fase 2: Write Request)
 */
void handle_wrq(ServerState *state, ClientSession *session, 
                PDU *pdu, struct sockaddr_in *client_addr, int data_len) {
    printf("[WRQ] Cliente ");
    print_address(client_addr);
    printf(" - Fase actual: %s\n", phase_to_string(session->phase));
    
    // Verificar que esté autenticado
    if (session->phase != PHASE_AUTHENTICATED) {
        printf("[ERROR] WRQ sin autenticacion previa, descartando\n");
        return;
    }
    
    // Verificar seq_num = 1
    if (pdu->seq_num != 1) {
        printf("[ERROR] WRQ con seq_num incorrecto (%d), descartando\n", pdu->seq_num);
        return;
    }
    
    // Extraer filename (null-terminated)
    char filename[MAX_FILENAME_LEN + 1];
    memset(filename, 0, sizeof(filename));
    memcpy(filename, pdu->data, data_len);
    
    printf("[INFO] Filename solicitado: '%s'\n", filename);
    
    // Validar filename
    if (!validate_filename(filename)) {
        send_ack(state, client_addr, 1, "Filename invalido (4-10 caracteres ASCII)");
        return;
    }
    
    // Crear directorio test_files si no existe
    static int dir_created = 0;
    if (!dir_created) {
        system("mkdir -p test_files");
        dir_created = 1;
    }
    
    // Crear path completo para el archivo
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "test_files/%s.received", filename);
    
    // Intentar abrir archivo para escritura
    session->file = fopen(filepath, "wb");
    if (!session->file) {
        perror("[ERROR] No se pudo crear archivo");
        send_ack(state, client_addr, 1, "Error creando archivo en servidor");
        return;
    }
    
    printf("[OK] Archivo abierto: %s\n", filepath);
    
    // Guardar filename y actualizar estado
    strncpy(session->filename, filename, MAX_FILENAME_LEN);
    session->phase = PHASE_WRQ_OK;
    session->expected_seq = 0; // Próximo DATA será seq=0
    session->last_activity = time(NULL);
    
    // Enviar ACK
    send_ack(state, client_addr, 1, NULL);
    printf("  TX: ACK seq=1\n");
}

/**
 * Handler para DATA (Fase 3: Transferencia)
 */
void handle_data(ServerState *state, ClientSession *session, 
                 PDU *pdu, struct sockaddr_in *client_addr, int data_len) {
    
    // Verificar que esté en fase correcta
    if (session->phase != PHASE_WRQ_OK && session->phase != PHASE_TRANSFERRING) {
        printf("[ERROR] DATA sin WRQ previo, descartando\n");
        return;
    }
    
    // Verificar seq_num correcto
    if (pdu->seq_num != session->expected_seq) {
        printf("[ERROR] DATA con seq_num incorrecto (esperado=%d, recibido=%d), descartando\n",
               session->expected_seq, pdu->seq_num);
        return;
    }
    
    printf("[DATA] seq=%d, %d bytes - ", pdu->seq_num, data_len);
    
    // Escribir datos al archivo
    if (session->file) {
        size_t written = fwrite(pdu->data, 1, data_len, session->file);
        if (written != (size_t)data_len) {
            printf("[ERROR] Error escribiendo en archivo\n");
            return;
        }
        fflush(session->file);
        printf("escrito OK\n");
    } else {
        printf("[ERROR] Archivo no abierto\n");
        return;
    }
    
    // Actualizar estado
    session->phase = PHASE_TRANSFERRING;
    session->last_activity = time(NULL);
    
    // Enviar ACK con el mismo seq_num
    send_ack(state, client_addr, pdu->seq_num, NULL);
    printf("  TX: ACK seq=%d\n", pdu->seq_num);
    
    // Alternar expected_seq: 0 -> 1, 1 -> 0
    session->expected_seq = 1 - session->expected_seq;
}

/**
 * Handler para FIN (Fase 4: Finalización)
 * CORREGIDO: Según aclaración del profesor, NO validar filename (payload está vacío)
 */
void handle_fin(ServerState *state, ClientSession *session, 
                PDU *pdu, struct sockaddr_in *client_addr, int data_len) {
    printf("[FIN] Cliente ");
    print_address(client_addr);
    printf(" - Fase actual: %s\n", phase_to_string(session->phase));
    
    // Verificar que esté en transferencia
    if (session->phase != PHASE_TRANSFERRING) {
        printf("[ERROR] FIN sin transferencia previa, descartando\n");
        return;
    }
    
    // CORREGIDO: Según aclaración del profesor, el payload debe estar vacío
    // NO validar filename, solo verificar que el payload esté vacío o ignorarlo
    if (data_len > 0) {
        printf("[WARNING] FIN con payload no vacío (%d bytes), ignorando payload\n", data_len);
    }
    
    printf("[OK] Transferencia completada para archivo: %s\n", session->filename);
    
    // Cerrar archivo
    if (session->file) {
        fclose(session->file);
        session->file = NULL;
    }
    
    // Actualizar estado
    session->phase = PHASE_COMPLETED;
    session->last_activity = time(NULL);
    
    // Enviar ACK final con el seq_num de la PDU FIN recibida
    send_ack(state, client_addr, pdu->seq_num, NULL);
    printf("  TX: ACK seq=%d\n", pdu->seq_num);
    
    // Liberar sesión
    free_session(session);
}

/**
 * Procesa un mensaje recibido
 */
void handle_message(ServerState *state, PDU *pdu, struct sockaddr_in *client_addr, 
                   int recv_len) {
    // Calcular tamaño de datos (recv_len - 2 bytes de header)
    int data_len = recv_len - 2;
    
    printf("\n----------------------------------------\n");
    printf("RX: ");
    print_pdu(pdu, data_len, "");
    printf("De: ");
    print_address(client_addr);
    printf("\n");
    
    // Buscar o crear sesión
    ClientSession *session = find_session(state, client_addr);
    
    if (!session) {
        // Solo crear sesión nueva si es HELLO
        if (pdu->type == TYPE_HELLO) {
            session = create_session(state, client_addr);
            if (!session) {
                printf("[ERROR] No se pudo crear sesion\n");
                return;
            }
        } else {
            printf("[ERROR] Cliente sin sesion enviando %s, descartando\n",
                   pdu_type_to_string(pdu->type));
            return;
        }
    }
    
    // Procesar según tipo de PDU
    switch (pdu->type) {
        case TYPE_HELLO:
            handle_hello(state, session, pdu, client_addr, data_len);
            break;
        case TYPE_WRQ:
            handle_wrq(state, session, pdu, client_addr, data_len);
            break;
        case TYPE_DATA:
            handle_data(state, session, pdu, client_addr, data_len);
            break;
        case TYPE_FIN:
            handle_fin(state, session, pdu, client_addr, data_len);
            break;
        default:
            printf("[ERROR] Tipo de PDU desconocido (%d), descartando\n", pdu->type);
    }
}

/**
 * Inicializa el servidor
 */
int init_server(ServerState *state, const char *credentials) {
    // Validar credenciales del servidor
    if (!validate_credentials(credentials)) {
        printf("[ERROR] Credenciales del servidor invalidas\n");
        return -1;
    }
    
    // Copiar credenciales
    strncpy(state->credentials, credentials, MAX_CREDENTIALS_SIZE - 1);
    
    // Crear socket
    state->sockfd = create_udp_socket();
    if (state->sockfd < 0) {
        return -1;
    }
    
    // Configurar dirección del servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    // Bind
    if (bind(state->sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");
        close(state->sockfd);
        return -1;
    }
    
    // Inicializar array de clientes
    for (int i = 0; i < MAX_CLIENTS; i++) {
        state->clients[i].active = 0;
    }
    
    printf("========================================\n");
    printf("  SERVIDOR UDP FILE TRANSFER\n");
    printf("========================================\n");
    printf("Puerto: %d\n", SERVER_PORT);
    printf("Credenciales: %s\n", credentials);
    printf("Max clientes: %d\n", MAX_CLIENTS);
    printf("Escuchando...\n\n");
    
    return 0;
}

/**
 * Función principal del servidor
 */
int main(int argc, char *argv[]) {
    ServerState state;
    
    // Credencial hardcodeada para tests
    const char *credentials = "TEST";
    
    // Si se pasa argumento, usarlo (para pruebas con g14-978e)
    if (argc == 2) {
        credentials = argv[1];
    }
    
    // Inicializar servidor
    if (init_server(&state, credentials) < 0) {
        return 1;
    }
    
    // Loop principal
    PDU pdu;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    
    while (1) {
        addr_len = sizeof(client_addr);
        
        // Recibir mensaje
        int recv_len = recvfrom(state.sockfd, &pdu, sizeof(PDU), 0,
                               (struct sockaddr*)&client_addr, &addr_len);
        
        if (recv_len < 0) {
            perror("Error en recvfrom");
            continue;
        }
        
        if (recv_len < 2) {
            printf("[ERROR] PDU demasiado pequeña (%d bytes), descartando\n", recv_len);
            continue;
        }
        
        // Procesar mensaje
        handle_message(&state, &pdu, &client_addr, recv_len);
    }
    
    close(state.sockfd);
    return 0;
}