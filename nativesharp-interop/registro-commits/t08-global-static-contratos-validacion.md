# T08-004: validación de contratos de globales internas

La construcción incremental de NSC terminó correctamente. El control C y los
consumidores NativeSharp pasaron O0–O3 tanto con referencia estática como con
referencia a DLL. El IR de ambos consumos y el de Clang conservaron los accesos
volatile y atómicos verificados; las dos globales TLS internas dejaron de
declararse como importaciones de la DLL. Se comprobó el rechazo de escritura a
través de un puntero a const y de un tipo que no puede proyectarse.

Pasaron las regresiones dirigidas de TLS interno, tipos TLS, ambigüedad,
punteros y globales complejas. La evidencia de esta ejecución está en
`A:\nsc-interop-build\tanda08-static-contratos-32444-13048`. Los compilados
presentaron dos advertencias de inicializadores existentes en el archivo
modificado; no se certifica cobertura de todos los tipos ni aislamiento por
hilo de cada variante. La validación completa de la tanda y la integración de
los carriles independientes siguen pendientes.
