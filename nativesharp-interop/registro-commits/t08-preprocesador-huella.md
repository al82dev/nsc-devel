# Huella del preprocesador de Clang

La sesión puede calcular opcionalmente una huella de los tokens expandidos y
de las decisiones observadas en `#if`, `#elif`, `__has_include` y
`__has_embed`. La captura está desactivada por defecto, para no añadir este
coste a las compilaciones normales. La huella se entrega como copia y deja de
estar disponible al cerrar la sesión.

La prueba de sesión compara aperturas iguales y comprueba un `__has_include`
que cambia únicamente un `#pragma pack`, sin incluir el archivo consultado.
El caso nuevo no reportó fallo, pero el ejecutable completo de pruebas de sesión
terminó con código 1: tres comprobaciones de dependencias de enum y dos del
gate ThinLTO fallaron. Se repitió con el entorno de la tanda y el resultado fue
el mismo; no se ha establecido su causa ni atribuido al cambio. El flujo C con
caché de ruta larga, con la captura desactivada, pasó en O0.

Investigación posterior: dos controles de catálogo llaman al lector sin indicar
el contexto del catálogo. Cambiar su valor predeterminado al contrato actual
hizo desaparecer esos dos fallos, pero produjo otros seis: determinadas entradas
se interpretaron como un tipo de catálogo diferente. Se retiró el cambio; no se
modificaron las pruebas de NSMETA. La corrección requiere desambiguar el contexto
sin adivinar por el primer byte. Los dos controles ThinLTO, por su parte, esperan
el rechazo general que se retiró en `e73afd9b0` al habilitar objetos nativos sin
internalización; no se restauró ese rechazo. La comprobación de enum restante
también sigue pendiente de diagnóstico.
Tras reconstruir incrementalmente el compilador y el ejecutable de pruebas
con el código restaurado, la prueba de sesión volvió a mostrar los cinco fallos
originales. La validación completa de la tanda no se considera aprobada.
El control ThinLTO se corrigió después: [resultado](t08-control-thinlto-internalizacion.md).
La comparación de enum desde buffer virtual también se corrigió después:
[resultado](t08-enum-fuente-virtual.md). Persisten los dos fallos del lector de catálogo.
Esta huella es una pieza de la identidad, no una clave de caché completa:
faltan combinar opciones, contenido, toolchain y demás dependencias, además de
validar su seguridad de extremo a extremo. La restricción de caché para
referencias C/C++ directas permanece activa. T08-002 y la tanda 8 siguen abiertos.
