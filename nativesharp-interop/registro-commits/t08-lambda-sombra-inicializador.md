# Sombras de nombres C en lambdas

El fixture existente de sombras ejecutó en O0 una lambda cuyo parámetro
sombrea la global C `shadow_value`. El nuevo fixture `lambda-inicializador`
comprueba una lambda dentro de una plantilla precompilada: el inicializador de
una variable local usa la global C antes de introducir la variable homónima.
El consumidor se construyó y ejecutó con el resultado esperado en O0.
Son muestras concretas; otras formas de captura y la batería completa quedan
fuera de esta evidencia.
