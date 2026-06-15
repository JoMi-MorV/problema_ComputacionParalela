// =============================================================================
// api_paralelo.h - Orquestador de consultas API y cálculo de métricas
//
// Expone la interfaz para enriquecer transacciones con género vía API REST
// usando OpenMP: extracción de UUIDs únicos, poblamiento de caché en paralelo
// y cálculo de promedios por género sin contención de candados.
// =============================================================================

#ifndef API_PARALELO_H
#define API_PARALELO_H

#include <vector>
#include <string>
#include <unordered_map>
#include "../csv/transaccion.h"

// Definimos la estructura limpia, SIN tildes
struct ResultadosAPI {
    double promedioFemenino = 0.0;
    double promedioMasculino = 0.0;
};

class APIParalelo {
public:
    // Cambiamos el tipo de retorno aquí también en la cabecera
    static ResultadosAPI procesarConsultas(int hilos, const std::vector<Transaccion>& transacciones);
    
private:
    static std::unordered_map<std::string, std::string> cacheGeneros_;
};

#endif
