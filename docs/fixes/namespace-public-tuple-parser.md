# Tuplas públicas de namespace en bibliotecas

El productor de una biblioteca con `using Pair = (int, long); using Other = Pair;`
y un estado público de namespace `Other value` fallaba al verificar su interfaz
generada: esta contiene `public (int, long) value;` y el despacho global del
parser no admitía el paréntesis inicial. El control de estado público escalar
se compilaba y el consumidor independiente lo escribía y leía correctamente.

`Parser::dispatchGlobalFallback` acepta un paréntesis cuando `canStartTypeAt`
reconoce un tipo válido y delega al parser existente. No cambia el formato de
metadata. La regresión `nsc.sdk.namespace-tuple-binary` construye bibliotecas
estáticas y consumidores independientes en O0 y O2; comprueba el control
escalar, la tupla mediante función y el acceso público directo. Exige el
resultado 42 y el rechazo sintáctico de una tupla inválida. Pasó 1/1 usando
el compilador aislado SHA-256
`25b5946bd906f16875dd02357b819ce352bf6ba2c5be581b55e02709463395c1`;
el comando y el log están en
`A:/nsc-fixes-evidencia/namespace-tuple-binary-20261111/registered.cmd` y
`registered.log`. La compilación del compilador fue incremental.

La prueba existente `nsc.parser` falla una de 23 comprobaciones que espera
rechazar el operador ternario; falla igual en la base aislada. Los resultados
están en `parser-test.log` y `baseline-parser-test.log` de la misma carpeta;
no se considera una prueba aprobada. No se comprobaron DLL, cruces O0/O2,
destrucción al descargar módulo ni tipos con préstamos en la tupla binaria.
