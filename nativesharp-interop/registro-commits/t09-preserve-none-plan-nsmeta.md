# Captura y transporte del plan preserve_none

El inventario de aliases C++ captura el plan de Clang para `preserve_none` cuando la firma recibe y devuelve el mismo registro identificado. El validador coteja convención, prototipo, identidad completa, tamaño, alineación y forma física. El catálogo NSMeta vigente puede codificar el plan usando el campo de plan existente; al decodificar vuelve a validarlo mediante la misma regla. No se agrega una estrategia de versiones históricas.

La prueba `nsc.cpp-interop.regcall-record-plan` pasó con planes Clang de registros de 8 y 44 bytes, ida y vuelta por el catálogo y rechazos de convención, paso físico e identidad adulterados. La compilación incremental pasó. Como regresión, los fixtures `pruebas-tanda-09-preserve-none-record` y `pruebas-tanda-09-preserve-none-record24` pasaron cada uno dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-record-precompilado-k92szqn3` y `A:/temp/nsc-t09-preserve-cpp-record24-precompilado-2cc033lk`).

La captura y el transporte no habilitan por sí mismos nuevas firmas. La conciliación posterior para una llamada indirecta se documenta [aquí](t09-preserve-none-plan-indirecto.md) y la de una directa [aquí](t09-preserve-none-plan-directo.md). Otras firmas siguen pendientes; T09-003 y la tanda 9 siguen abiertos.
