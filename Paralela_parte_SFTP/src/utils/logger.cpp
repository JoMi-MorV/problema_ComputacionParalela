// =============================================================================
// logger.cpp - Implementación del sistema de bitácora
//
// Formatea cada mensaje con timestamp y nivel, y lo escribe en consola
// y en el archivo de log de forma sincronizada con mutex.
// =============================================================================

#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

// Retorna la instancia única del logger
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

// Traduce el nivel de log a una cadena de texto
std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::DEBUG:   return "DEBUG";
        default:                return "UNKNOWN";
    }
}

// Escribe un mensaje formateado en consola y en el archivo de log
void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << "] "
       << "[" << levelToString(level) << "] " << message;

    std::string formatted = ss.str();
    std::cout << formatted << std::endl;

    if (logFile_.is_open()) {
        logFile_ << formatted << std::endl;
        logFile_.flush();
    }
}

// Abre el archivo de log en modo append; retorna false si falla
bool Logger::openLogFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    logFile_.open(filepath, std::ios::app);
    return logFile_.is_open();
}

// Cierra el archivo de log de forma segura
void Logger::closeLogFile() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.close();
    }
}
