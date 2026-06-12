// =============================================================================
// api.h - Cliente REST para consultas a la API de personas
//
// Gestiona autenticación JWT y consulta del género de un cliente por UUID.
// Usa libcurl para las peticiones HTTP.
// =============================================================================

#ifndef API_H
#define API_H

#include <string>
#include <stddef.h>

class API {
private:
    // Callback de libcurl: acumula la respuesta HTTP en un string
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    // Token JWT compartido entre todas las consultas de la sesión
    static std::string tokenGlobal;

public:
    // Autentica con email/rut y guarda el token JWT en memoria
    static bool autenticarCliente();

    // Consulta el género (MASCULINO/FEMENINO) de un cliente por su UUID
    static std::string obtenerGeneroCliente(const std::string& uuid);
};

#endif
