// =============================================================================
// api.cpp - Implementación del cliente REST
//
// Realiza autenticación POST para obtener JWT y consultas GET (individuales o
// en lote con curl_multi) para obtener el género de cada cliente. Cada hilo
// OpenMP mantiene su propio pool de conexiones persistentes (Keep-Alive).
// =============================================================================

#include "api.h"
#include "../utils/config.h"
#include "../utils/logger.h"
#include <curl/curl.h>
#include <algorithm>
#include <mutex>
#include <omp.h>

std::string API::tokenGlobal = "";

// --- Recursos globales compartidos entre hilos (DNS + sesiones SSL) ---
static CURLSH* shareHandle = nullptr;
static std::mutex shareMutex;
static bool curlInicializado = false;

static void curlShareLock(CURL*, curl_lock_data, curl_lock_access, void*) {
    shareMutex.lock();
}

static void curlShareUnlock(CURL*, curl_lock_data, void*) {
    shareMutex.unlock();
}

static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// --- Estado de red exclusivo por hilo OpenMP ---
struct PeticionContexto {
    std::string respuesta;
    long httpCode = 0;
};

struct ContextoHilo {
    CURLM* multi = nullptr;
    std::vector<CURL*> poolEasy;
    struct curl_slist* headers = nullptr;
    std::string tokenEnHeaders;

    void configurarEasy(CURL* easy) const {
        curl_easy_setopt(easy, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, (long)Config::API_CONNECT_TIMEOUT);
        curl_easy_setopt(easy, CURLOPT_TIMEOUT, (long)Config::API_REQUEST_TIMEOUT);
        curl_easy_setopt(easy, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(easy, CURLOPT_TCP_KEEPIDLE, 15L);
        curl_easy_setopt(easy, CURLOPT_TCP_KEEPINTVL, 2L);
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        if (shareHandle != nullptr) {
            curl_easy_setopt(easy, CURLOPT_SHARE, shareHandle);
        }
    }

    void asegurarInicializado() {
        if (multi != nullptr) return;

        multi = curl_multi_init();
        poolEasy.reserve(Config::API_CONCURRENT_REQUESTS);

        for (int i = 0; i < Config::API_CONCURRENT_REQUESTS; ++i) {
            CURL* easy = curl_easy_init();
            if (!easy) continue;
            configurarEasy(easy);
            poolEasy.push_back(easy);
        }
    }

    void actualizarHeaders(const std::string& token) {
        if (token == tokenEnHeaders && headers != nullptr) return;

        if (headers != nullptr) {
            curl_slist_free_all(headers);
            headers = nullptr;
        }

        tokenEnHeaders = token;
        std::string authHeader = "Authorization: Bearer " + token;
        headers = curl_slist_append(headers, authHeader.c_str());
        headers = curl_slist_append(headers, "accept: application/json");

        for (CURL* easy : poolEasy) {
            curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
        }
    }

    void liberar() {
        if (headers != nullptr) {
            curl_slist_free_all(headers);
            headers = nullptr;
        }
        for (CURL* easy : poolEasy) {
            curl_easy_cleanup(easy);
        }
        poolEasy.clear();
        if (multi != nullptr) {
            curl_multi_cleanup(multi);
            multi = nullptr;
        }
        tokenEnHeaders.clear();
    }
};

static thread_local ContextoHilo contextoHilo;

namespace {

// Extrae el valor de gender del JSON de respuesta
std::string extraerGenero(const std::string& json) {
    size_t posGender = json.find("\"gender\"");
    if (posGender == std::string::npos) return "DESCONOCIDO";

    if (json.find("MASCULINO", posGender) != std::string::npos) return "MASCULINO";
    if (json.find("FEMENINO", posGender) != std::string::npos) return "FEMENINO";
    return "DESCONOCIDO";
}

} // namespace

// --- Asegura que exista un token válido antes de consultar ---
static bool asegurarToken() {
    #pragma omp flush(API::tokenGlobal)

    if (!API::tokenGlobal.empty()) return true;

    #pragma omp critical(token_management_lock)
    {
        #pragma omp flush(API::tokenGlobal)
        if (API::tokenGlobal.empty()) {
            API::autenticar();
            #pragma omp flush(API::tokenGlobal)
        }
    }

    return !API::tokenGlobal.empty();
}

// --- Ejecuta un mini-lote de peticiones GET concurrentes con curl_multi ---
static void ejecutarMiniLote(
    const std::vector<std::string>& uuids,
    size_t inicio,
    size_t cantidad,
    std::vector<std::string>& destino
) {
    contextoHilo.asegurarInicializado();
    contextoHilo.actualizarHeaders(API::tokenGlobal);

    const size_t maxConcurrentes = contextoHilo.poolEasy.size();
    size_t procesados = 0;

    while (procesados < cantidad) {
        size_t lote = std::min(maxConcurrentes, cantidad - procesados);
        std::vector<PeticionContexto> ctxs(lote);
        std::vector<std::string> urls(lote);

        for (size_t j = 0; j < lote; ++j) {
            size_t idxGlobal = inicio + procesados + j;
            urls[j] = std::string(Config::API_BASE_URL) + "/person/" + uuids[idxGlobal];

            CURL* easy = contextoHilo.poolEasy[j];
            ctxs[j].respuesta.clear();
            ctxs[j].httpCode = 0;

            curl_easy_setopt(easy, CURLOPT_URL, urls[j].c_str());
            curl_easy_setopt(easy, CURLOPT_WRITEDATA, &ctxs[j].respuesta);
            curl_multi_add_handle(contextoHilo.multi, easy);
        }

        int stillRunning = 1;
        while (stillRunning) {
            curl_multi_perform(contextoHilo.multi, &stillRunning);

            if (stillRunning) {
                curl_multi_wait(contextoHilo.multi, nullptr, 0, 50, nullptr);
            }
        }

        bool tokenExpirado = false;

        for (size_t j = 0; j < lote; ++j) {
            CURL* easy = contextoHilo.poolEasy[j];
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &ctxs[j].httpCode);
            curl_multi_remove_handle(contextoHilo.multi, easy);

            size_t idxDestino = inicio + procesados + j;

            if (ctxs[j].httpCode == 401) {
                tokenExpirado = true;
                destino[idxDestino] = "TOKEN_EXPIRADO";
            } else if (ctxs[j].httpCode == 200) {
                destino[idxDestino] = extraerGenero(ctxs[j].respuesta);
            } else {
                destino[idxDestino] = "DESCONOCIDO";
            }
        }

        if (tokenExpirado) {
            API::invalidarToken();
            contextoHilo.actualizarHeaders(API::tokenGlobal);

            for (size_t j = 0; j < lote; ++j) {
                size_t idxDestino = inicio + procesados + j;
                if (destino[idxDestino] == "TOKEN_EXPIRADO") {
                    destino[idxDestino] = API::obtenerGeneroCliente(uuids[idxDestino]);
                }
            }
        }

        procesados += lote;
    }
}

