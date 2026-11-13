# Validación de planes escalares preserve_none

Se añade una comprobación de planes Clang `preserve_none` para firmas C++ no variádicas con resultados y argumentos numéricos o booleanos. Contrasta convención, símbolo, destino, tipos físicos, orden y atributos, incluidos los booleanos. Punteros, `void`, registros y variádicas quedan fuera de esta comprobación específica.

La prueba `nsc.cpp-interop.regcall-record-plan` pasó con planes Clang reales para `int(float)`, `bool(bool)` y una firma de cinco argumentos con tipos mixtos. Comprueba además el rechazo de una convención, tipo físico, atributo físico, atributo lógico o estructura física alterados. La compilación incremental pasó.

Esta pieza no captura ni transporta aún el plan escalar en NSMeta y no habilita por sí sola firmas nuevas en el consumidor. Se requieren pruebas de ejecución y precompilación antes de acreditar interoperabilidad adicional; T09-003 y la tanda 9 siguen abiertos.

Validación adicional: se comprueban coerciones lógicas y se rechazan offsets expandidos o un pointee físico en firmas escalares. Clang marca flatten en estas firmas; se conserva ese dato sin rechazarlo. La prueba dirigida nsc.cpp-interop.regcall-record-plan pasó con los planes reales y las corrupciones añadidas.
