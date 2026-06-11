// =============================================================================
// sftp_client.cpp - Implementación del cliente SFTP
//
// Gestiona conexión, listado de directorios y descarga de archivos usando
// libcurl. Soporta reintentos con backoff exponencial ante fallos de red.
// =============================================================================

#include "sftp_client.h"
#include "utils/logger.h"
#include "utils/config.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <fnmatch.h>
#include <thread>
#include <chrono>
#include <filesystem>

// Callback de libcurl: escribe los datos recibidos en el archivo de salida
size_t SFTPClient::writeCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    std::ofstream* file = static_cast<std::ofstream*>(stream);
    file->write(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

// Inicializa libcurl global y guarda las credenciales de conexión
SFTPClient::SFTPClient(const std::string& host, int port,
                       const std::string& user, const std::string& pass,
                       const std::string& rootPath)
    : curl_(nullptr), connected_(false),
      host_(host), port_(port), user_(user), pass_(pass), rootPath_(rootPath) {
    curl_global_init(CURL_GLOBAL_ALL);
}

// Cierra la conexión y libera los recursos de libcurl
SFTPClient::~SFTPClient() {
    disconnect();
    curl_global_cleanup();
}

// Conecta al servidor SFTP y verifica acceso al directorio raíz
bool SFTPClient::connect() {
    if (connected_) return true;

    curl_ = curl_easy_init();
    if (!curl_) {
        lastError_ = "Failed to initialize CURL";
        LOG_ERROR(lastError_);
        return false;
    }

    std::string url = "sftp://" + host_ + ":" + std::to_string(port_);

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_USERPWD, (user_ + ":" + pass_).c_str());
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, Config::CURL_CONNECT_TIMEOUT);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, Config::CURL_TRANSFER_TIMEOUT);
    curl_easy_setopt(curl_, CURLOPT_LOW_SPEED_LIMIT, 512L);
    curl_easy_setopt(curl_, CURLOPT_LOW_SPEED_TIME, 15L);
    curl_easy_setopt(curl_, CURLOPT_USE_SSL, CURLUSESSL_TRY);
    curl_easy_setopt(curl_, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_SINGLECWD);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl_, CURLOPT_DIRLISTONLY, 1L);

    // Prueba de conexión listando el directorio raíz
    std::string testUrl = url + rootPath_;
    curl_easy_setopt(curl_, CURLOPT_URL, testUrl.c_str());

    CURLcode res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        lastError_ = "Connection failed: " + std::string(curl_easy_strerror(res));
        LOG_ERROR(lastError_);
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
        return false;
    }

    connected_ = true;
    LOG_INFO("Conexión SFTP establecida con " + host_);
    return true;
}

// Libera el handle CURL y marca la conexión como cerrada
void SFTPClient::disconnect() {
    if (curl_) {
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
    }
    connected_ = false;
    LOG_DEBUG("Conexión SFTP cerrada");
}

// Arma la URL SFTP completa combinando host, puerto y ruta remota
std::string SFTPClient::buildURL(const std::string& path) const {
    std::string cleanPath = path;
    if (cleanPath.empty() || cleanPath[0] != '/') {
        cleanPath = rootPath_ + cleanPath;
    }
    return "sftp://" + host_ + ":" + std::to_string(port_) + cleanPath;
}

// Verifica si un nombre de archivo coincide con el patrón dado (fnmatch)
bool SFTPClient::matchesPattern(const std::string& filename, const std::string& pattern) const {
    return fnmatch(pattern.c_str(), filename.c_str(), 0) == 0;
}

// Lista archivos del directorio raíz que coinciden con el patrón indicado
std::vector<SFTPFileInfo> SFTPClient::listFilesByPattern(const std::string& pattern) {
    std::vector<SFTPFileInfo> result;

    if (!connected_ || !curl_) {
        lastError_ = "Not connected to SFTP";
        return result;
    }

    std::string listUrl = buildURL(rootPath_);
    curl_easy_setopt(curl_, CURLOPT_URL, listUrl.c_str());
    curl_easy_setopt(curl_, CURLOPT_DIRLISTONLY, 1L);

    std::stringstream ss;
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t size, size_t nmemb, void* data) -> size_t {
        static_cast<std::stringstream*>(data)->write(static_cast<char*>(ptr), size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &ss);

    CURLcode res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        lastError_ = "Failed to list directory: " + std::string(curl_easy_strerror(res));
        LOG_ERROR(lastError_);
        return result;
    }

    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (line.empty()) continue;

        if (matchesPattern(line, pattern)) {
            SFTPFileInfo info;
            info.filename = line;
            info.fullPath = rootPath_ + line;
            info.size = 0;
            info.mtime = 0;
            result.push_back(info);
            LOG_DEBUG("Archivo encontrado: " + info.filename);
        }
    }

    LOG_INFO("Se encontraron " + std::to_string(result.size()) + " archivos que coinciden con '" + pattern + "'");
    return result;
}

