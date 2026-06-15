// =============================================================================
// main.cpp - Punto de entrada automatizado (SFTP + Parseo + API)
//
// Orquesta las tres fases de forma continua y automatizada: descarga SFTP,
// parseo CSV paralelo, consulta a API REST con caché y volcado a resultados.txt.
// =============================================================================

#include <iostream>
#include <fstream>
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
    // 1. Inicialización del entorno
    fs::create_directories(Config::OUTPUT_DIR);

    if (!Logger::getInstance().openLogFile(Config::LOG_FILE)) {
        std::cerr << "Warning: No se pudo abrir el archivo log global (" << Config::LOG_FILE << ")" << std::endl;
    }

    double start_time = omp_get_wtime();
    LOG_INFO("=== Iniciando aplicación ===");
    std::cout << "=== INICIANDO ===\n" << std::endl;

    int max_threads = omp_get_num_procs();
    std::vector<Transaccion> todasLasTransacciones;

    std::cout << "[+] Cores detectados: " << max_threads << std::endl;

    // Variables de tiempo inicializadas
    double tiempo_sftp = 0.0;
    double tiempo_parseo = 0.0;
    double tiempo_api = 0.0;

    // -------------------------------------------------------------------------
    // FASE 1: Descarga SFTP paralela
    // -------------------------------------------------------------------------
    LOG_INFO("Fase 1: Descargando archivos desde SFTP...");
    std::cout << "[1/3] Iniciando descarga masiva SFTP..." << std::endl;
    
    double sftp_start = omp_get_wtime();
    int descargados = 0, omitidos = 0, fallados = 0;
    
    SFTPParalelo::descargarArchivos(max_threads, descargados, omitidos, fallados);
    
    tiempo_sftp = omp_get_wtime() - sftp_start; // Medición de fase 1
    
    std::cout << "    -> Finalizado en " << tiempo_sftp << " s. Descargados: " << descargados << "\n" << std::endl;

    // -------------------------------------------------------------------------
    // FASE 2: Parseo CSV paralelo
    // -------------------------------------------------------------------------
    LOG_INFO("Fase 2: Parseando archivos CSV...");
    std::cout << "[2/3] Iniciando parseo paralelo..." << std::endl;
    
    double parseo_start = omp_get_wtime();
    ParseoParalelo::procesarArchivos(max_threads, todasLasTransacciones);
    tiempo_parseo = omp_get_wtime() - parseo_start;
    
    std::cout << "TIEMPO EXCLUSIVO DE PARSEO: " << tiempo_parseo << " s.\n" << std::endl;

    // -------------------------------------------------------------------------
    // FASE 3: Consultas API
    // -------------------------------------------------------------------------
    ResultadosAPI resFinal; 

    if (!todasLasTransacciones.empty()) {
        LOG_INFO("Fase 3: Consumiendo API REST");
        std::cout << "[3/3] Iniciando consultas a la API..." << std::endl;

        double api_start = omp_get_wtime();
        resFinal = APIParalelo::procesarConsultas(max_threads, todasLasTransacciones);
        tiempo_api = omp_get_wtime() - api_start;

        std::cout << "TIEMPO EXCLUSIVO DE API: " << tiempo_api << " s.\n" << std::endl;
    }

    // -------------------------------------------------------------------------
    // SALIDA Y REGISTRO
    // -------------------------------------------------------------------------
    double elapsed_global = omp_get_wtime() - start_time;
    
    std::cout << "=== PROCESO FINALIZADO EXITOSAMENTE ===" << std::endl;
    std::cout << "TIEMPO TOTAL GLOBAL: " << elapsed_global << " segundos\n" << std::endl;

    // Escritura en resultados.txt
    std::ofstream archivo(Config::RESULTS_FILE);
    if (archivo.is_open()) {
        archivo << "FEMENINO = " << resFinal.promedioFemenino << "\n";
        archivo << "MASCULINO = " << resFinal.promedioMasculino << "\n";
        archivo << "TIEMPO_SFTP = " << tiempo_sftp << " segundos\n";
        archivo << "TIEMPO_PARSEOCSV = " << tiempo_parseo << " segundos\n";
        archivo << "TIEMPO_API = " << tiempo_api << " segundos\n";
        archivo << "TIEMPO_GLOBAL = " << elapsed_global << " segundos\n";
        archivo.close();
    }

    LOG_INFO("=== Aplicación finalizada. Tiempos -> SFTP: " + std::to_string(tiempo_sftp) + 
             "s | Parseo: " + std::to_string(tiempo_parseo) + 
             "s | API: " + std::to_string(tiempo_api) + 
             "s | Total: " + std::to_string(elapsed_global) + "s ===");
             
    return 0;
}