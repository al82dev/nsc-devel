# Análisis de globales C++ calificados

El análisis preserva restricciones de solo lectura y rechaza formar punteros
sin calificadores hacia globales C++ calificados. También comunica errores
diferidos al seleccionar una declaración y rastrea el operando de dirección.

El modelo incluye interruptores experimentales para pruebas de vida útil; su
presencia no significa que esa capacidad esté habilitada en todos los casos.
