// =============================================================================
// transaccion.h - Estructura de datos de una transacción de venta
//
// Representa una fila válida del CSV. Solo almacena datos; no contiene lógica.
// =============================================================================

#ifndef TRANSACCION_H
#define TRANSACCION_H

#include <string>

struct Transaccion {
    std::string fecha;              // Fecha y hora de la operación (ISO 8601)
    std::string canal;              // Canal de venta: POS o WEB
    int sku;                        // Código del producto
    std::string producto;           // Nombre del producto
    int unidades;                   // Cantidad comprada
    double porcentajeDescuento;     // Porcentaje de descuento aplicado
    double montoAplicado;           // Monto final pagado por el cliente
    long boleta;                    // Número de boleta
    std::string local;              // Identificador del local de venta
    std::string codigoCliente;      // UUID del cliente
};

#endif
