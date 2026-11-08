# Aislamiento entre hilos de TLS C interna en plantillas

El hilo principal modifica las dos globales `static _Thread_local` de productores
precompilados. Un segundo hilo creado con `CreateThread` llama mediante callback
NativeSharp a las plantillas, comprueba los valores iniciales de ese hilo y los
modifica. Tras esperar al hilo, el principal comprueba que conserva sus valores.

El consumidor compiló y ejecutó correctamente en O0 sin los encabezados de los
productores, con LLD y el arranque TLS existente. Se reproduce con
`static-dos-bibliotecas-mutables/validar-tls.cmd hilos`. El control prueba dos
hilos y esos dos tipos escalares; no acredita DLL ni otras optimizaciones.
