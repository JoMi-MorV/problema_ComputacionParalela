## Descripción
Sistema en C++ para descargar archivos CSV desde un servidor SFTP usando paralelismo con OpenMP.

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

Los archivos descargados se guardan en `output/`.
