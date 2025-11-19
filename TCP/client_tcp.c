#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>

#define SERVER_PORT 20252
#define MIN_PAYLOAD 500
#define MAX_PAYLOAD 1000

static void usage(const char *prog) {
    fprintf(stderr, "Uso: %s -h host -d milisegundos -N segundos\n", prog);
    exit(EXIT_FAILURE);
}

static uint64_t get_timestamp_usec(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday falló");
        return 0;
    }
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

static int send_all(int sockfd, const void *buf, size_t len) {
    size_t total_sent = 0;
    const uint8_t *p = (const uint8_t *)buf;

    while (total_sent < len) {
        ssize_t nsent = send(sockfd, p + total_sent, len - total_sent, 0);
        if (nsent < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Error en send");
            return -1;
        }
        if (nsent == 0) {
            fprintf(stderr, "Conexión cerrada durante el envío\n");
            return -1;
        }
        total_sent += (size_t)nsent;
    }
    return 0;
}