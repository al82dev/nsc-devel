# Globales C TLS internas en plantillas precompiladas

Dos productores usan globales C `static _Thread_local` del mismo nombre con
valores distintos. La captura y el verificador descartaban estas dependencias,
por lo que el consumidor no podía resolverlas en las plantillas.

La publicación acepta la representación existente de TLS interna, conservando
su símbolo y procedencia propios. No cambia el formato de NSMeta ni NSLink.
Con LLD y el arranque TLS existente, ambas bibliotecas y el consumidor compilaron,
enlazaron y ejecutaron en O0 después de retirar los encabezados de los productores.
Las lecturas y modificaciones mantuvieron estados independientes.

La regresión existente de TLS interna pasó en O0–O3 y el caso de TLS externa
precompilada volvió a pasar. El caso nuevo solo observa el hilo principal; no
acredita aislamiento entre hilos, DLL ni otras optimizaciones.
