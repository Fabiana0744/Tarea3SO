#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// Variables globales
char *ruta_archivo = NULL;

// Estructura para almacenar datos de respuesta HTTP en memoria.
struct Memoria {
    char *respuesta;
    size_t tamano;
};

/*
 * Obtiene el tipo de contenido (Content-Type) basado en la extensión de un archivo.
 * Entrada:
 *   - nombre_archivo: Nombre o ruta del archivo del cual extraer la extensión.
 * Salida:
 *   - Retorna un string con el tipo de contenido asociado o "application/octet-stream" si no se encuentra.
 */
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
        { "xci",  "application/octet-stream" },
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
    if (!ext) return "text/plain";

    ext++; // saltar el punto
    for (int i = 0; tabla[i].ext != NULL; i++) {
        if (strcasecmp(ext, tabla[i].ext) == 0) {
            return tabla[i].tipo;
        }
    }

    return "application/octet-stream";      // Tipo genérico si no se reconoce la extensión
}


/*
 * Función de escritura que almacena la respuesta del servidor en memoria.
 * Entrada:
 *   - contenido: Puntero a los datos recibidos.
 *   - tamano_elemento: Tamaño de cada unidad de contenido.
 *   - cantidad_elementos: Número de unidades de contenido.
 *   - memoria_usuario: Puntero a la estructura de memoria del usuario.
 * Salida:
 *   - Retorna el número de bytes escritos.
 */
size_t escribir_respuesta(void *contenido, size_t tamano_elemento, size_t cantidad_elementos, void *memoria_usuario) {
    size_t total_bytes = tamano_elemento * cantidad_elementos;
    struct Memoria *memoria = (struct Memoria *)memoria_usuario;

    if (!contenido || total_bytes == 0) return 0;

    char *nueva_respuesta = realloc(memoria->respuesta, memoria->tamano + total_bytes + 1);     // Redimensionar la memoria para agregar los nuevos datos
    if (nueva_respuesta == NULL) {
        fprintf(stderr, "Error de memoria al expandir la respuesta.\n");
        return 0;
    }

    memoria->respuesta = nueva_respuesta;
    memcpy(memoria->respuesta + memoria->tamano, contenido, total_bytes);
    memoria->tamano += total_bytes;
    memoria->respuesta[memoria->tamano] = '\0';

    return total_bytes;
}


/*
 * Configura los parámetros de una solicitud HTTP según el método especificado.
 * Entrada:
 *   - curl: Puntero al manejador CURL inicializado.
 *   - metodo: Método HTTP ("GET", "POST", "PUT", "DELETE", "HEAD").
 *   - datos: Datos a enviar en la solicitud (pueden ser NULL).
 *   - url: URL destino de la solicitud.
 *   - fp: Puntero a archivo usado para cargas PUT.
 *   - tamano_archivo: Puntero al tamaño del archivo en caso de PUT.
 * Salida:
 *   - No retorna valor. Configura directamente el objeto CURL.
 */
void configurar_solicitud(CURL *curl, const char *metodo, const char *datos, const char *url, FILE **fp, long *tamano_archivo) {
    if (strcasecmp(metodo, "GET") == 0) {
        return;
    }

    if (strcasecmp(metodo, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, datos ? datos : "");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        return;
    }

    if (strcasecmp(metodo, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        return;
    }

    if (strcasecmp(metodo, "HEAD") == 0) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        return;
    }

    if (strcasecmp(metodo, "PUT") == 0) {
        // Si se pasó datos, se hace un PUT con contenido
        if (datos != NULL) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, datos);
            return;
        }

        if (ruta_archivo != NULL) {
            *fp = fopen(ruta_archivo, "rb");
            if (*fp == NULL) {
                fprintf(stderr, "[Error] No se pudo acceder al archivo: %s\n", ruta_archivo);
                curl_easy_cleanup(curl);
                exit(EXIT_FAILURE);
            }

            fseek(*fp, 0, SEEK_END);    // Ir al final del archivo para obtener su tamaño
            *tamano_archivo = ftell(*fp);
            rewind(*fp);

            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl, CURLOPT_READDATA, *fp);
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)(*tamano_archivo));

            char header[256];
            struct curl_slist *headers = NULL;
            snprintf(header, sizeof(header), "Content-Type: %s", obtener_tipo_contenido(ruta_archivo));
            headers = curl_slist_append(headers, header);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            return;
        }

        fprintf(stderr, "[Error] El método PUT requiere datos (-d) o archivo (-f)\n");
        curl_easy_cleanup(curl);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Método HTTP no reconocido: %s\n", metodo);
    exit(EXIT_FAILURE);
}


