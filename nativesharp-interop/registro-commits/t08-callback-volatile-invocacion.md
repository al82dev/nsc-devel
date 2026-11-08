# Reproducción: invocación indirecta con puntero C volátil

La reconstrucción de la declaración de un callback con `volatile Payload*`
ya funciona, pero invocarlo a través de una plantilla precompilada se rechaza
con `indirect call cannot yet preserve pointer pointee volatile qualification`.
El control C equivalente se ejecuta correctamente; el consumidor NativeSharp
no llega a producir ejecutable. Se reprodujo en O0 y O3.

La restricción está en `ProjectMethodBodyModel::bindDelegateInvocationArguments`.
La identidad estructural de la función se construye en
`ProjectMemberModel::canonicalFunctionTypeKey` usando claves de parámetros que
no conservan el calificador del dato apuntado. Eliminar el rechazo sin trasladar
y comprobar dicha información permitiría descartar `volatile` indebidamente.

`typedef-calificadores/validar-invocacion.py` genera la muestra de dos DLL,
ejecuta el control C y verifica que NSC emite el diagnóstico observado. El
script tiene éxito al reproducir el fallo: no acredita una corrección. Para
cerrar el caso hace falta conservar la calificación en la firma, validar las
conversiones y comprobar la invocación y el rechazo de pérdida de calificador.

## Estado posterior

Corregido en [t08-callback-volatile-identidad.md](t08-callback-volatile-identidad.md). Desde esa corrección,
`typedef-calificadores/validar-invocacion.py` exige que la invocación compile y se ejecute; ya no reproduce el
rechazo descrito arriba, que se conserva como registro histórico.
