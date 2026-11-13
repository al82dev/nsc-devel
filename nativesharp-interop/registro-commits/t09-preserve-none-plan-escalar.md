# Validación de planes escalares preserve_none

Se añade una comprobación de planes Clang `preserve_none` para firmas C++ no variádicas con resultados y argumentos numéricos o booleanos. Contrasta convención, símbolo, destino, tipos físicos, orden y atributos, incluidos los booleanos. Punteros, `void`, registros y variádicas quedan fuera de esta comprobación específica.

La prueba `nsc.cpp-interop.regcall-record-plan` pasó con planes Clang reales para `int(float)`, `bool(bool)` y una firma de cinco argumentos con tipos mixtos. Comprueba además el rechazo de una convención, tipo físico, atributo físico, atributo lógico o estructura física alterados. La compilación incremental pasó.

Esta pieza no captura ni transporta aún el plan escalar en NSMeta y no habilita por sí sola firmas nuevas en el consumidor. Se requieren pruebas de ejecución y precompilación antes de acreditar interoperabilidad adicional; T09-003 y la tanda 9 siguen abiertos.

Validación adicional: se comprueban coerciones lógicas y se rechazan offsets expandidos o un pointee físico en firmas escalares. Clang marca flatten en estas firmas; se conserva ese dato sin rechazarlo. La prueba dirigida nsc.cpp-interop.regcall-record-plan pasó con los planes reales y las corrupciones añadidas.

Captura parcial: el inventario admite los planes escalares de alias validados y NSMeta los conserva en el campo opcional existente. La prueba dirigida verifica ida y vuelta y rechazo de una coerción incompatible. Esta comprobación no valida todavía el inventario de extremo a extremo ni habilita llamadas escalares nuevas; siguen pendientes.

Captura directa: ClangSession obtiene planes de preserve_none C++ con escalares y los valida frente al simbolo y firma; NSMeta los transporta en el campo existente. La prueba nsc.cpp-interop.regcall-record-plan paso para int(float), bool(bool) y double(int,float,unsigned long long,short,double), incluidos ida y vuelta y rechazo de tipo fisico alterado. Todavia no habilita ni prueba ejecucion de firmas adicionales.

## Llamada directa int(float)

La sesion Clang captura el plan escalar de la declaracion C++ y el contrato vigente de NSMeta lo conserva. El compilador acepta `int(float)` solo si el plan coincide con su firma y simbolo. En esa etapa la llamada indirecta quedaba pendiente; su validacion posterior se describe abajo. La generalizacion a otras combinaciones sigue pendiente; T09-003 no se cierra.

Validacion con Clang 23.1.0 y NSC de desarrollo: ocho controles de Clang, ocho comparaciones directas NSC/Clang en estatica y DLL O0-O3 con inspeccion de simbolo e IR y dos valores, y dieciseis consumidores precompilados con y sin encabezado. Evidencias: `A:/temp/nsc-t09-preserve-cpp-escalar-plan-os68xx_4`, `A:/temp/nsc-t09-preserve-cpp-escalar-plan-lhb1fwdk` y `A:/temp/nsc-t09-preserve-cpp-escalar-plan-precompilado-hgxkjc9y`. La prueba dirigida `nsc.cpp-interop.regcall-record-plan` paso; tambien se conserva el rechazo de una firma aun no implementada de tres flotantes (`A:/temp/nsc-t09-preserve-none-negativo-bhox5j7j`).
## Llamada indirecta int(float)

La guarda del emisor admite la firma escalar concreta `int(float)` con `preserve_none`. La comprobacion sigue siendo por firma especifica, no exige el plan transportado del alias y no generaliza otras combinaciones. T09-003 sigue abierto.

