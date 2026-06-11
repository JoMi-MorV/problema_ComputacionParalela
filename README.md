## Descripción

Sistema en C++ para procesar archivos de ventas con paralelismo usando OpenMP.

Este repositorio parte de la funcionalidad de **descarga SFTP** (conexión al servidor, listado y descarga paralela de archivos `reporte_*.csv`) y agrega el módulo de **parseo CSV en paralelo**, que lee los archivos descargados, valida su contenido y carga las transacciones en memoria.

### Funcionalidades incluidas

**Descarga SFTP (base del proyecto)**
- Conexión al servidor SFTP mediante libcurl.
- Descarga paralela de archivos CSV con OpenMP.
- Reintentos con backoff exponencial ante fallos de conexión.
- Los archivos se guardan en la carpeta `output/`.

**Parseo CSV (agregado en esta etapa)**
- Lectura paralela de todos los `.csv` en `output/`.
- Validación de encabezado y de cada fila (fecha ISO, SKU, unidades, montos, UUID, etc.).
- Filtrado de filas inválidas o corruptas.
- Carga de transacciones válidas en un vector unificado en memoria.
- Resumen en consola con cantidad de archivos procesados, errores y total de transacciones.

## Requisitos

- Ubuntu 24.04 LTS (o distribución Linux compatible)
- Compilador g++ con soporte C++17 o superior
- Make para sistema de build
- Librerías externas:
  - `libcurl4-openssl-dev` → Conexiones SFTP con libcurl
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
| 2 | Solo parsear los CSV ya presentes en `output/` |
| 3 | Descargar y luego parsear en secuencia |

En cada fase se puede configurar la cantidad de hilos OpenMP a utilizar.

## Estructura del proyecto

```
src/
├── main.cpp                    # Orquestador principal y menú
├── sftp/
│   ├── sftp_client.cpp/h       # Cliente SFTP (libcurl)
├── csv/
│   ├── csv_parseo.cpp/h        # Lógica de lectura y validación CSV
│   └── transaccion.h           # Estructura de datos por fila
├── paralelismo/
│   ├── sftp_paralelo.cpp/h     # Descarga paralela SFTP
│   └── parseo_paralelo.cpp/h   # Parseo paralelo de archivos CSV
└── utils/
    ├── config.cpp/h            # Configuración (host, rutas, timeouts)
    └── logger.cpp/h            # Registro de eventos en log
```

## Salidas

- Archivos CSV descargados: `output/`
- Log de ejecución: `output/log.txt`
