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
#include <netinet/in.h>

#define SERVER_PORT 20252
#define BACKLOG 10
#define BUFFER_SIZE 4096

static uint64_t get_timestamp_usec(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday falló");
        return 0;
    }
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

static void process_pdu(const uint8_t *buf, size_t len, size_t *measurement_index) {
    if (len < 9) {
        fprintf(stderr, "La PDU recibida es demasiado corta\n");
        return;
    }

    uint64_t origin_ts = 0;
    memcpy(&origin_ts, buf, sizeof(uint64_t));

    uint64_t dest_ts = get_timestamp_usec();

    if (dest_ts < origin_ts) {
        fprintf(stderr, "Marca de tiempo de destino es menor que la de origen\n");
        return;
    }

    uint64_t delay_usec = dest_ts - origin_ts;
    double delay_sec = (double)delay_usec / 1000000.0;

    (*measurement_index)++;
    printf("Medición %zu: delay = %.6f s (tamaño PDU = %zu bytes)\n", *measurement_index, delay_sec, len);
    fflush(stdout);
}

