# Resultado del hilo en la prueba TLS

El primer enlace falló por `__imp_GetExitCodeThread`, dependencia del
instrumento que no está disponible en el catálogo usado. El hilo comunica su
resultado por memoria que el hilo principal lee tras esperar su finalización.
La prueba de aislamiento TLS no cambia y debe repetirse.
