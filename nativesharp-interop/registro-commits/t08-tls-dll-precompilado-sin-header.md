# TLS de DLL precompilado con header ausente

El fixture oculta únicamente la copia temporal de `TlsApi.h` al compilar cada
consumidor y la restaura incluso si el compilador devuelve error. Esto comprueba
la independencia efectiva del header, más allá de omitir la referencia en el
proyecto. El resultado queda pendiente de validación agrupada.
