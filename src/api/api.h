// =============================================================================
// api.h - Cliente REST para autenticación y consulta de género
//
// Encapsula la conexión HTTP con libcurl: autenticación JWT, consultas GET
// por UUID y consultas en lote vía curl_multi con conexiones persistentes
// por hilo (thread_local) para minimizar latencia de red.
// =============================================================================

#ifndef API_H
#define API_H

#include <string>
#include <vector>

class API {
public:
    // Inicializa libcurl globalmente y la caché compartida de DNS/SSL
    static bool inicializar();

    // Libera recursos globales de libcurl al terminar el programa
    static void finalizar();

    // Obtiene un JWT válido desde /login/authenticate
    static bool autenticar();

    // Invalida el token actual y solicita uno nuevo (401 concurrentes)
    static void invalidarToken();

    // Consulta GET /person/{uuid} y devuelve MASCULINO, FEMENINO o DESCONOCIDO
    static std::string obtenerGeneroCliente(const std::string& uuid);

    // Consulta un bloque de UUIDs en paralelo dentro del hilo (curl_multi)
    static void obtenerGenerosBloque(
        const std::vector<std::string>& uuids,
        size_t inicio,
        size_t cantidad,
        std::vector<std::string>& destino
    );

    // Libera handles curl del hilo actual al salir de una región OpenMP
    static void limpiarRecursosHilo();

    static std::string tokenGlobal;
};

#endif // API_H
