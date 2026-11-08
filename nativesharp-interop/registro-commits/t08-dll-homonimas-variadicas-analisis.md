# Tanda 8: llamadas variádicas con DLL TLS homónimas

## Hallazgo

`HirBuildCoordinator.cpp` rechaza actualmente funciones C variádicas con el
mismo símbolo en distintas DLL que necesitan alias TLS. Es una limitación
abierta, no una prueba de interoperabilidad.

La ruta existente para funciones no variádicas crea una función intermedia.
`emitRuntimeBoundForwarding` reenvía los parámetros declarados; por tanto no
puede reenviar automáticamente argumentos adicionales de una llamada variádica.
En `lowerStaticCall`, sin embargo, ya están disponibles los argumentos de la
llamada y el tipo LLVM de la función. Una alternativa por investigar es resolver
la dirección de la exportación en ese punto y realizar la llamada con el tipo
variádico original. Deben verificarse las promociones y el ABI con Clang.

La dirección de una función también puede participar en conversiones a delegado
y otras expresiones. Evitar generar un intermediario variádico incorrecto exige
revisar esas rutas; una prueba de llamada directa por sí sola no las acredita.

## Estado

Se desglosaron estos pendientes en el tracking. No se modificó el compilador ni
se ejecutaron pruebas para este análisis; las alternativas y su cobertura quedan
pendientes de implementación y validación dirigida.
