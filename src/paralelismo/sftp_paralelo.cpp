// =============================================================================
// sftp_paralelo.cpp - Implementación de la descarga SFTP paralela
//
// Lista los archivos remotos con un cliente SFTP secuencial y luego
// distribuye las descargas entre hilos OpenMP con schedule(dynamic).
// =============================================================================

#include "sftp_paralelo.h"
#include "../sftp/sftp_client.h"
#include "../utils/config.h"
#include "../utils/logger.h"
#include <filesystem>
#include <omp.h>

namespace fs = std::filesystem;

// Orquesta la descarga masiva: conexión, listado y descarga paralela por hilo
bool SFTPParalelo::descargarArchivos(int hilos, int& descargados, int& omitidos, int& fallados) {
    SFTPClient sftp(
        Config::SFTP_HOST, Config::SFTP_PORT,
        Config::SFTP_USER, Config::SFTP_PASS, Config::SFTP_ROOT
    );

    if (!sftp.connect()) {
        LOG_ERROR("No se pudo conectar al servidor SFTP");
        return false;
    }

    auto files = sftp.listFilesByPattern(Config::FILE_PATTERN);
    if (files.empty()) {
        LOG_WARNING("No hay archivos en el servidor SFTP");
        sftp.disconnect();
        return true;
    }

    omp_set_num_threads(hilos);

    #pragma omp parallel for schedule(dynamic) reduction(+:descargados, omitidos, fallados)
    for (size_t i = 0; i < files.size(); i++) {
        std::string localPath = std::string(Config::OUTPUT_DIR) + files[i].filename;

        // Omite archivos que ya fueron descargados previamente
        if (fs::exists(localPath) && fs::file_size(localPath) > 0) {
            omitidos++;
            continue;
        }

        std::string error;
        if (SFTPClient::downloadFileThreadSafe(
                Config::SFTP_HOST, Config::SFTP_PORT, Config::SFTP_USER, Config::SFTP_PASS,
                files[i].fullPath, localPath, error)) {
            descargados++;
        } else {
            fallados++;
            #pragma omp critical
            {
                LOG_ERROR(error);
            }
        }
    }

    sftp.disconnect();
    return true;
}