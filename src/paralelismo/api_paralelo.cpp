// =============================================================================
// api_paralelo.cpp - Implementación de consultas API paralelas
//
// Aplica una estrategia de 3 fases: extracción de UUIDs únicos, poblamiento
// masivo de caché vía API 
// =============================================================================

#include "api_paralelo.h"
#include "../api/api.h"
#include "../utils/config.h"
#include "../utils/logger.h"
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <omp.h>
#include <algorithm>


std::unordered_map<std::string, std::string> APIParalelo::cacheGeneros_;

// Orquesta las 3 fases: únicos → API REST → promedios por género
ResultadosAPI APIParalelo::procesarConsultas(int hilos, const std::vector<Transaccion>& transacciones) {
    if (transacciones.empty()) {
        LOG_WARNING("Vector de transacciones vacío en fase API.");
        return ResultadosAPI();
    }

    const size_t totalLineas = transacciones.size();
    omp_set_num_threads(hilos);

    std::cout << "\n=== PROCESANDO API EN PARALELO (Estrategia de 3 Fases) ===\n";
    std::cout << "Ejecutando con " << hilos << " hilos...\n\n";

    cacheGeneros_.clear();
    cacheGeneros_.reserve(totalLineas / 4);

    if (!API::inicializar()) {
        LOG_ERROR("Falló la inicialización de libcurl.");
        return ResultadosAPI();
    }

    // =========================================================================
    // FASE 1: EXTRAER CLIENTES ÚNICOS
    // =========================================================================
    std::cout << "[1/3] Extrayendo clientes únicos de los registros...\n";
    
    std::unordered_set<std::string> setGlobal;
    setGlobal.reserve(totalLineas / 3);

    #pragma omp parallel
    {
        std::unordered_set<std::string> setLocal;
        setLocal.reserve(4096);

        #pragma omp for schedule(static)
        for (size_t i = 0; i < totalLineas; ++i) {
            setLocal.insert(transacciones[i].codigoCliente);
        }

        #pragma omp critical(consolidar_clientes)
        {
            setGlobal.insert(setLocal.begin(), setLocal.end());
        }
    }

    std::vector<std::string> clientesUnicos(setGlobal.begin(), setGlobal.end());
    const size_t totalUnicos = clientesUnicos.size();
    std::cout << "-> Se redujeron " << totalLineas << " líneas a " << totalUnicos << " clientes únicos.\n\n";

    // =========================================================================
    // FASE 2: POBLAR CACHÉ VÍA API
    // =========================================================================
    std::cout << "[2/3] Consultando la API REST...\n";
    if (!API::autenticar()) {
        LOG_ERROR("No se pudo autenticar contra la API REST.");
        return ResultadosAPI();
    }

    std::vector<std::string> generos(totalUnicos);
    const size_t batchSize = static_cast<size_t>(Config::API_BATCH_SIZE);
    const size_t totalBloques = (totalUnicos + batchSize - 1) / batchSize;

    double apiStartTime = omp_get_wtime();
    size_t bloquesCompletados = 0;

    #pragma omp parallel
    {
        #pragma omp for schedule(dynamic, 1)
        for (size_t bloque = 0; bloque < totalBloques; ++bloque) {
            size_t inicio = bloque * batchSize;
            size_t cantidad = std::min(batchSize, totalUnicos - inicio);

            API::obtenerGenerosBloque(clientesUnicos, inicio, cantidad, generos);

            size_t progreso = 0;
            #pragma omp atomic capture
            {
                bloquesCompletados++;
                progreso = bloquesCompletados;
            }

            if (progreso % 10 == 0 || progreso == totalBloques) {
                size_t clientesIndexados = std::min(progreso * batchSize, totalUnicos);
                double tiempoParcial = omp_get_wtime() - apiStartTime;

                #pragma omp critical(console_print_api)
                {
                    std::cout << "   -> API: [" << clientesIndexados << " / " << totalUnicos
                              << "] clientes. Tiempo: " << tiempoParcial << " s\n";
                }
            }
        }

        API::limpiarRecursosHilo();
    }

    double tiempoApi = omp_get_wtime() - apiStartTime;

    for (size_t i = 0; i < totalUnicos; ++i) {
        cacheGeneros_[clientesUnicos[i]] = generos[i];
    }
    std::cout << "-> Caché poblada en " << tiempoApi << " segundos.\n\n";

    // =========================================================================
    // FASE 3: CÁLCULO DE MÉTRICAS
    // =========================================================================
    std::cout << "[3/3] Calculando promedios finales en memoria...\n";
    double totalMontoMasculino = 0.0, totalMontoFemenino = 0.0;
    long totalTransaccionesMasculino = 0, totalTransaccionesFemenino = 0;

    #pragma omp parallel for schedule(static) reduction(+:totalMontoMasculino, totalMontoFemenino, totalTransaccionesMasculino, totalTransaccionesFemenino)
    for (size_t i = 0; i < totalLineas; ++i) {
        const std::string& uuid = transacciones[i].codigoCliente;
        const double monto = transacciones[i].montoAplicado;
        auto it = cacheGeneros_.find(uuid);
        if (it == cacheGeneros_.end()) continue;
        const std::string& genero = it->second;

        if (genero == "MASCULINO") {
            totalMontoMasculino += monto;
            totalTransaccionesMasculino++;
        } else if (genero == "FEMENINO") {
            totalMontoFemenino += monto;
            totalTransaccionesFemenino++;
        }
    }

    ResultadosAPI res;
    res.promedioMasculino = (totalTransaccionesMasculino > 0) ? (totalMontoMasculino / totalTransaccionesMasculino) : 0.0;
    res.promedioFemenino = (totalTransaccionesFemenino > 0) ? (totalMontoFemenino / totalTransaccionesFemenino) : 0.0;

    LOG_INFO("Métricas calculadas en el submódulo de red. Redireccionando a main.");
    return res; 
}
