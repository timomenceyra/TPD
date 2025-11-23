#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#define SERVER_PORT 20252
#define MIN_PAYLOAD 500
#define MAX_PAYLOAD 1000

ssize_t send_all(int sock, const void *buffer, size_t length) {
    size_t total_sent = 0;
    const uint8_t *buf = buffer;

    while (total_sent < length) {
        ssize_t sent = send(sock, buf + total_sent, length - total_sent, 0);
        if (sent <= 0) {
            return -1; // Error or connection closed
        }
        total_sent += sent;
    }
    return total_sent;
}

uint64_t get_timestamp_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

int main(int argc, char *argv[]) {
    // Parseo de argumentos
    if (argc != 5) {
        fprintf(stderr, "Uso: %s -d <ms> -N <segs>\n", argv[0]);
        exit(1);
    }

    int d_ms = 0;
    int N_secs = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            d_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-N") == 0) {
            N_secs = atoi(argv[++i]);
        }
    }

    if (d_ms <= 0 || N_secs <= 0) {
        fprintf(stderr, "Parámetros inválidos.\n");
        exit(1);
    }

    // Creación del socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    printf("Conectando al servidor...\n");
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(1);
    }
    printf("Conectado.\n");

    // Bucle
    uint64_t start = get_timestamp_usec();
    uint64_t now = start;

    // int payload_len = (rand() % (MAX_PAYLOAD - MIN_PAYLOAD + 1)) + MIN_PAYLOAD;
    // size_t pdu_len = 8 + payload_len + 1;

    // uint8_t *pdu = malloc(pdu_len);
    // memset(pdu + 8, 0x20, payload_len);
    // pdu[8 + payload_len] = '|';

    while (now - start < (uint64_t)N_secs * 1000000ULL) {

        size_t payload_len = (rand() % 501) + 500;
        size_t pdu_len = 8 + payload_len + 1;
        uint8_t *pdu = malloc(pdu_len);
        uint64_t ts = get_timestamp_usec();
        memcpy(pdu, &ts, 8);
        memset(pdu + 8, 0x20, payload_len);
        pdu[8 + payload_len] = '|';

        // uint64_t ts = get_timestamp_usec();
        // memcpy(pdu, &ts, 8);

        if (send_all(sock, pdu, pdu_len) < 0) {
            perror("send_all");
            free(pdu);
            break;
        }

        free(pdu);

        usleep(d_ms * 1000);
        now = get_timestamp_usec();
    }

    // free(pdu);
    close(sock);

    printf("Cliente terminado.\n");
    return 0;
}