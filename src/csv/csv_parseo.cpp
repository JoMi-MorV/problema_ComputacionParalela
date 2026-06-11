// =============================================================================
// csv_parseo.cpp - Implementación del parser CSV
//
// Lee archivos delimitados por ';', valida encabezado y filas, descarta
// registros inválidos y retorna un vector de Transaccion.
// =============================================================================

#include "csv_parseo.h"
#include "../utils/logger.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

// Valida que el UUID tenga formato estándar (8-4-4-4-12 caracteres hex)
bool CSVParseo::validarUUID(const std::string& uuid) {
    std::regex uuidRegex(R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)");
    return std::regex_match(uuid, uuidRegex);
}

// Verifica que la cadena contenga solo dígitos (entero positivo)
bool CSVParseo::esEnteroValido(const std::string& str) {
    if (str.empty()) return false;
    return std::all_of(str.begin(), str.end(), ::isdigit);
}

// Verifica que la cadena sea un número decimal válido (ej: 12.5 o 100)
bool CSVParseo::esDecimalValido(const std::string& str) {
    if (str.empty()) return false;
    std::regex decimalRegex(R"(^[0-9]+(\.[0-9]+)?$)");
    return std::regex_match(str, decimalRegex);
}

// Valida formato de fecha ISO: YYYY-MM-DDTHH:MM:SS (19 caracteres)
bool CSVParseo::validarFechaISO(const std::string& fecha) {
    if (fecha.length() != 19) return false;
    if (fecha[4] != '-' || fecha[7] != '-' || fecha[10] != 'T' || fecha[13] != ':' || fecha[16] != ':') {
        return false;
    }
    return true;
}

// Lee un archivo CSV, valida cada fila y retorna las transacciones válidas
std::vector<Transaccion> CSVParseo::leerArchivo(const std::string& rutaArchivo) {
    std::vector<Transaccion> transacciones;
    int filasInvalidas = 0;

    std::ifstream archivo(rutaArchivo);
    if (!archivo.is_open()) {
        LOG_ERROR("No se pudo abrir el archivo: " + rutaArchivo);
        return transacciones;
    }

    std::string linea;

    // Primera línea: encabezado obligatorio
    if (!getline(archivo, linea)) {
        LOG_ERROR("Archivo vacío: " + rutaArchivo);
        return transacciones;
    }

    std::vector<std::string> encabezado = separarLinea(linea);
    if (!validarEncabezado(encabezado)) {
        LOG_ERROR("Encabezado inválido: " + rutaArchivo);
        return transacciones;
    }

    // Procesar cada fila de datos
    while (getline(archivo, linea)) {
        if (linea.empty()) {
            continue;
        }

        std::vector<std::string> columnas = separarLinea(linea);

        // Debe tener exactamente 10 columnas
        if (columnas.size() != 10) {
            filasInvalidas++;
            continue;
        }

        // Rechaza filas con campos vacíos tras limpiar espacios
        bool filaInvalida = false;
        for (size_t i = 0; i < columnas.size(); i++) {
            columnas[i].erase(columnas[i].find_last_not_of(" \r\n\t") + 1);

            if (columnas[i].empty()) {
                filaInvalida = true;
                break;
            }
        }

        if (filaInvalida) {
            filasInvalidas++;
            continue;
        }

        // Autocorrección: agrega ":00" si la fecha viene sin segundos (16 chars)
        if (columnas[0].length() == 16 && columnas[0].find('T') != std::string::npos) {
            columnas[0] += ":00";
        }

        // Validación de tipos y formatos por columna
        if (!validarFechaISO(columnas[0])      ||
            !esEnteroValido(columnas[2])       ||
            !esEnteroValido(columnas[4])       ||
            !esDecimalValido(columnas[5])      ||
            !esDecimalValido(columnas[6])      ||
            !esEnteroValido(columnas[7])       ||
            !validarUUID(columnas[9])) {

            filasInvalidas++;
            continue;
        }

        // Mapeo seguro de columnas al struct Transaccion
        try {
            Transaccion t;
            t.fecha = columnas[0];
            t.canal = columnas[1];
            t.sku = std::stoi(columnas[2]);
            t.producto = columnas[3];
            t.unidades = std::stoi(columnas[4]);
            t.porcentajeDescuento = std::stod(columnas[5]);
            t.montoAplicado = std::stod(columnas[6]);
            t.boleta = std::stol(columnas[7]);
            t.local = columnas[8];
            t.codigoCliente = columnas[9];

            transacciones.push_back(t);

        } catch (...) {
            filasInvalidas++;
        }
    }

    archivo.close();

    LOG_INFO("Archivo procesado: " + rutaArchivo +
             " | Válidos: " + std::to_string(transacciones.size()) +
             " | Omitidos: " + std::to_string(filasInvalidas));

    return transacciones;
}

// Compara el encabezado leído contra las 10 columnas esperadas del CSV
bool CSVParseo::validarEncabezado(const std::vector<std::string>& columnas) {
    std::vector<std::string> esperado = {
        "FECHA", "CANAL", "SKU", "PRODUCTO", "UNIDADES",
        "PORCENTAJE DESCUENTO", "MONTO APLICADO", "BOLETA", "LOCAL", "CODIGO CLIENTE"
    };
    return columnas == esperado;
}

// Separa una línea por ';' y elimina comillas envolventes de cada campo
std::vector<std::string> CSVParseo::separarLinea(const std::string& linea) {
    std::vector<std::string> columnas;
    std::stringstream stream(linea);
    std::string valor;

    while (getline(stream, valor, ';')) {
        if (valor.size() >= 2 && valor.front() == '"' && valor.back() == '"') {
            valor = valor.substr(1, valor.size() - 2);
        }
        columnas.push_back(valor);
    }

    if (!linea.empty() && linea.back() == ';') {
        columnas.push_back("");
    }

    return columnas;
}
