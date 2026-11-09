# Almacenamiento estático de Rc y WeakRc

El usuario eligió prohibir el almacenamiento estático compartido de `Rc` y
`WeakRc`, incluso dentro de otros tipos. Una reproducción anterior guardaba
`WeakRc` en un campo estático y lo usaba desde un método iniciado por `Thread`;
el análisis la aceptó (`public-local-thread-negative.log`). No se ejecutó esa
variante con contadores no atómicos.

El modelo de miembros examina el tipo resuelto y sigue campos, arreglos,
tuplas, referencias tipadas, clases base y derivados conocidos. La búsqueda
comprueba la plantilla y la identidad de especialización al reconocer las
familias locales. Un índice inverso de herencia evita volver a descubrir todas
las relaciones al procesar cada campo. El análisis de miembros rechaza campos
estáticos afectados, el de namespace rechaza estado global y el de cuerpos
rechaza locales estáticos. Los tipos atómicos siguen admitidos cuando su
contenido no incluye una referencia local.

Las reproducciones anteriores aceptaban fuertes y débiles, contenedores,
arreglos, locales estáticos y estado de namespace. También aceptaban una base
o interfaz estática capaz de recibir una clase derivada con `WeakRc`; las
salidas anteriores están en `public-tuple-borrow-<caso>-Rc.log` bajo
`A:/nsc-fixes-evidencia`. La prueba autocontenida
`nsc.compiler.shared-static-semantics` pasó sus 19 casos: verifica esos
destinos, tipos genéricos, arreglos fijos y punteros, y conserva controles de
`Arc`, uso local, ciclo, clase hermana y `Rc` de otro namespace
(`shared-static-ctest.log`). El fixture declara tipos de prueba con los mismos
nombres de Core; no demuestra por sí solo el SDK público.

Con las fuentes públicas en curso se rechazaron once reproducciones y pasaron
en O0/O2 los controles de `Rc` local, `WeakArc` estático, tipo ajeno y conteo
reentrante. Para este último la prueba usa un observador `unsafe` estrictamente
local al test, pues la referencia débil real ya no puede ser un campo estático.
Una prueba con productor y consumidor NativeSharp distintos rechazó los cinco
casos afectados y aceptó los controles local y atómico
(`shared-static-project-reference.log`). El análisis semántico del proyecto
Core aislado pasó (`shared-static-core-check.log`) y la prueba de contienda
`Arc`/`WeakArc` desde fuentes pasó en O0/O2 (`public-atomic-race-ctest.log`).

La regla es conservadora: puede rechazar un estático que solo se usa en un
hilo y una base o interfaz por tener un derivado afectado conocido. Recorre
solo los tipos, miembros y relaciones presentes en la compilación; no se ha
acreditado información suficiente sobre campos ocultos de bibliotecas
precompiladas ni clases cargadas posteriormente. La identidad de Core se
contrasta con la plantilla resuelta y su namespace; no existe una verificación
criptográfica del proveedor. El código `unsafe` que convierte un puntero a
entero puede ocultar la procedencia al análisis y no queda cubierto. Por tanto,
estas pruebas no certifican la restricción para todas las cargas binarias ni
el contrato completo de concurrencia de las cuatro familias.

Las compilaciones fueron incrementales y usaron temporales en A. El cambio de
la cabecera del modelo de miembros obligó a recompilar dependencias del propio
compilador, incluidos objetos de fuentes de interoperabilidad en la carpeta de
build aislada; no se modificaron esas fuentes ni las carpetas de build de
interoperabilidad y no se ejecutaron sus pruebas.
