// =============================================================================
// csv_parseo.h - Interfaz del parser de archivos CSV
//
// Define la lectura y validación de archivos CSV de ventas.
// Cada fila válida se convierte en un objeto Transaccion.
// =============================================================================

#ifndef CSV_PARSEO_H
#define CSV_PARSEO_H

#include <string>
#include <vector>
#include "transaccion.h"

class CSVParseo {
public:
    // Lee un archivo CSV completo y retorna las transacciones válidas encontradas
    static std::vector<Transaccion> leerArchivo(const std::string& rutaArchivo);

private:
    // Verifica que el encabezado coincida con las 10 columnas esperadas
    static bool validarEncabezado(const std::vector<std::string>& columnas);

    // Divide una línea del CSV usando ';' como separador
    static std::vector<std::string> separarLinea(const std::string& linea);

    // Validaciones de formato por campo
    static bool validarUUID(const std::string& uuid);
    static bool esEnteroValido(const std::string& str);
    static bool esDecimalValido(const std::string& str);
    static bool validarFechaISO(const std::string& fecha);
};

#endif // CSV_PARSEO_H
