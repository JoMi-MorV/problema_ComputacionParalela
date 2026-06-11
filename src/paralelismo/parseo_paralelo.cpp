// =============================================================================
// parseo_paralelo.cpp - Implementación del parseo CSV paralelo
//
// Descubre archivos .csv en output/, los distribuye entre hilos OpenMP
// y consolida todas las transacciones válidas en un vector global.
// =============================================================================

#include "parseo_paralelo.h"
#include "../csv/csv_parseo.h"
#include "../utils/config.h"
#include "../utils/logger.h"
#include <filesystem>
#include <iostream>
#include <omp.h>

namespace fs = std::filesystem;

// Orquesta el parseo masivo: descubrimiento de archivos, procesamiento paralelo y resumen
void ParseoParalelo::procesarArchivos(int hilos, std::vector<Transaccion>& todasLasTransacciones) {
    std::vector<std::string> rutasCSV;

    // Recolecta todas las rutas de archivos .csv en el directorio de salida
    for (const auto& archivo : fs::directory_iterator(Config::OUTPUT_DIR)) {
        if (archivo.path().extension() == ".csv") {
            rutasCSV.push_back(archivo.path().string());
        }
    }

    if (rutasCSV.empty()) {
        LOG_WARNING("No existen archivos CSV en la carpeta " + std::string(Config::OUTPUT_DIR));
        return;
    }

    omp_set_num_threads(hilos);
    LOG_INFO("Parseo en paralelo configurado con " + std::to_string(hilos) + " hilos");

    int archivosParseados = 0;
    int totalArchivosVaciosMalos = 0;
    std::vector<std::string> archivosConErrores;

    #pragma omp parallel for schedule(dynamic) reduction(+:archivosParseados, totalArchivosVaciosMalos)
    for (size_t i = 0; i < rutasCSV.size(); i++) {
        std::vector<Transaccion> transaccionesLocales = CSVParseo::leerArchivo(rutasCSV[i]);

        if (transaccionesLocales.empty()) {
            totalArchivosVaciosMalos++;
        }

        // Sección crítica: fusionar resultados locales al vector global
        #pragma omp critical
        {
            archivosParseados++;
            if (transaccionesLocales.empty()) {
                archivosConErrores.push_back(rutasCSV[i] + " (Vacío o Cabecera Inválida)");
            }
            todasLasTransacciones.insert(
                todasLasTransacciones.end(),
                transaccionesLocales.begin(),
                transaccionesLocales.end()
            );
        }
    }

    LOG_INFO("Archivos CSV procesados exitosamente: " + std::to_string(archivosParseados));
    LOG_INFO("Total transacciones cargadas en memoria: " + std::to_string(todasLasTransacciones.size()));

    // Resumen final en consola
    std::cout << "\n=========================================\n";
    std::cout << "        RESUMEN DEL PARSEO CSV           \n";
    std::cout << "=========================================\n";
    std::cout << "CSV procesados con éxito: " << archivosParseados - totalArchivosVaciosMalos << "\n";
    std::cout << "CSV vacíos o corruptos:   " << totalArchivosVaciosMalos << "\n";
    std::cout << "Transacciones totales:    " << todasLasTransacciones.size() << "\n";

    if (!archivosConErrores.empty()) {
        std::cout << "\n[!] Archivos que fallaron o venían vacíos:\n";
        for (const auto& arch : archivosConErrores) {
            std::cout << "  -> " << arch << "\n";
        }
    }
    std::cout << "=========================================\n";
}
