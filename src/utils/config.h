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

    // Parámetros de la API REST de clientes
    constexpr const char* API_BASE_URL = "https://api.sebastian.cl/cpyd/v1";
    constexpr const char* API_AUTH_EMAIL = "jvargasm@utem.cl";
    constexpr const char* API_AUTH_RUT = "21.142.624-1";
    constexpr int API_CONNECT_TIMEOUT = 3;     // Segundos para establecer TCP/SSL
    constexpr int API_REQUEST_TIMEOUT = 6;     // Segundos máximos por consulta GET
    constexpr int API_CONCURRENT_REQUESTS = 16; // Peticiones simultáneas por hilo (curl_multi)
    constexpr int API_BATCH_SIZE = 64;         // UUIDs por bloque en la fase paralela
}

#endif // CONFIG_H
