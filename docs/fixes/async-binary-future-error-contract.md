# Error de Future al cruzar una biblioteca NativeSharp

En `7dc80164d`, un productor compilado devolvía un future con error tipado;
el consumidor binario esperaba indefinidamente cuando el productor publicaba
el error. El proceso O0 excedió los 120 segundos del runner y fue terminado;
O2 no llegó a ejecutarse. Evidencia y comandos:
`A:/nsc-fixes-evidencia/async-poll-external-current-7dc80164d.log`.
El ABI del productor contiene `future:builtin:int|type:...Exception` y su
future ocupa 48 bytes. El LLVM del consumidor reconstruía `future:builtin:int`
de 40 bytes y solo trataba -1 como estado terminal; ambos volcados y el ABI
están en la carpeta homónima. La incompatibilidad de tamaño también afecta
la escritura del resultado retornado; no se ejecutó otro acceso inseguro.

La materialización anterior descartaba la parte de error de la clave. Se
adopta `Future<T,E>` como superficie estructural explícita, equivalente al
tratamiento de `Iterator<T,E>`, y se preserva esa información al reconstruir
tipos de metadata. `Future<T>` continúa representando un future sin error. Una
fábrica síncrona que puede lanzar y devuelve `Future<T>` sigue siendo distinta
de una que devuelve `Future<T,E>`. No se modifica el formato de metadata.

El build incremental terminó 0; el compilador aislado tuvo SHA-256
`b269cc4b96c40fb3bb9be87e6ccf20ad1c221210cabfed0943209c815bf640d1`.
El runner binario cruzó productor O0/O2 con consumidor O0/O2: las cuatro
ejecuciones capturaron el error y devolvieron 42
(`A:/nsc-fixes-evidencia/async-poll-external-future-final-20261112.log`). El
runner de fábricas desde fuente pasó diez ejecuciones O0/O2, incluida una
fábrica `Future<int, Exception>`, y mantuvo dos rechazos de conversión a
`Future<int>`; el control binario sin error pasó con caché de objetos
(`async-future-error-source-controls-20261112.log` y
`async-future-error-binary-controls-20261112.log`). El timeout de ejecución
del runner binario se acotó a 10 segundos; compilación conserva 120.

El control binario se amplió con una fábrica síncrona `throws` que devuelve
`Future<int>`: cuando no lanza, el future sin error se espera y devuelve 5;
cuando lanza, se captura su excepción. Las cuatro combinaciones O0/O2 del
productor y consumidor también ejecutan la reproducción original de error y
devuelven 42 (`async-poll-external-future-verified-20261112.log`). El primer
intento de esta ampliación fue rechazado porque el consumidor no capturaba la
llamada declarada lanzadora; el log `async-poll-external-future-factory-20261112.log`
conserva ese diagnóstico. El control posterior agregó el catch sin cambiar la
reproducción original del error.

Se añadió al mismo control una fábrica síncrona que devuelve
`Future<int, Exception>` sin declarar que ella misma lanza. La excepción del
future se capturó, al igual que en la llamada async directa, en las cuatro
combinaciones de productor y consumidor O0/O2
(`async-poll-external-future-relay-20261112.log`).

No se acreditaban future anidados, todos los tipos de error, consumidores de
otros compiladores, ni cancelación o finally suspendible. No se ejecutó la
batería completa ni interoperabilidad C/C++.

En `9a219326e`, una prueba desde fuente de un async que devuelve
`Future<int, Exception>` se rechazó en O0 y O2: interpreta el error del future
interior como error del exterior y reduce el resultado interior a
`Future<int>`. El comando y diagnóstico se conservaron en
`A:/nsc-fixes-evidencia/future-nested-source-probe-20261112.log`. La clave
actual concatena ambos componentes con `|` sin delimitar futures anidados;
por tanto la representación de `Future<Future<int, E>>` coincidía con
la de `Future<Future<int>, E>`. En el cambio posterior se delimita el resultado
compuesto con separador interior dentro de la clave, conservando las claves sin
ese separador; se calculan los
layouts de futures anidados y se reconstruye su tipo desde metadata sin cambiar
el formato del archivo. Los resultados anidados necesarios se incluyen en los
layouts MIR.

`async_nested_future_source.py` ejecutó 42 en O0/O2 con dos y tres niveles,
verificando la excepción interior y su destrucción. `async_nested_future_binary.py`
cruzó productor y consumidor O0/O2: las cuatro ejecuciones devolvieron 42 con
errores interiores y exteriores por separado y combinados, y cuatro
construcciones y destrucciones de excepción; el control binario
original no anidado también pasó las cuatro combinaciones. Evidencia:
`A:/nsc-fixes-evidencia/future-nested-last-source-20261112.log`,
`future-nested-last-binary-20261112.log` y
`future-nested-last-control-20261112.log`. No se acreditan todas las
composiciones de tipos, otras plataformas, abandono ni compatibilidad con
metadata histórica ambigua de futures anidados.

La misma prueba se amplió con un parámetro binario
`Future<Future<int, Exception>>` consumido por una función async. Las cuatro
combinaciones productor/consumidor O0/O2 devolvieron 42 y contaron cinco
construcciones y destrucciones de excepción; la ejecución está en
`A:/nsc-fixes-evidencia/future-nested-binary-parameter-20261112.log`.
