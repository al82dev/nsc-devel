# Validación de planes escalares preserve_none

Se añade una comprobación de planes Clang `preserve_none` para firmas C++ no variádicas con resultados y argumentos numéricos o booleanos. Contrasta convención, símbolo, destino, tipos físicos, orden y atributos, incluidos los booleanos. Punteros, `void`, registros y variádicas quedan fuera de esta comprobación específica.

La prueba `nsc.cpp-interop.regcall-record-plan` pasó con planes Clang reales para `int(float)`, `bool(bool)` y una firma de cinco argumentos con tipos mixtos. Comprueba además el rechazo de una convención, tipo físico, atributo físico, atributo lógico o estructura física alterados. La compilación incremental pasó.

Esta pieza no captura ni transporta aún el plan escalar en NSMeta y no habilita por sí sola firmas nuevas en el consumidor. Se requieren pruebas de ejecución y precompilación antes de acreditar interoperabilidad adicional; T09-003 y la tanda 9 siguen abiertos.

Validación adicional: se comprueban coerciones lógicas y se rechazan offsets expandidos o un pointee físico en firmas escalares. Clang marca flatten en estas firmas; se conserva ese dato sin rechazarlo. La prueba dirigida nsc.cpp-interop.regcall-record-plan pasó con los planes reales y las corrupciones añadidas.

Captura parcial: el inventario admite los planes escalares de alias validados y NSMeta los conserva en el campo opcional existente. La prueba dirigida verifica ida y vuelta y rechazo de una coerción incompatible. Esta comprobación no valida todavía el inventario de extremo a extremo ni habilita llamadas escalares nuevas; siguen pendientes.

Captura directa: ClangSession obtiene planes de preserve_none C++ con escalares y los valida frente al simbolo y firma; NSMeta los transporta en el campo existente. La prueba nsc.cpp-interop.regcall-record-plan paso para int(float), bool(bool) y double(int,float,unsigned long long,short,double), incluidos ida y vuelta y rechazo de tipo fisico alterado. Todavia no habilita ni prueba ejecucion de firmas adicionales.

## Llamada directa int(float)

La sesion Clang captura el plan escalar de la declaracion C++ y el contrato vigente de NSMeta lo conserva. El compilador acepta `int(float)` solo si el plan coincide con su firma y simbolo. La llamada indirecta con esta firma y la generalizacion a otras combinaciones quedan pendientes; T09-003 no se cierra.

Validacion con Clang 23.1.0 y NSC de desarrollo: ocho controles de Clang, ocho comparaciones directas NSC/Clang en estatica y DLL O0-O3 con inspeccion de simbolo e IR y dos valores, y dieciseis consumidores precompilados con y sin encabezado. Evidencias: `A:/temp/nsc-t09-preserve-cpp-escalar-plan-os68xx_4`, `A:/temp/nsc-t09-preserve-cpp-escalar-plan-lhb1fwdk` y `A:/temp/nsc-t09-preserve-cpp-escalar-plan-precompilado-hgxkjc9y`. La prueba dirigida `nsc.cpp-interop.regcall-record-plan` paso; tambien se conserva el rechazo de una firma aun no implementada de tres flotantes (`A:/temp/nsc-t09-preserve-none-negativo-bhox5j7j`).