#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>

#define PUERTO_POR_DEFECTO 8080
#define TAM_BUFFER 4096

int descriptor_servidor;
int puerto = PUERTO_POR_DEFECTO;
char *directorio_raiz = ".";
int cantidad_hilos = 4;
int clientes_activos = 0;
pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER;


struct Protocolo {
    int puerto1;
    int puerto2;
    const char *nombre;
};

struct Protocolo protocolos[] = {
    {21, 2121, "FTP"},
    {22, 2222, "SSH"},
    {25, 2525, "SMTP"},
    {53, 5353, "DNS"},
    {23, 2323, "TELNET"},
    {161, 16161, "SNMP"},
};

const char* obtener_tipo_contenido(const char *nombre_archivo) {
    static const struct { const char *ext, *tipo; } tabla[] = {
        { "html", "text/html" }, { "htm",  "text/html" },
        { "txt",  "text/plain" },
        { "jpg",  "image/jpeg" }, { "jpeg", "image/jpeg" },
        { "png",  "image/png" },
        { "gif",  "image/gif" },
        { "css",  "text/css" },
        { "js",   "application/javascript" },
        { "json", "application/json" },
        { "pdf",  "application/pdf" },
        { "zip",  "application/zip" },
        { "xml",  "application/xml" },
        { "mp4",  "video/mp4" },
        { "mp3",  "audio/mpeg" },
        { "wav",  "audio/wav" },
        { "avi",  "video/x-msvideo" },
        { "flv",  "video/x-flv" },
        { "mkv",  "video/x-matroska" },
        { "svg",  "image/svg+xml" },
        { "ico",  "image/x-icon" },
        { "woff", "font/woff" },
        { "woff2","font/woff2" },
        { "ttf",  "font/ttf" },
        { "otf",  "font/otf" },
        { "eot",  "font/eot" },
        { "csv",  "text/csv" },
        { NULL,   NULL }
    };

    const char *ext = strrchr(nombre_archivo, '.');
    if (!ext) return "application/octet-stream";
    ext++;

    for (int i = 0; tabla[i].ext; i++) {
        if (strcasecmp(ext, tabla[i].ext) == 0)
            return tabla[i].tipo;
    }
    return "application/octet-stream";
}

const char* detectar_protocolo(int puerto) {
    for (int i = 0; i < sizeof(protocolos) / sizeof(protocolos[0]); i++) {
        if (puerto == protocolos[i].puerto1 || puerto == protocolos[i].puerto2) {
            return protocolos[i].nombre;
        }
    }
    return "Desconocido";
}

void enviar_error(int cliente_fd, int codigo, const char *texto, const char *tipo_contenido) {
    char respuesta[512];
    int len = snprintf(respuesta, sizeof(respuesta),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nConnection: close\r\n\r\n%s\n",
        codigo, texto, tipo_contenido, texto);
    write(cliente_fd, respuesta, len);
    printf("HTTP/1.1 %d %s\n", codigo, texto);
}

