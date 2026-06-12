// =============================================================================
// api_paralelo.cpp - Implementación de consultas API paralelas
//
// Usa una tabla hash compartida para cachear géneros por UUID, evitando
// consultas duplicadas a internet. Calcula el monto promedio por género.
// =============================================================================

#include "api_paralelo.h"
#include "../api/api.h"
#include "../utils/logger.h"
#include <iostream>
#include <unordered_map>
#include <omp.h>
#include <unistd.h>

// Procesa todas las transacciones en paralelo consultando género y acumulando montos
void APIParalelo::procesarConsultas(int hilos, const std::vector<Transaccion>& transacciones) {
    if (transacciones.empty()) {
        std::cout << "\n[!] No hay transacciones para analizar.\n";
        return;
    }

    double start_time = omp_get_wtime();
    size_t totalLineas = transacciones.size();
    omp_set_num_threads(hilos);

    std::cout << "\n=== PROCESANDO API EN PARALELO (Doble Verificación Asíncrona) ===\n";
    std::cout << "Ejecutando con " << hilos << " hilos a máxima velocidad...\n\n";

    double totalMontoMasculino = 0.0;
    double totalMontoFemenino = 0.0;
    long totalTransaccionesMasculino = 0;
    long totalTransaccionesFemenino = 0;
    size_t lineasProcesadasGlobal = 0;

    // Cache compartida: UUID -> género (o "PENDIENTE" mientras se consulta)
    std::unordered_map<std::string, std::string> tablaHashGlobal;

    #pragma omp parallel reduction(+:totalMontoMasculino, totalMontoFemenino, totalTransaccionesMasculino, totalTransaccionesFemenino)
    {
        double montoMascLocal = 0.0;
        double montoFemLocal = 0.0;
        long contMascLocal = 0;
        long contFemLocal = 0;

        #pragma omp for schedule(dynamic, 50)
        for (size_t i = 0; i < totalLineas; i++) {
            std::string uuid = transacciones[i].codigoCliente;
            double monto = transacciones[i].montoAplicado;
            std::string genero = "";
            bool consultarInternet = false;

            // Paso 1: buscar en cache RAM; si no existe, reservar como PENDIENTE
            #pragma omp critical(hash_access)
            {
                if (tablaHashGlobal.find(uuid) != tablaHashGlobal.end()) {
                    genero = tablaHashGlobal[uuid];
                } else {
                    tablaHashGlobal[uuid] = "PENDIENTE";
                    consultarInternet = true;
                }
            }

            // Paso 2: consulta HTTP fuera del candado (paralelismo real)
            if (consultarInternet) {
                genero = API::obtenerGeneroCliente(uuid);

                // Renueva token si expiró y reintenta la consulta
                if (genero == "TOKEN_EXPIRADO") {
                    #pragma omp critical(token_emergency)
                    {
                        API::autenticarCliente();
                    }
                    genero = API::obtenerGeneroCliente(uuid);
                }

                #pragma omp critical(hash_access)
                {
                    tablaHashGlobal[uuid] = genero;
                }
            }
            // Otro hilo ya reservó este UUID: esperar a que termine la consulta
            else if (genero == "PENDIENTE") {
                bool listo = false;
                while (!listo) {
                    usleep(5000);
                    #pragma omp critical(hash_access)
                    {
                        if (tablaHashGlobal[uuid] != "PENDIENTE") {
                            genero = tablaHashGlobal[uuid];
                            listo = true;
                        }
                    }
                }
            }

            // Paso 3: contador de progreso y acumulación de montos por género
            size_t progresoActual = 0;
            #pragma omp atomic capture
            {
                lineasProcesadasGlobal++;
                progresoActual = lineasProcesadasGlobal;
            }

            if (progresoActual % 1000 == 0 || progresoActual == totalLineas) {
                #pragma omp critical(console_print)
                {
                    std::cout << "[" << progresoActual << " / " << totalLineas << "] Procesados con éxito.\n";
                }
            }

            if (genero == "MASCULINO") {
                montoMascLocal += monto;
                contMascLocal++;
            } else if (genero == "FEMENINO") {
                montoFemLocal += monto;
                contFemLocal++;
            }
        }

        totalMontoMasculino += montoMascLocal;
        totalMontoFemenino += montoFemLocal;
        totalTransaccionesMasculino += contMascLocal;
        totalTransaccionesFemenino += contFemLocal;
    }

    // Cálculo de promedios y salida según formato de la rúbrica
    double promedioMasculino = (totalTransaccionesMasculino > 0) ? (totalMontoMasculino / totalTransaccionesMasculino) : 0.0;
    double promedioFemenino = (totalTransaccionesFemenino > 0) ? (totalMontoFemenino / totalTransaccionesFemenino) : 0.0;
    double elapsed = omp_get_wtime() - start_time;

    std::cout << "\n5. Cálculo de Métricas:";
    std::cout << "\n- Promedio de compras por género:";
    std::cout << "\n  - FEMENINO = " << promedioFemenino;
    std::cout << "\n  - MASCULINO = " << promedioMasculino;
    std::cout << "\n";
    std::cout << "\n6. Salida:";
    std::cout << "\n- FEMENINO = " << promedioFemenino;
    std::cout << "\n- MASCULINO = " << promedioMasculino;
    std::cout << "\n- TIEMPO = " << elapsed << " segundos\n";
}
