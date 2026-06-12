// =============================================================================
// main.cpp - Punto de entrada del programa completo (SFTP + Parseo + API)
//
// Orquesta las tres fases: descarga SFTP, parseo CSV paralelo
// y consultas API paralelas con cálculo de métricas por género.
// =============================================================================

#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <omp.h>

#include "utils/logger.h"
#include "utils/config.h"
#include "csv/transaccion.h"
#include "paralelismo/sftp_paralelo.h"
#include "paralelismo/parseo_paralelo.h"
#include "paralelismo/api_paralelo.h"

namespace fs = std::filesystem;

int main() {
    // Crea la carpeta de salida si no existe
    fs::create_directories(Config::OUTPUT_DIR);

    // Abre el archivo de log global
    if (!Logger::getInstance().openLogFile(Config::LOG_FILE)) {
        std::cerr << "Warning: No se pudo abrir el archivo log global" << std::endl;
    }

    double start_time = omp_get_wtime();
    LOG_INFO("=== Iniciando aplicación Cruz Morada ===");

    // Menú principal: el usuario elige qué fases ejecutar
    int opcion;
    std::cout << "\n=== MENU ===\n";
    std::cout << "1. Descargar CSV\n";
    std::cout << "2. Parsear CSV\n";
    std::cout << "3. Descargar + Parsear\n";
    std::cout << "Seleccione opcion: ";
    std::cin >> opcion;

    int max_threads = omp_get_num_procs();
    std::vector<Transaccion> todasLasTransacciones;

    // -------------------------------------------------------------------------
    // FASE 1: Descarga SFTP paralela (mismo módulo que Paralela_parte_SFTP)
    // -------------------------------------------------------------------------
    if (opcion == 1 || opcion == 3) {
        LOG_INFO("Fase 1: Descargando archivos...");

        int user_threads;
        std::cout << "\nHilos para Descarga (1-" << max_threads << "): ";
        std::cin >> user_threads;
        user_threads = std::clamp(user_threads, 1, max_threads);

        int descargados = 0, omitidos = 0, fallados = 0;
        if (SFTPParalelo::descargarArchivos(user_threads, descargados, omitidos, fallados)) {
            std::cout << "\nDescargados: " << descargados
                      << " | Omitidos ya existentes: " << omitidos
                      << " | Fallidos: " << fallados << std::endl;
        }
    }

    // -------------------------------------------------------------------------
    // FASE 2: Parseo CSV paralelo (mismo módulo que Paralela_2)
    // -------------------------------------------------------------------------
    if (opcion == 2 || opcion == 3) {
        LOG_INFO("Fase 2: Parseando archivos CSV...");

        int user_threads_parseo;
        std::cout << "\n=== CONFIGURACION PARSEO ===\n";
        std::cout << "Procesadores lógicos detectados: " << max_threads << "\n";
        std::cout << "Ingrese cantidad de hilos: ";
        std::cin >> user_threads_parseo;
        user_threads_parseo = std::clamp(user_threads_parseo, 1, max_threads);

        ParseoParalelo::procesarArchivos(user_threads_parseo, todasLasTransacciones);

        // ---------------------------------------------------------------------
        // FASE 3: Consultas API paralelas y cálculo de métricas (módulo propio)
        // ---------------------------------------------------------------------
        if (!todasLasTransacciones.empty()) {
            int user_threads_api;
            std::cout << "\n=== CONFIGURACION API ===\n";
            std::cout << "Ingrese cantidad de hilos para la API (Modo Test): ";
            std::cin >> user_threads_api;
            user_threads_api = std::clamp(user_threads_api, 1, max_threads);

            APIParalelo::procesarConsultas(user_threads_api, todasLasTransacciones);
        }
    }

    double elapsed = omp_get_wtime() - start_time;
    std::cout << "\nTiempo total de ejecución: " << elapsed << " segundos" << std::endl;

    return 0;
}
