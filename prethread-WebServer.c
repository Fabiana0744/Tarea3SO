#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define PUERTO_POR_DEFECTO 8080
#define TAM_BUFFER 4096

int descriptor_servidor;
int puerto = PUERTO_POR_DEFECTO;
int cantidad_hilos = 4;

void enviar_respuesta(int cliente_fd, const char *mensaje) {
    char respuesta[512];
    int len = snprintf(respuesta, sizeof(respuesta),
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n%s\n", mensaje);
    write(cliente_fd, respuesta, len);
}

void procesar_cliente(int cliente_fd) {
    char buffer[TAM_BUFFER] = {0};
    read(cliente_fd, buffer, TAM_BUFFER);
    printf("Solicitud recibida:\n%s\n", buffer);
    enviar_respuesta(cliente_fd, "Servidor básico en funcionamiento");
}

void *atender_cliente(void *arg) {
    struct sockaddr_in dir_cli;
    socklen_t lon = sizeof(dir_cli);
    while (1) {
        int cli_fd = accept(descriptor_servidor, (struct sockaddr *)&dir_cli, &lon);
        if (cli_fd < 0) { perror("Error al aceptar conexión"); continue; }
        procesar_cliente(cli_fd);
        close(cli_fd);
    }
    return NULL;
}

void procesar_argumentos(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            cantidad_hilos = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            puerto = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Uso: %s [-n hilos] [-p puerto]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[]) {
    procesar_argumentos(argc, argv);

    struct sockaddr_in dir_ser;
    descriptor_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor_servidor < 0) { perror("Error al crear socket"); exit(EXIT_FAILURE); }

    dir_ser.sin_family = AF_INET;
    dir_ser.sin_addr.s_addr = INADDR_ANY;
    dir_ser.sin_port = htons(puerto);

    if (bind(descriptor_servidor, (struct sockaddr *)&dir_ser, sizeof(dir_ser)) < 0) { perror("Error en bind"); exit(EXIT_FAILURE); }
    if (listen(descriptor_servidor, 10) < 0) { perror("Error en listen"); exit(EXIT_FAILURE); }

    printf("Servidor básico escuchando en el puerto %d con %d hilos...\n", puerto, cantidad_hilos);

    pthread_t hilos[cantidad_hilos];
    for (int i = 0; i < cantidad_hilos; i++) pthread_create(&hilos[i], NULL, atender_cliente, NULL);
    for (int i = 0; i < cantidad_hilos; i++) pthread_join(hilos[i], NULL);

    close(descriptor_servidor);
    return 0;
}
