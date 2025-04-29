#!/usr/bin/env python3

import argparse
import subprocess
import threading
import sys


# /*
#  * Procesa los argumentos de línea de comandos.
#  * Entrada:
#  *   - sys.argv: Argumentos pasados al script.
#  * Salida:
#  *   - Retorna una tupla (n, comando), donde n es el número de hilos y comando es una lista con el comando a ejecutar.
#  */
def procesar_argumentos():
    if "-n" not in sys.argv:
        print("Error: Debes especificar el número de hilos con el argumento -n.")
        sys.exit(1)

    try:
        n_index = sys.argv.index("-n")
        n = int(sys.argv[n_index + 1])
    except (IndexError, ValueError):
        print("Error: El valor para -n debe ser un número entero.")
        sys.exit(1)

    comando = sys.argv[n_index + 2:]
    if not comando:
        print("Error: Debes proporcionar un comando a ejecutar.")
        sys.exit(1)

    return n, comando

# /*
#  * Ejecuta un comando HTTP y registra el resultado.
#  * Entrada:
#  *   - orden: Lista de strings que representa el comando a ejecutar.
#  *   - registros: Lista compartida donde se almacenan los resultados por hilo.
#  *   - numero: Índice del hilo actual.
#  * Salida:
#  *   - Actualiza la posición correspondiente del hilo en la lista registros con "OK", "503" o "ERROR".
#  */
def ejecutar_cliente_http(orden, registros, numero):
    try:
        resultado = subprocess.run(
            ' '.join(orden), shell=True, capture_output=True, text=True
        )
        respuesta_completa = resultado.stdout + resultado.stderr
        print(f"[Hilo {numero}] Respuesta:\n{respuesta_completa}")

        if "503" in respuesta_completa:
            registros[numero] = "503"
        elif any(codigo in respuesta_completa for codigo in ["200", "201"]):
            registros[numero] = "OK"
        else:
            registros[numero] = "ERROR"
    except Exception:
        registros[numero] = "ERROR"


# /*
#  * Lanza múltiples hilos para ejecutar una prueba de estrés.
#  * Entrada:
#  *   - cantidad: Número de hilos a crear.
#  *   - orden: Lista con el comando a ejecutar en cada hilo.
#  * Salida:
#  *   - Retorna una lista con los resultados de cada hilo (OK, 503 o ERROR).
#  */
def lanzar_simulacion(cantidad, orden):
    registros_resultados = [None] * cantidad
    lista_hilos = []

    for identificador in range(cantidad):
        hilo = threading.Thread(
            target=ejecutar_cliente_http, args=(orden, registros_resultados, identificador)
        )
        hilo.start()
        lista_hilos.append(hilo)

    for hilo in lista_hilos:
        hilo.join()

    return registros_resultados



# /*
#  * Muestra un resumen de los resultados de la prueba de estrés.
#  * Entrada:
#  *   - registros: Lista con los resultados de cada hilo.
#  * Salida:
#  *   - Imprime en pantalla el número de solicitudes exitosas, rechazadas y fallidas.
#  */
def mostrar_resumen(registros):
    exitosos = registros.count("OK")
    rechazados = registros.count("503")
    fallidos = registros.count("ERROR")

    print("\n=== Resultado Final del Stress Test ===")
    print(f"Solicitudes exitosas       : {exitosos}")
    print(f"Solicitudes rechazadas (503): {rechazados}")
    print(f"Solicitudes fallidas        : {fallidos}")
    print("========================================\n")

def main():
    n, comando = procesar_argumentos()

    print(f"\nIniciando prueba con {n} hilos: {' '.join(comando)}\n")

    resultados = lanzar_simulacion(n, comando)
    mostrar_resumen(resultados)

if __name__ == "__main__":
    main()
