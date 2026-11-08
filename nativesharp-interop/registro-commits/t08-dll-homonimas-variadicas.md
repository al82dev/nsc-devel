# Funciones C variádicas de DLL TLS homónimas

El rechazo anterior impedía usar dos funciones variádicas del mismo nombre aun
cuando cada plantilla precompilada conservaba la identidad de su DLL. Una
función intermedia convencional no puede reenviar los argumentos variables.
En llamadas directas, LLVM resuelve ahora la exportación de la DLL identificada
y la invoca con el tipo variádico y todos los argumentos de la llamada original.
La conversión de esa importación a delegado se rechaza en el backend mientras se
resuelve su representación; esa ruta no fue comprobada en esta muestra.

`validar-variadicas.py` construye dos DLL llamadas `api.dll`, con estado TLS y
exportaciones homónimas; en cada una un control C llama con seis enteros
variables. Dos plantillas NativeSharp precompiladas llaman a su propia DLL y
comprueban resultados distintos tras actualizar sus variables TLS. Las cuatro
optimizaciones O0–O3 pasaron, incluida la restauración desde caché y ejecución
desde otro directorio. La regresión preexistente de DLL homónimas también pasó
en O0–O3. Se incorporó el caso al validador de optimizaciones.

Esta evidencia cubre solo la muestra entera directa. No acredita flotantes,
promociones, agregados, punteros a función, otros tipos de consumidor ni la tanda
completa. La tanda 8 permanece abierta.
