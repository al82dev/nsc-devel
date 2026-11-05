# Tanda 08: proyección compartida del tipo enum C

El helper prepara la conversión a la identidad nominal de un enum importado,
comprobando propietario, entero subyacente, tamaño, alineación y conflicto.
Conserva const para que sus consumidores puedan aplicarlo. La selección
transitiva y el comportamiento ABI siguen pendientes de validación agrupada.
