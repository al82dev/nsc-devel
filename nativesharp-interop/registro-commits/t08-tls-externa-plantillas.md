# Dependencia TLS C externa en plantillas precompiladas

Dos productores usan una global C externa `_Thread_local` en plantillas. Antes,
la captura y el verificador rechazaban expresamente dependencias TLS; el
consumidor no podía resolver la variable al instanciar las plantillas.

La captura admite ahora TLS externa y el verificador acepta su representación
existente. TLS interna sigue excluida. No cambia el formato persistido de NSMeta
ni se modifica NSLink.

Con LLD y el arranque TLS del proyecto, dos bibliotecas precompiladas publicaron
la dependencia y el consumidor compiló, enlazó y ejecutó en O0 después de retirar
los encabezados originales. La regresión TLS precompilada existente pasó en
O0–O3. La muestra nueva solo observa el hilo principal: no acredita aislamiento
entre hilos, DLL ni otras optimizaciones.
