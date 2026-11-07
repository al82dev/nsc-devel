# T08-004: enum, void y const struct en punteros atómicos

El control C pasó. Las variantes de referencia estática y DLL de NativeSharp
pasaron O0–O3 con lecturas y escrituras de punteros atómicos a enum, void y
const struct. El puntero a enum avanzó por elemento y la escritura al miembro
de const struct se rechazó con diagnóstico. El IR verifica los accesos
atómicos de las cinco variables de la matriz.

La construcción incremental terminó con dos advertencias de inicializadores
existentes. Pasaron las regresiones dirigidas de punteros internos y de
punteros atómicos globales y TLS de anchos adicionales. Evidencia:
`A:\nsc-interop-build\tanda08-static-contratos-4241-31576`.

Esto no acredita todas las combinaciones de calificadores, escapes,
concurrencia o consumo precompilado. Los ítems y la tanda siguen abiertos.