void enviar_archivo(int cliente_fd, const char *tipo_contenido, int archivo_fd, const char *metodo) {
    char cabecera[256];
    int len = snprintf(cabecera, sizeof(cabecera),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nConnection: close\r\n\r\n", tipo_contenido);
    write(cliente_fd, cabecera, len);
    printf("%s", cabecera);
    if (strcmp(metodo, "GET") == 0) {
        char buffer[TAM_BUFFER];
        int leidos;
        while ((leidos = read(archivo_fd, buffer, TAM_BUFFER)) > 0) {
            write(cliente_fd, buffer, leidos);
        }
    }
    close(archivo_fd);
}

void recibir_y_guardar_datos(int cliente_fd, int archivo_fd, const char *cabecera_http, int tamanio_cabecera) {
    int contenido_total = 0;
    const char *pos_content_length = strcasestr(cabecera_http, "Content-Length:");
    
    if (pos_content_length) {
        sscanf(pos_content_length, "Content-Length: %d", &contenido_total);
    }

    if (contenido_total <= 0) return;

    const char *inicio_cuerpo = strstr(cabecera_http, "\r\n\r\n");
    if (!inicio_cuerpo) return;

    inicio_cuerpo += 4;  // Saltar los "\r\n\r\n"
    int bytes_header = inicio_cuerpo - cabecera_http;
    int cuerpo_en_buffer = tamanio_cabecera - bytes_header;

    if (cuerpo_en_buffer > 0) {
        write(archivo_fd, inicio_cuerpo, cuerpo_en_buffer);
    }

    int faltantes = contenido_total - cuerpo_en_buffer;
    char buffer[TAM_BUFFER];

    while (faltantes > 0) {
        int cantidad = read(cliente_fd, buffer, (faltantes > TAM_BUFFER) ? TAM_BUFFER : faltantes);
        if (cantidad <= 0) break;
        write(archivo_fd, buffer, cantidad);
        faltantes -= cantidad;
    }
}

void procesar_cliente(int cliente_fd) {
    char buffer[TAM_BUFFER] = {0};
    read(cliente_fd, buffer, TAM_BUFFER);
    printf("Solicitud: %s\n", buffer);

    // Verificar protocolo por puerto
    if (puerto != 80 && puerto != PUERTO_POR_DEFECTO) {
        const char *protocolo = detectar_protocolo(puerto);
        char resp[256];
        int L = snprintf(resp, sizeof(resp),
            "HTTP/1.1 501 Not Implemented\r\nConnection: close\r\n\r\nEl protocolo %s no está implementado en este servidor. Solo se permite el uso de HTTP/1.1 a través del puerto %d.\n",
            protocolo, PUERTO_POR_DEFECTO);
        write(cliente_fd, resp, L);
        printf("%s", resp);
        return;
    }

    char metodo[8], ruta[1024];
    sscanf(buffer, "%7s %1023s", metodo, ruta);
    char *nombre = ruta + 1;
    if (strlen(nombre) == 0) nombre = "index.html";
    char ruta_completa[2048];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", directorio_raiz, nombre);
    const char *tipo = obtener_tipo_contenido(nombre);

    if (strcmp(metodo, "GET") == 0 || strcmp(metodo, "HEAD") == 0) {
        int fd = open(ruta_completa, O_RDONLY);
        if (fd < 0) {
            enviar_error(cliente_fd, 404, "Not Found", tipo);
        } else {
            enviar_archivo(cliente_fd, tipo, fd, metodo);
        }
    } else if (strcmp(metodo, "POST") == 0) {
        char *cuerpo = strstr(buffer, "\r\n\r\n");
        if (cuerpo) cuerpo += 4;
        char resp[256 + strlen(cuerpo)];
        int len = snprintf(resp, sizeof(resp),
            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nDatos recibidos correctamente: %s",
            cuerpo);
        write(cliente_fd, resp, len);
        printf("%s\n", resp);
    } else if (strcmp(metodo, "PUT") == 0) {
        int fd = open(ruta_completa, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            enviar_error(cliente_fd, 500, "Internal Server Error", tipo);
        } else {
            recibir_y_guardar_datos(cliente_fd, fd, buffer, strlen(buffer));
            close(fd);
            enviar_error(cliente_fd, 201, "Created", tipo);
        }
    } else if (strcmp(metodo, "DELETE") == 0) {
        if (unlink(ruta_completa) == 0) {
            enviar_error(cliente_fd, 200, "OK", tipo);
        } else {
            enviar_error(cliente_fd, 404, "Not Found", tipo);
        }
    } else {
        enviar_error(cliente_fd, 501, "Not Implemented", "text/plain");
    }
}

void *atender_cliente(void *arg) {
    struct sockaddr_in dir_cli;
    socklen_t lon = sizeof(dir_cli);
    while (1) {
        int cli_fd = accept(descriptor_servidor, (struct sockaddr *)&dir_cli, &lon);
        if (cli_fd < 0) { perror("Error al aceptar conexión"); continue; }

        pthread_mutex_lock(&mutex_clientes);
        if (clientes_activos >= cantidad_hilos) {
            pthread_mutex_unlock(&mutex_clientes);
            enviar_error(cli_fd, 503, "Service Unavailable", "text/plain");
            close(cli_fd);
            continue;
        }
        clientes_activos++;
        pthread_mutex_unlock(&mutex_clientes);

        procesar_cliente(cli_fd);
        close(cli_fd);

        pthread_mutex_lock(&mutex_clientes);
        clientes_activos--;
        pthread_mutex_unlock(&mutex_clientes);
    }
    return NULL;
}



void procesar_argumentos(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            cantidad_hilos = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            directorio_raiz = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            puerto = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Uso incorrecto: %s -n <hilos> -w <directorio> -p <puerto>\n", argv[0]);
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

    printf("============================================\n");
    printf("Servidor iniciado exitosamente.\n");
    printf("Puerto en uso     : %d\n", puerto);
    printf("Hilos disponibles : %d\n", cantidad_hilos);
    printf("Directorio raíz   : %s\n", directorio_raiz);
    printf("============================================\n");

    pthread_t hilos[cantidad_hilos];
    for (int i = 0; i < cantidad_hilos; i++) pthread_create(&hilos[i], NULL, atender_cliente, NULL);
    for (int i = 0; i < cantidad_hilos; i++) pthread_join(hilos[i], NULL);

    close(descriptor_servidor);
    return 0;
}
