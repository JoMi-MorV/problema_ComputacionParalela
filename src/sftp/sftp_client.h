// =============================================================================
// sftp_client.h - Cliente SFTP basado en libcurl
//
// Encapsula la conexión, listado y descarga de archivos desde un servidor SFTP.
// Incluye una versión thread-safe de descarga para uso con OpenMP.
// =============================================================================

#ifndef SFTP_CLIENT_H
#define SFTP_CLIENT_H

#include <string>
#include <vector>
#include <curl/curl.h>

// Metadatos de un archivo encontrado en el servidor SFTP
struct SFTPFileInfo {
    std::string filename;
    std::string fullPath;
    long size;
    long mtime;
};

class SFTPClient {
public:
    // Crea el cliente con credenciales y ruta raíz del servidor
    SFTPClient(const std::string& host, int port,
               const std::string& user, const std::string& pass,
               const std::string& rootPath = "/");
    ~SFTPClient();

    // Establece la conexión SFTP con el servidor
    bool connect();

    // Cierra la conexión y libera el handle CURL
    void disconnect();

    // Indica si hay una conexión activa
    bool isConnected() const { return connected_; }

    // Lista archivos remotos que coinciden con un patrón (ej: "reporte_*.csv")
    std::vector<SFTPFileInfo> listFilesByPattern(const std::string& pattern);

    // Descarga un archivo remoto a disco local (uso secuencial, un solo hilo)
    bool downloadFile(const std::string& remotePath, const std::string& localPath);

    // Descarga thread-safe: cada hilo crea su propio handle CURL (para OpenMP)
    static bool downloadFileThreadSafe(
        const std::string& host, int port,
        const std::string& user, const std::string& pass,
        const std::string& remotePath, const std::string& localPath,
        std::string& errorOut
    );

    // Retorna el último mensaje de error registrado
    std::string getLastError() const { return lastError_; }

private:
    CURL* curl_;
    bool connected_;
    std::string host_;
    int port_;
    std::string user_;
    std::string pass_;
    std::string rootPath_;
    std::string lastError_;

    // Callback de libcurl: escribe los bytes recibidos en el archivo local
    static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* stream);

    // Construye la URL SFTP completa a partir de una ruta remota
    std::string buildURL(const std::string& path) const;

    // Compara un nombre de archivo contra un patrón con wildcards (* y ?)
    bool matchesPattern(const std::string& filename, const std::string& pattern) const;
};

#endif // SFTP_CLIENT_H
