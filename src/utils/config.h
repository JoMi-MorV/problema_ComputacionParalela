// =============================================================================
// config.h - Constantes globales de configuración del proyecto
//
// Centraliza credenciales SFTP, rutas de salida y parámetros de conexión
// (timeouts y reintentos) usados por el cliente y la descarga paralela.
// =============================================================================

#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace Config {
    // Credenciales y parámetros del servidor SFTP
    constexpr const char* SFTP_HOST = "137.184.45.251";
    constexpr const int SFTP_PORT = 22;
    constexpr const char* SFTP_USER = "utem";
    constexpr const char* SFTP_PASS = "CPyD.2026";
    constexpr const char* SFTP_ROOT = "/";
    constexpr const char* FILE_PATTERN = "reporte_*.csv";

    // Rutas locales donde se guardan archivos y logs
    constexpr const char* OUTPUT_DIR = "output/";
    constexpr const char* LOG_FILE = "output/log.txt";
    constexpr const char* RESULTS_FILE = "output/resultados.txt"; // Resultados de métricas API

    // Timeouts y política de reintentos para transferencias SFTP
    constexpr int CURL_CONNECT_TIMEOUT = 10;   // Segundos para el handshake SSH
    constexpr int CURL_TRANSFER_TIMEOUT = 120; // Segundos máximos por transferencia
    constexpr int MAX_RETRIES = 5;
    constexpr int RETRY_BASE_DELAY_SEC = 2;    // Base del backoff exponencial
}

#endif // CONFIG_H