// Descarga un archivo remoto a una ruta local con reintentos y backoff exponencial
bool SFTPClient::downloadFile(const std::string& remotePath, const std::string& localPath) {
    if (!connected_ || !curl_) {
        lastError_ = "Not connected to SFTP";
        LOG_ERROR(lastError_);
        return false;
    }

    if (std::filesystem::exists(localPath)) {
        std::filesystem::remove(localPath);
    }

    std::ofstream outFile(localPath, std::ios::binary);
    if (!outFile.is_open()) {
        lastError_ = "Cannot open local file for writing: " + localPath;
        LOG_ERROR(lastError_);
        return false;
    }

    std::string downloadUrl = buildURL(remotePath);
    curl_easy_setopt(curl_, CURLOPT_URL, downloadUrl.c_str());
    curl_easy_setopt(curl_, CURLOPT_DIRLISTONLY, 0L);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &outFile);
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, Config::CURL_CONNECT_TIMEOUT);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, Config::CURL_TRANSFER_TIMEOUT);
    curl_easy_setopt(curl_, CURLOPT_LOW_SPEED_LIMIT, 512L);
    curl_easy_setopt(curl_, CURLOPT_LOW_SPEED_TIME, 15L);
    curl_easy_setopt(curl_, CURLOPT_FAILONERROR, 1L);

    for (int attempt = 1; attempt <= Config::MAX_RETRIES; ++attempt) {
        CURLcode res = curl_easy_perform(curl_);
        if (res == CURLE_OK) {
            outFile.close();
            LOG_INFO("Descargado: " + remotePath + " -> " + localPath);
            return true;
        }

        LOG_WARNING("Intento " + std::to_string(attempt) + " fallido: " +
                    curl_easy_strerror(res));

        if (attempt < Config::MAX_RETRIES) {
            int delay = Config::RETRY_BASE_DELAY_SEC * (1 << (attempt - 1));
            LOG_DEBUG("Esperando " + std::to_string(delay) + "s antes del siguiente intento...");
            std::this_thread::sleep_for(std::chrono::seconds(delay));
        }
    }

    lastError_ = "Download failed after " + std::to_string(Config::MAX_RETRIES) + " attempts";
    LOG_ERROR(lastError_);

    outFile.close();
    if (std::filesystem::exists(localPath)) {
        std::filesystem::remove(localPath);
    }

    return false;
}

// Descarga thread-safe: cada hilo usa su propio handle CURL independiente
bool SFTPClient::downloadFileThreadSafe(
    const std::string& host, int port,
    const std::string& user, const std::string& pass,
    const std::string& remotePath, const std::string& localPath,
    std::string& errorOut)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        errorOut = "CURL handle initialization failed";
        return false;
    }

    if (std::filesystem::exists(localPath)) {
        std::filesystem::remove(localPath);
    }

    std::ofstream outFile(localPath, std::ios::binary);
    if (!outFile.is_open()) {
        errorOut = "Cannot open local file: " + localPath;
        curl_easy_cleanup(curl);
        return false;
    }

    std::string url = "sftp://" + host + ":" + std::to_string(port) + remotePath;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD, (user + ":" + pass).c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, Config::CURL_CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, Config::CURL_TRANSFER_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 512L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);

    for (int attempt = 1; attempt <= Config::MAX_RETRIES; ++attempt) {
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            outFile.close();
            curl_easy_cleanup(curl);
            return true;
        }

        LOG_WARNING("Intento " + std::to_string(attempt) + " fallido para " + remotePath + ": " + curl_easy_strerror(res));

        if (attempt < Config::MAX_RETRIES) {
            int delay = Config::RETRY_BASE_DELAY_SEC * (1 << (attempt - 1));
            std::this_thread::sleep_for(std::chrono::seconds(delay));
        }
    }

    errorOut = "Download failed after " + std::to_string(Config::MAX_RETRIES) + " attempts";
    outFile.close();
    if (std::filesystem::exists(localPath)) std::filesystem::remove(localPath);
    curl_easy_cleanup(curl);
    return false;
}
