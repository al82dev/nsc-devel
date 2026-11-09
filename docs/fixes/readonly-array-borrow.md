# Arreglos accesibles por un préstamo de solo lectura

Un objeto de clase extraído mediante `Rc.Get()` o `Arc.Get()` es un préstamo de
solo lectura. La implementación anterior conservaba esa condición al acceder a
campos de clase, pero la perdía si el campo era un arreglo nativo. En ambas
familias el análisis aceptó `alias.Data[0] = 10`; con `Rc` aceptó además guardar
`alias.Data` en un local fuera del alcance del dueño. Los comandos y resultados
anteriores están en `A:/nsc-fixes-evidencia/public-readonly-array-field.log`,
`public-readonly-array-field-Arc.log` y `public-readonly-array-escape-Rc.log`.
Las reproducciones se limitaron al análisis semántico, sin ejecutar el escape.

La corrección conserva el préstamo al leer un campo de arreglo desde ese alias,
impide escribir sus elementos y conserva el préstamo en elementos de referencia.
También rechaza guardar el arreglo en destinos sin seguimiento del préstamo y
mantiene la marca al convertir explícitamente a un arreglo nativo. El análisis de
la conversión previamente aceptó `((int[])alias.Data)[0] = 10`; la salida de
esa primera ejecución se observó, pero su archivo fue sobrescrito al repetir la
prueba. La nueva regresión conserva esa expresión como caso negativo.

La regresión independiente `nsc.compiler.readonly-borrowed-array` usa los
controles local y atómico ya existentes. Comprueba escritura, inicialización y
reasignación de un destino, referencia mutable a elemento, condicional,
coalescencia, conversión explícita y acceso a elementos de clase; verifica la
lectura por el préstamo y la escritura ordinaria en O0/O2. Pasó 1/1 en
`A:/nsc-fixes-evidencia/readonly-array-ctest.log`. Los controles de las familias
públicas registran también rechazos para retorno y argumento y pasaron en
`public-readonly-array-regressions.log`, `public-readonly-array-escape-regressions.log`
y `public-readonly-array-class-escape.log`. Las pruebas existentes de referencias
locales y retornos pasaron 2/2 (`readonly-array-reference-tests.log`).

La compilación del compilador fue incremental y está documentada en
`readonly-array-build.log`; el sistema regeneró la configuración y la identidad
de versión, por lo que reconstruyó más objetivos que los cinco archivos de
frontend modificados. Las pruebas de punteros públicos se hicieron desde
fuentes; no acreditan consumo binario, seguridad concurrente, todos los caminos
de conversión ni acceso mediante código `unsafe`.
