// =============================================================================
// api.cpp - Implementación del cliente REST
//
// Realiza autenticación POST para obtener JWT y consultas GET para
// obtener el género de cada cliente desde la API externa.
// =============================================================================

#include "api.h"
#include "../utils/logger.h"
#include <curl/curl.h>
#include <iostream>

// Inicialización del token global (vacío hasta la primera autenticación)
std::string API::tokenGlobal = "";

// Acumula los bytes de la respuesta HTTP en el string indicado por userp
size_t API::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    ((std::string*)userp)->append((char*)contents, totalSize);
    return totalSize;
}

// Envía credenciales a /login/authenticate y extrae el JWT de la respuesta
bool API::autenticarCliente() {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = "https://api.sebastian.cl/cpyd/v1/login/authenticate";
    std::string respuestaJson;
    long httpCode = 0;

    std::string jsonDatos = R"({
        "email": "jvargasm@utem.cl",
        "rut": "21.142.624-1"
    })";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonDatos.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respuestaJson);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        LOG_ERROR("Error en la solicitud de autenticación.");
        return false;
    }

    // Extrae el valor del campo "jwt" del JSON de respuesta
    size_t posJwt = respuestaJson.find("\"jwt\":\"");
    if (posJwt != std::string::npos) {
        size_t inicioToken = posJwt + 7;
        size_t finToken = respuestaJson.find("\"", inicioToken);
        if (finToken != std::string::npos) {
            tokenGlobal = respuestaJson.substr(inicioToken, finToken - inicioToken);
            LOG_INFO("Nuevo Token dinámico generado con éxito de manera reactiva.");
            return true;
        }
    }
    return false;
}

// Consulta GET /person/{uuid} y retorna MASCULINO, FEMENINO o DESCONOCIDO
std::string API::obtenerGeneroCliente(const std::string& uuid) {
    if (tokenGlobal.empty()) {
        if (!autenticarCliente()) return "DESCONOCIDO";
    }

    CURL* curl = curl_easy_init();
    if (!curl) return "DESCONOCIDO";

    std::string url = "https://api.sebastian.cl/cpyd/v1/person/" + uuid;
    std::string respuestaJson;
    long httpCode = 0;

    std::string authHeader = "Authorization: Bearer " + tokenGlobal;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respuestaJson);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "DESCONOCIDO";

    // Señal para que el orquestador renueve el token y reintente
    if (httpCode == 401) {
        return "TOKEN_EXPIRADO";
    }

    if (httpCode != 200) return "DESCONOCIDO";

    // Busca el campo "gender" en el JSON de respuesta
    size_t posGender = respuestaJson.find("\"gender\"");
    if (posGender != std::string::npos) {
        if (respuestaJson.find("MASCULINO", posGender) != std::string::npos) return "MASCULINO";
        if (respuestaJson.find("FEMENINO", posGender) != std::string::npos) return "FEMENINO";
    }

    return "DESCONOCIDO";
}
