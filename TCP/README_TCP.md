Este módulo implementa el Cliente y Servidor TCP utilizados para medir el one-way delay de PDUs enviadas desde el cliente hacia el servidor, respetando el formato solicitado por la cátedra.

Compilación

El proyecto se compila desde la carpeta TCP usando:

```bash
make
```

Esto genera dos binarios:

- `client_tcp`
- `server_tcp`

---

Ejecución

Servidor

```bash
make run-server
```

o directamente:

```bash
./server_tcp
```

El servidor escucha en el puerto indicado en `server_tcp.c` (20252), recibe PDUs, detecta framing usando el delimitador '|', valida tamaños y calcula el delay real:
delay = arrival_timestamp - origin_timestamp

Los resultados se guardan en `tcp_delays.csv`.

---

Cliente

El cliente envía PDUs con un timestamp de 8 bytes, un payload de entre 500 y 1000 bytes, y un delimitador `'|'`.

La interfaz es:

```bash
./client_tcp -d <ms> -N <segundos>
```

- `-d <ms>` → intervalo entre envíos (milisegundos)
- `-N <seg>` → duración total del envío (segundos)

Ejemplo:

```bash
./client_tcp -d 50 -N 3
```

Esto envía una PDU cada 50 ms durante 3 segundos (aprox. 60 PDUs).

También podés usar:

```bash
make run-client
```

que corre el cliente con `-d 50 -N 3` como valores por defecto.

---

Limpiar el directorio

Para borrar los binarios y el CSV generado:

```bash
make clean
```

---