// --- Inicialización global de libcurl (llamar una vez al inicio) ---
bool API::inicializar() {
    if (curlInicializado) return true;

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        LOG_ERROR("No se pudo inicializar libcurl.");
        return false;
    }

    shareHandle = curl_share_init();
    if (shareHandle != nullptr) {
        curl_share_setopt(shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
        curl_share_setopt(shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
        curl_share_setopt(shareHandle, CURLSHOPT_LOCKFUNC, curlShareLock);
        curl_share_setopt(shareHandle, CURLSHOPT_UNLOCKFUNC, curlShareUnlock);
    }

    curlInicializado = true;
    return true;
}

void API::finalizar() {
    if (!curlInicializado) return;

    if (shareHandle != nullptr) {
        curl_share_cleanup(shareHandle);
        shareHandle = nullptr;
    }
    curl_global_cleanup();
    curlInicializado = false;
}

// --- Autenticación POST protegida (Double-Checked Locking con OpenMP) ---
bool API::autenticar() {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = std::string(Config::API_BASE_URL) + "/login/authenticate";
    std::string respuestaJson;
    long httpCode = 0;

    std::string jsonDatos = std::string(R"({"email":")") + Config::API_AUTH_EMAIL +
                            R"(","rut":")" + Config::API_AUTH_RUT + R"("})";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonDatos.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respuestaJson);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long)Config::API_CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        LOG_ERROR("Error en la solicitud de autenticación HTTP " + std::to_string(httpCode));
        return false;
    }

    size_t posJwt = respuestaJson.find("\"jwt\":\"");
    if (posJwt != std::string::npos) {
        size_t inicioToken = posJwt + 7;
        size_t finToken = respuestaJson.find('"', inicioToken);
        if (finToken != std::string::npos) {
            tokenGlobal = respuestaJson.substr(inicioToken, finToken - inicioToken);
            LOG_INFO("Token JWT obtenido correctamente.");
            return true;
        }
    }

    LOG_ERROR("Respuesta de autenticación sin campo jwt.");
    return false;
}

void API::invalidarToken() {
    #pragma omp flush(tokenGlobal)

    #pragma omp critical(token_management_lock)
    {
        #pragma omp flush(tokenGlobal)
        if (!tokenGlobal.empty()) {
            LOG_INFO("Token expirado. Renovando credenciales...");
            tokenGlobal.clear();
            autenticar();
            #pragma omp flush(tokenGlobal)
        }
    }
}

// --- Consulta GET individual reutilizando el primer easy handle del hilo ---
std::string API::obtenerGeneroCliente(const std::string& uuid) {
    if (!asegurarToken()) return "DESCONOCIDO";

    contextoHilo.asegurarInicializado();
    contextoHilo.actualizarHeaders(tokenGlobal);

    if (contextoHilo.poolEasy.empty()) return "DESCONOCIDO";

    CURL* easy = contextoHilo.poolEasy[0];
    std::string url = std::string(Config::API_BASE_URL) + "/person/" + uuid;
    PeticionContexto ctx;

    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &ctx.respuesta);

    CURLcode res = curl_easy_perform(easy);
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &ctx.httpCode);

    if (res != CURLE_OK) return "DESCONOCIDO";
    if (ctx.httpCode == 401) return "TOKEN_EXPIRADO";
    if (ctx.httpCode != 200) return "DESCONOCIDO";

    return extraerGenero(ctx.respuesta);
}

// --- Consulta un bloque completo de UUIDs (usado por api_paralelo) ---
void API::obtenerGenerosBloque(
    const std::vector<std::string>& uuids,
    size_t inicio,
    size_t cantidad,
    std::vector<std::string>& destino
) {
    if (cantidad == 0) return;
    if (!asegurarToken()) {
        for (size_t i = 0; i < cantidad; ++i) {
            destino[inicio + i] = "DESCONOCIDO";
        }
        return;
    }

    ejecutarMiniLote(uuids, inicio, cantidad, destino);
}

void API::limpiarRecursosHilo() {
    contextoHilo.liberar();
}