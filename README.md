## Descripción

Sistema en C++ para procesar archivos de ventas con paralelismo usando OpenMP.

Este repositorio parte de la funcionalidad de **descarga SFTP** y **parseo CSV en paralelo**, y agrega el módulo de **consultas API REST en paralelo**, que enriquece cada transacción consultando el género del cliente y calcula el monto promedio de compra por género.

### Funcionalidades incluidas

**Descarga SFTP (base del proyecto)**
- Conexión al servidor SFTP mediante libcurl.
- Descarga paralela de archivos CSV con OpenMP.
- Reintentos con backoff exponencial ante fallos de conexión.
- Los archivos se guardan en la carpeta `output/`.

**Parseo CSV (etapa anterior)**
- Lectura paralela de todos los `.csv` en `output/`.
- Validación de encabezado y de cada fila (fecha ISO, SKU, unidades, montos, UUID, etc.).
- Filtrado de filas inválidas o corruptas.
- Carga de transacciones válidas en un vector unificado en memoria.
- Resumen en consola con cantidad de archivos procesados, errores y total de transacciones.

**Consultas API (agregado en esta etapa)**
- Autenticación automática con JWT contra la API REST.
- Consulta paralela del género de cada cliente por UUID.
- Cache en memoria (tabla hash) para evitar consultas duplicadas.
- Renovación automática del token si expira durante la ejecución.
- Cálculo del monto promedio de compra por género (MASCULINO / FEMENINO).
- Salida en consola con métricas y tiempo de ejecución de la fase API.

## Requisitos

- Ubuntu 24.04 LTS (o distribución Linux compatible)
- Compilador g++ con soporte C++17 o superior
- Make para sistema de build
- Librerías externas:
  - `libcurl4-openssl-dev` → Conexiones SFTP y HTTP con libcurl
  - `libomp-dev` → Soporte para paralelismo con OpenMP

## Instalación de dependencias

```bash
sudo apt update
sudo apt install build-essential libcurl4-openssl-dev libomp-dev
```

## Compilación y ejecución

```bash
make
make run
```

Al ejecutar el programa se muestra un menú con tres opciones:

| Opción | Descripción |
|--------|-------------|
| 1 | Solo descargar CSV desde SFTP |
| 2 | Parsear los CSV en `output/` y luego consultar la API |
| 3 | Descargar, parsear y consultar la API en secuencia |

En cada fase se puede configurar la cantidad de hilos OpenMP a utilizar. La fase API se ejecuta automáticamente después del parseo (opciones 2 y 3) si hay transacciones cargadas en memoria.

## Estructura del proyecto

```
src/
├── main.cpp                    # Orquestador principal y menú
├── sftp/
│   ├── sftp_client.cpp/h       # Cliente SFTP (libcurl)
├── csv/
│   ├── csv_parseo.cpp/h        # Lógica de lectura y validación CSV
│   └── transaccion.h           # Estructura de datos por fila
├── api/
│   ├── api.cpp/h               # Cliente REST (autenticación y consulta de género)
├── paralelismo/
│   ├── sftp_paralelo.cpp/h     # Descarga paralela SFTP
│   ├── parseo_paralelo.cpp/h   # Parseo paralelo de archivos CSV
│   └── api_paralelo.cpp/h      # Consultas API paralelas y cálculo de métricas
└── utils/
    ├── config.cpp/h            # Configuración (host, rutas, timeouts)
    └── logger.cpp/h            # Registro de eventos en log
```

## Salidas

- Archivos CSV descargados: `output/`
- Log de ejecución: `output/log.txt`
- Métricas por género y tiempo: impresas en consola al finalizar la fase API
