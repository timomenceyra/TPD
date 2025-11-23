#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#define SERVER_PORT 20252
#define RECV_BUF_SIZE 4096
#define MIN_PDU_SIZE 509
#define MAX_PDU_SIZE 1009   // 8 + 1000 + '|' de sobra

uint64_t get_timestamp_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

void process_pdu(const uint8_t *pdu, size_t pdu_len, FILE *fp, size_t *seq) {
    // Validaciones de tamaños
    // Tamaño PDU
    if (pdu_len < MIN_PDU_SIZE || pdu_len > MAX_PDU_SIZE) {
        fprintf(stderr, "PDU con tamańo inválido (%zu bytes, esperado entre 509 y 1009), ignorada.\n", pdu_len);
        return;
    }

    // Caracter final
    if (pdu[pdu_len - 1] != '|') {
        fprintf(stderr, "PDU inválida: no termina con '|', ignorada.\n");
        return;
    }

    // Tamaño payload
    size_t payload_len = pdu_len - 9;
    if (payload_len < 500 || payload_len > 1000) {
        fprintf(stderr, "Payload con tamaño inválido (%zu bytes, esperado entre 500 y 1000), PDU ignorada.\n", payload_len);
        return;
    }

    // Lectura de la PDU
    uint64_t origin_ts = 0;
    memcpy(&origin_ts, pdu, sizeof(origin_ts));
    uint64_t dest_ts = get_timestamp_usec();

    int delay_us = (int64_t)(dest_ts - origin_ts);
    double delay_sec = (double)delay_us / 1000000.0;

    // Carga en archivo CSV
    (*seq)++;
    fprintf(fp, "%zu,%.5f\n", *seq, delay_sec);
    fflush(fp);
}

void handle_client(int client_sock, FILE *fp) {
    uint8_t recv_buf[RECV_BUF_SIZE];
    uint8_t assembly_buf[RECV_BUF_SIZE * 4];
    size_t assembly_len = 0;
    size_t seq = 0;

    while(1) {
        ssize_t n = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        if (n < 0) {
            perror("recv");
            break;
        }

        if (n == 0) {
            printf("El cliente cerró la conexión.\n");
            break;
        }

        // ESTA OPCIÓN DESCARTA LO QUE YA ESTABA EN EL BUFFER
        if (assembly_len + (size_t)n > sizeof(assembly_buf)) {
            fprintf(stderr, "Overflow en el buffer de ensamblado. Se descarta la información que estaba.\n");
            assembly_len = 0;
        }

        // ESTA OPCIÓN DESCARTA LA INFO NUEVA QUE LLEGA
        // if (assembly_len + (size_t)n > sizeof(assembly_buf)) {
        //     fprintf(stderr, "Overflow en el buffer de ensamblado. Se descarta la información nueva.\n");
        //     continue;
        // }

        // ESTA OPCIÓN CIERRA EL CLIENTE ANTES OVERFLOW EN EL BUFFER DE ENSAMBLADO
        // if (assembly_len + (size_t)n > sizeof(assembly_buf)) {
        //     fprintf(stderr, "Overflow en el buffer de ensamblado. Cerrando la conexión.\n");
        //     close(client_sock);
        //     return;
        // }

        // HAY QUE REVISAR QUE OPCIÓN PREFERIMOS HACER

        memcpy(assembly_buf + assembly_len, recv_buf, (size_t)n);
        assembly_len += (size_t)n;

        // Extracción de PDUs completas
        while (1) {
            size_t i;
            int found = 0;
            for (i = 0; i < assembly_len; i++) {
                if (assembly_buf[i] == '|') {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                break;
            }

            size_t pdu_len = i + 1;

            if (pdu_len > MAX_PDU_SIZE) {
                fprintf(stderr, "PDU demasiado grande (%zu bytes), ignorada.\n", pdu_len);
                memmove(assembly_buf, assembly_buf + pdu_len, assembly_len - pdu_len);
                assembly_len -= pdu_len;
                continue;
            }

            // Se procesa la PDU encontrada y se elimina del buffer de ensamblado
            process_pdu(assembly_buf, pdu_len, fp, &seq);
            size_t remaining = assembly_len - pdu_len;
            memmove(assembly_buf, assembly_buf + pdu_len, remaining);
            assembly_len = remaining;
        }
    }
}

int main(void) {
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror ("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(listen_sock, 1) < 0) {
        perror("listen");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    printf("Servidor TCP escuchando en puerto %d...\n", SERVER_PORT);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
    if (client_sock < 0) {
        perror("accept");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    printf("Cliente conectado desde %s:%d\n", client_ip, ntohs(client_addr.sin_port));

    // Abre el archivo CSV
    FILE *fp = fopen("tcp_delays.csv", "w");
    if (!fp) {
        perror("fopen");
        close(client_sock);
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "seq,delay_seconds\n");
    handle_client(client_sock, fp);

    fclose(fp);
    close(client_sock);
    close(listen_sock);

    printf("Sevidor terminado.\n");
    return 0;
}