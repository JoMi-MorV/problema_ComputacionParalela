// =============================================================================
// sftp_paralelo.h - Orquestador de descarga SFTP en paralelo
//
// Expone la interfaz para descargar masivamente archivos del servidor
// usando OpenMP con un número configurable de hilos.
// =============================================================================

#ifndef SFTP_PARALELO_H
#define SFTP_PARALELO_H

class SFTPParalelo {
public:
    // Conecta al SFTP, lista archivos y los descarga en paralelo con OpenMP.
    // Actualiza los contadores: descargados, omitidos (ya existían) y fallados.
    static bool descargarArchivos(int hilos, int& descargados, int& omitidos, int& fallados);
};

#endif // SFTP_PARALELO_H