Con Clang 23.1.0 pasaron ocho controles estaticos y DLL O0-O3 (`A:/temp/nsc-t09-preserve-cpp-escalar-indirecto-f4gdwj6c`); antes del cambio NSC rechazaba la firma (`A:/temp/nsc-t09-preserve-cpp-escalar-indirecto-pb8m9py1`). Pasaron ocho comparaciones directas Clang/NSC de llamada indirecta, identidad de puntero, dos valores e IR (`A:/temp/nsc-t09-preserve-cpp-escalar-indirecto-3rtrisdt`) y 16 consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-escalar-indirecto-precompilado-7aldpmw0`).
## Llamadas directas escalares con plan validado

La ruta directa admite escalares numericos y booleanos cuando el plan de Clang coincide con firma, simbolo, convencion y forma fisica. Las guardas anteriores para formas ya probadas se conservan. Esta regla no prueba todas las combinaciones posibles, no incluye punteros ni variadicas por esta via y no generaliza el callback. T09-003 permanece abierto.

Para `int(float,float,float)`, el control Clang y ocho comparaciones NSC/Clang pasaron para estatica y DLL O0-O3, y 16 consumidores precompilados pasaron con y sin encabezado. Evidencia: `A:/temp/nsc-t09-preserve-cpp-escalar-multiple-gp_z9548`, `A:/temp/nsc-t09-preserve-cpp-escalar-multiple-196vjvh4`, `A:/temp/nsc-t09-preserve-cpp-escalar-multiple-precompilado-npo_1tvf`. Para `double(int,float,unsigned long long,short,double)`, pasaron ocho comparaciones y 16 consumidores precompilados: `A:/temp/nsc-t09-preserve-cpp-escalar-heterogeneo-vfx053yg`, `A:/temp/nsc-t09-preserve-cpp-escalar-heterogeneo-precompilado-e206jh83`. La prueba de rechazo permanece con una firma que contiene un puntero no cubierta por esta ruta: `A:/temp/nsc-t09-preserve-none-negativo-z0szni8k`.
## Callbacks escalares con plan conciliado

El proveedor recupera el plan escalar del alias desde la sesion viva o el catalogo precompilado; comprueba identidad, tipo proyectado y plan. HIR adjunta el plan y el emisor vuelve a comprobarlo frente a los tipos escalares de la llamada. Se conservan las guardas especificas previamente soportadas. No se habilitan punteros, registros ni variadicas por esta ruta; T09-003 sigue abierto.

El callback `int(float,float,float)` paso ocho controles Clang y ocho comparaciones Clang/NSC O0-O3 en estatica y DLL, con plan presente en HIR, identidad de puntero, dos valores e IR: `A:/temp/nsc-t09-preserve-cpp-multiple-indirecto-xttj7z4m`, `A:/temp/nsc-t09-preserve-cpp-multiple-indirecto-z5vi9nvp`. Pasaron 16 consumidores precompilados con y sin encabezado y se verifico la restauracion del plan en O0: `A:/temp/nsc-t09-preserve-cpp-multiple-indirecto-precompilado-jcwd84lu`. La prueba sin alias de tipo rechaza el callback por ausencia de plan: `A:/temp/nsc-t09-preserve-none-escalar-sin-plan-n2dk_79o`. Pasaron la prueba de planes `nsc.cpp-interop.regcall-record-plan` y los controles previos del callback de registro de 44 bytes y del callback `int(float)`: `A:/temp/nsc-t09-preserve-cpp-record24-t8_jt4b9`, `A:/temp/nsc-t09-preserve-cpp-escalar-indirecto-61yrz9cj`. Estas muestras no certifican todas las combinaciones escalares.
Prueba adicional del callback heterogeneo `double(int,float,unsigned long long,short,double)`: ocho comparaciones Clang/NSC O0-O3 estaticas y DLL y 16 consumidores precompilados, comprobando presencia del plan, identidad de puntero y valores. Evidencias: `A:/temp/nsc-t09-preserve-cpp-heterogeneo-indirecto-ur3szihs` y `A:/temp/nsc-t09-preserve-cpp-heterogeneo-indirecto-precompilado-m3jnb6dn`.