/*
 * Procesa la respuesta de la solicitud HTTP y maneja su salida.
 * Entrada:
 *   - curl: Puntero al manejador CURL.
 *   - chunk: Puntero a la estructura de memoria que contiene la respuesta.
 *   - archivo_salida: Nombre del archivo donde guardar la respuesta (opcional).
 *   - metodo: Método HTTP utilizado.
 * Salida:
 *   - No retorna valor. Imprime información o guarda archivo según corresponda.
 */
void procesar_respuesta(CURL *curl, struct Memoria *chunk, const char *archivo_salida, const char *metodo) {
    long codigo_http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &codigo_http);      // Obtener el código HTTP
    printf("Código de respuesta HTTP: %ld\n", codigo_http);

    if (!chunk->respuesta || chunk->tamano == 0) {
        printf("No se recibió contenido del servidor.\n");
        return;
    }

    if (archivo_salida && strcmp(metodo, "GET") == 0 && codigo_http == 200) {
        FILE *archivo = fopen(archivo_salida, "w");
        if (!archivo) {
            fprintf(stderr, "No se pudo abrir el archivo '%s' para escribir.\n", archivo_salida);
            return;
        }
        fwrite(chunk->respuesta, 1, chunk->tamano, archivo);
        fclose(archivo);
        printf("Archivo guardado exitosamente en '%s'.\n", archivo_salida);
    } else {
        if (chunk->tamano < 10000) {
            printf("Respuesta del servidor:\n%s\n", chunk->respuesta);
        } else {
            printf("Se recibió un archivo grande (%.2f MB). No se mostrará por pantalla.\n",
                   chunk->tamano / (1024.0 * 1024.0));
        }
    }
}

/*
 * Procesa los argumentos de la línea de comandos para configurar la solicitud.
 * Entrada:
 *   - argc: Número de argumentos.
 *   - argv: Array de argumentos.
 *   - host: Puntero para almacenar el host.
 *   - metodo: Puntero para almacenar el método HTTP.
 *   - ruta: Puntero para almacenar la ruta del recurso.
 *   - datos: Puntero para almacenar datos opcionales de envío.
 *   - archivo_salida: Puntero para almacenar el nombre del archivo de salida.
 * Salida:
 *   - No retorna valor. Llena las variables proporcionadas.
 */
void procesar_argumentos(int argc, char *argv[], char **host, char **metodo, char **ruta, char **datos, char **archivo_salida) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            *host = argv[++i];
        } else if (!*metodo) {
            *metodo = argv[i];
        } else if (!*ruta) {
            *ruta = argv[i];
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            *datos = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            *archivo_salida = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            ruta_archivo = argv[++i];
        } else {
            fprintf(stderr, "Error: Argumento no válido.\n");
            fprintf(stderr, "Uso: ./ClienteHTTP -h <host> <metodo> <ruta> [-d \"datos\"] [-o archivo_salida]\n");
            fprintf(stderr, "Ejemplo: ./ClienteHTTP -h localhost:8080 GET /archivo.txt -o descargado.txt\n");
            exit(1);
        }
    }

    if (!*host || !*metodo || !*ruta) {
        fprintf(stderr, "Error: Faltan parámetros obligatorios. El host, método y ruta son esenciales.\n");
        fprintf(stderr, "Uso: ./ClienteHTTP -h <host> <metodo> <ruta> [-d \"datos\"] [-o archivo_salida]\n");
        fprintf(stderr, "Ejemplo: ./ClienteHTTP -h localhost:8080 GET /archivo.txt -o descargado.txt\n");
        exit(1);
    }
}


int main(int argc, char *argv[]) {
    

    char *host = NULL;
    char *metodo = NULL;
    char *ruta = NULL;
    char *datos = NULL;
    char *archivo_salida = NULL;

    // Procesar argumentos
    procesar_argumentos(argc, argv, &host, &metodo, &ruta, &datos, &archivo_salida);


    // Construir URL
    char url[1024];
    snprintf(url, sizeof(url), "http://%s%s", host, ruta);

    // Inicializar CURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error al inicializar curl\n");
        return 1;
    }

    struct Memoria chunk = { .respuesta = NULL, .tamano = 0 };
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, escribir_respuesta);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    FILE *fp = NULL;
    long tamano_archivo = 0;

    // Configurar solicitud
    configurar_solicitud(curl, metodo, datos, url, &fp, &tamano_archivo);

    // Ejecutar solicitud
    CURLcode res = curl_easy_perform(curl);
    if (fp) fclose(fp);

    if (res != CURLE_OK) {
        fprintf(stderr, "Error en curl: %s\n", curl_easy_strerror(res));
        free(chunk.respuesta);
        curl_easy_cleanup(curl);
        return 1;
    }

    procesar_respuesta(curl, &chunk, archivo_salida, metodo);

    free(chunk.respuesta);
    curl_easy_cleanup(curl);
    return 0;
}
