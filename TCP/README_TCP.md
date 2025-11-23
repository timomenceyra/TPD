# README – Cliente/Servidor TCP para medición de One-Way Delay

Este proyecto implementa la parte **TCP** del TP de Redes y Comunicación.  
Consta de dos programas en C:

- **client_tcp.c** → envía PDUs con timestamp.  
- **server_tcp.c** → recibe PDUs, calcula one-way delay y guarda resultados en CSV.

## 1. Compilación

```bash
gcc server_tcp.c -o server_tcp
gcc client_tcp.c -o client_tcp
```

## 2. Ejecución

### Servidor

```bash
./server_tcp
```

### Cliente

Modificar la IP en el código:

```c
inet_pton(AF_INET, "AQUI_IP_SERVIDOR", &server_addr.sin_addr);
```

Ejecutar:

```bash
./client_tcp -d 50 -N 10
```

## 3. Resultado

El archivo generado en el servidor:

```
tcp_delays.csv
```

## 4. Configuración de VMs

- Host-Only
- NAT + Port Forwarding
- Bridged + WANem

## 5. Pruebas WANem / NetEm

- Pérdidas: 1%, 2%, 5%
- Jitter: delay 50 ms, jitter 40 ms

## 6. Análisis

Graficar:
- X: número de medición
- Y: delay (s)

