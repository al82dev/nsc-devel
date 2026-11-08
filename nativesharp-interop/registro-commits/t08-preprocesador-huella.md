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
Esta huella es una pieza de la identidad, no una clave de caché completa:
faltan combinar opciones, contenido, toolchain y demás dependencias, además de
validar su seguridad de extremo a extremo. La restricción de caché para
referencias C/C++ directas permanece activa. T08-002 y la tanda 8 siguen abiertos.
