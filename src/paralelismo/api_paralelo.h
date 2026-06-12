// =============================================================================
// api_paralelo.h - Orquestador de consultas API en paralelo
//
// Procesa transacciones en paralelo con OpenMP, consulta el género de cada
// cliente vía API REST y calcula métricas de compra promedio por género.
// =============================================================================

#ifndef API_PARALELO_H
#define API_PARALELO_H

#include "../csv/transaccion.h"
#include <vector>

class APIParalelo {
public:
    // Consulta géneros en paralelo y calcula promedios de monto por MASCULINO/FEMENINO
    static void procesarConsultas(int hilos, const std::vector<Transaccion>& transacciones);
};

#endif
