# T08 H3: función C con record en plantilla precompilada

`ConsumerRecordLocal` llamaba desde una plantilla a `gen_c_record`, cuyo resultado es un puntero a un record C. Aunque el record también está publicado en la superficie del productor, la captura de candidatas partía de una proyección de funciones sin contexto de records y omitía la función. El consumidor terminaba con `unknown identifier`.

El productor reutiliza la proyección existente con lookup y records para completar las observaciones de funciones que antes no se podían proyectar, antes de seleccionar y capturar las dependencias de la plantilla. La conciliación usa proyecto, símbolo y USR.

El validador dirigido de identidad compiló y ejecutó `ConsumerRecordLocal` y terminó con código 0; registro: `A:/nsc-interop-build/tanda08-identidad-record-local.log`. La regresión agrupada de dependencias en O0–O3 también terminó con código 0. El caso no demuestra la publicación de un record que no aparezca en otra parte de la superficie del productor, ni cierra T08-002.
