#!/usr/bin/env python3

import argparse
import subprocess
import threading
import sys

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
