#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// Estructura básica para acumular respuesta
struct Response {
    char *data;
    size_t size;
};

// Callback simplificado: sólo cuenta bytes y descarta datos
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    struct Response *resp = (struct Response *)userdata;
    // Aquí podrías realloc y guardar, pero lo dejamos para implementación futura
    printf("[DEBUG] Recibidos %zu bytes\n", total);
    resp->size += total;
    return total;
}

// Versión primitiva: siempre devuelve este tipo
const char* get_content_type(const char *filename) {
    // TODO: mapear extensiones mínimas o dejar fijo
    (void)filename;
    return "application/octet-stream";
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <host:puerto> <METODO> <ruta>\n", argv[0]);
        return 1;
    }

    char *host      = argv[1];  // ej. "localhost:8080"
    char *method    = argv[2];  // GET, POST, etc.
    char *path      = argv[3];  // ej. "/archivo.txt"
    char  url[512];

    snprintf(url, sizeof(url), "http://%s%s", host, path);

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error al inicializar curl\n");
        return 1;
    }

    struct Response resp = { .data = NULL, .size = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    // Sólo implementamos GET y dejamos marcados los demás
    if (strcasecmp(method, "GET") == 0) {
        // GET es por defecto
    }
    else if (strcasecmp(method, "POST") == 0) {
        // TODO: manejar datos de POST
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ""); // datos por implementar
    }
    else {
        fprintf(stderr, "Método '%s' no soportado (sólo GET/POST en esta versión)\n", method);
        curl_easy_cleanup(curl);
        return 1;
    }

    // Ejecutar
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Error en curl: %s\n", curl_easy_strerror(res));
    } else {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        printf("HTTP %ld recibido, total de bytes: %zu\n", code, resp.size);
        // TODO: guardar en archivo si se desea
    }

    // Limpieza
    curl_easy_cleanup(curl);
    free(resp.data);  // aunque no se reallocó nada aquí
    return (res == CURLE_OK) ? 0 : 1;
}

