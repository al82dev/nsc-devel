# T08-004: globales atómicas internas con biblioteca precompilada

Una biblioteca NativeSharp usó globales C internas, incluidos punteros
atómicos a record, enum, bool, void y const record. El consumidor ejecutó el
método compilado en O0–O3 después de ocultar la copia del encabezado C. El
control directo también seleccionó el record sin invocar funciones que lo
retornaran. Evidencia:
`A:\nsc-interop-build\tanda08-static-contratos-4783-13572`.

El consumidor solo invoca el método ya compilado; esta prueba no demuestra
que las declaraciones C sean visibles desde una plantilla pública ni cubre
DLL NativeSharp intermedia o concurrencia. T08-004 y la tanda siguen abiertos.
