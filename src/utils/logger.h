// =============================================================================
// logger.h - Sistema de registro de eventos (bitácora)
//
// Singleton thread-safe que escribe mensajes en consola y en un archivo de log.
// Usado por todos los módulos para registrar info, advertencias y errores.
// =============================================================================

#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

enum class LogLevel { INFO, WARNING, ERROR, DEBUG };

class Logger {
public:
    // Obtiene la única instancia del logger (patrón Singleton)
    static Logger& getInstance();

    // Registra un mensaje con el nivel indicado
    void log(LogLevel level, const std::string& message);

    // Atajos para cada nivel de log
    void info(const std::string& msg)    { log(LogLevel::INFO, msg); }
    void warning(const std::string& msg) { log(LogLevel::WARNING, msg); }
    void error(const std::string& msg)   { log(LogLevel::ERROR, msg); }
    void debug(const std::string& msg)   { log(LogLevel::DEBUG, msg); }

    // Abre el archivo de log en modo append
    bool openLogFile(const std::string& filepath);

    // Cierra el archivo de log si está abierto
    void closeLogFile();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger() { closeLogFile(); }

    std::ofstream logFile_;
    std::mutex mutex_;

    // Convierte el enum LogLevel a texto legible
    std::string levelToString(LogLevel level);
};

#define LOG_INFO(msg)    Logger::getInstance().info(msg)
#define LOG_WARNING(msg) Logger::getInstance().warning(msg)
#define LOG_ERROR(msg)   Logger::getInstance().error(msg)
#define LOG_DEBUG(msg)   Logger::getInstance().debug(msg)

#endif // LOGGER_H
