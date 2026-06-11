// =============================================================================
// parseo_paralelo.h - Orquestador de parseo CSV en paralelo
//
// Expone la interfaz para procesar masivamente archivos CSV locales
// usando OpenMP con un número configurable de hilos.
// =============================================================================

#ifndef PARSEO_PARALELO_H
#define PARSEO_PARALELO_H

#include <vector>
#include <string>
#include "../csv/transaccion.h"

class ParseoParalelo {
public:
    // Busca CSV en output/, los parsea en paralelo y acumula todas las transacciones
    static void procesarArchivos(int hilos, std::vector<Transaccion>& todasLasTransacciones);
};

#endif // PARSEO_PARALELO_H
