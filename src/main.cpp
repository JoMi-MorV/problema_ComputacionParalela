// =============================================================================
// main.cpp - Punto de entrada del programa (módulo SFTP)
//
// Inicializa el logger, solicita la cantidad de hilos al usuario y ejecuta
// la descarga paralela de archivos CSV desde el servidor SFTP.
// =============================================================================

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <omp.h>

#include "utils/logger.h"
#include "utils/config.h"
#include "paralelismo/sftp_paralelo.h"

namespace fs = std::filesystem;

int main() {
    // Crea la carpeta de salida si no existe
    fs::create_directories(Config::OUTPUT_DIR);

    // Abre el archivo de log global
    if (!Logger::getInstance().openLogFile(Config::LOG_FILE)) {
        std::cerr << "Warning: No se pudo abrir el archivo log global" << std::endl;
    }

    double start_time = omp_get_wtime();
    LOG_INFO("=== Iniciando descarga SFTP ===");

    int max_threads = omp_get_num_procs();
    int user_threads;

    // Solicita al usuario cuántos hilos usar para la descarga
    std::cout << "\n=== DESCARGA SFTP ===\n";
    std::cout << "Procesadores lógicos detectados: " << max_threads << "\n";
    std::cout << "Hilos para descarga (1-" << max_threads << "): ";
    std::cin >> user_threads;
    user_threads = std::clamp(user_threads, 1, max_threads);

    // Ejecuta la descarga paralela y muestra el resumen
    int descargados = 0, omitidos = 0, fallados = 0;
    if (SFTPParalelo::descargarArchivos(user_threads, descargados, omitidos, fallados)) {
        std::cout << "\nDescargados: " << descargados
                  << " | Omitidos ya existentes: " << omitidos
                  << " | Fallidos: " << fallados << std::endl;
    }

    double elapsed = omp_get_wtime() - start_time;
    std::cout << "\nTiempo total de ejecución: " << elapsed << " segundos" << std::endl;

    return 0;
}
