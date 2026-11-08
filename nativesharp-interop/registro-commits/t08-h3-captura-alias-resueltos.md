# Tanda 8: capturar alias resueltos de plantillas

La captura utiliza el alias elegido por el análisis de nombres, su proyecto y
su referencia C para reconstruir un snapshot de dependencia. Rechaza un origen
que no exista y no altera la salida cuando falla. La identidad `cpp-reference:N`
es local al módulo productor y representa el índice de la referencia de ese
proyecto; no es un hash ni acredita por sí sola que dos encabezados coincidan.

Las plantillas sin binding permanecen sin resolver: esta captura no las declara
completas ni publica todavía el catálogo. La prueba añadida cubre el rechazo
de una referencia inexistente; la compilación y ejecución quedan pendientes
para agruparlas con los siguientes cambios. Sigue pendiente la prueba positiva
con referencias C reales y el circuito completo productor–consumidor.
