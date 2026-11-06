# Span de la prueba de identidad

La compilación de cb60b585e advirtió que la expresión para obtener el nodo de
MIR partía de un span temporal. El span se conserva en una variable local
durante la prueba. Queda pendiente verificar la advertencia y el resultado en
la batería agrupada.
