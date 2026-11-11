# Adaptador de heap para AddressSanitizer

El ejemplo 97 original reserva un arreglo de cuatro enteros y escribe fuera de
su límite. Con la opción ASan existente, compilaba pero la ejecución no
reportaba el acceso en O0/O2: el runtime asignaba con el heap de Windows,
que no estaba conectado al asignador observado por ASan. La instrumentación
del acceso fue observada en el ejecutable O0; la reproducción y el análisis
están en `A:/nsc-fixes-evidencia/asan-original-97-20261111/`. La fuente y el
proyecto de la regresión coinciden byte por byte con el ejemplo original.

Para enlaces ASan, el compilador comprueba las referencias al heap, crea una
copia temporal del runtime y redirige sus llamadas `HeapAlloc`, `HeapReAlloc`
y `HeapFree` a un adaptador que usa `malloc`, `realloc` y `free` bajo el
runtime ASan. La biblioteca de entrada no se modifica. El compilador comprueba
la copia transformada y rechaza una transformación fallida o referencias
incompatibles; los enlaces normales no siguen esta ruta. El adaptador rechaza
flags, heaps y punteros que no puede manejar con seguridad. La opción por
defecto `allocator_may_return_null=1` conserva el fallo de reserva como null,
aunque `ASAN_OPTIONS` puede modificar la configuración efectiva.

Para un runtime independiente, se compara con el generado por el propio
compilador; una biblioteca diferente se rechaza porque sus referencias a las
tres funciones de heap no prueban que su asignador las utilice realmente.
Esto es conservador y también puede rechazar runtimes compatibles. No se
afirma soporte general para runtimes personalizados ni equivalencia con todos
los flags de Windows. La detección específica de doble liberación depende de
las opciones de cuarentena ASan; con cuarentena cero se ha observado el
diagnóstico genérico del adaptador.

Con el compilador aislado SHA-256
`84b3406e935d6bd5a71007c34daeea1d2040f3bf868cf1341bda8676fef99ac1`, la
selección dirigida `asan-current-20261111.cmd` pasó 5/5 sin omisiones. Incluye
el control de instrumentación, el driver, el ejemplo original de heap, el
runtime integrado y una prueba de liberación excepcional que depende de
otros cambios locales y no forma parte de este commit. La regresión de heap
comprueba desbordamiento y uso después de liberar, en O0/O2; la de runtime
integrado comprueba reserva válida y fallo de reserva y reasignación. Se usó
`detect_leaks=0`; no se verifica ausencia general de fugas, instrumentación
de bibliotecas precompiladas ni todos los runtimes personalizados.

El compilador probado contiene otros cambios locales y el auxiliar de prueba
es un binario construido anteriormente en la misma carpeta aislada. La prueba
no acredita este commit sobre un árbol limpio. La ausencia real de
`llvm-objcopy` tampoco se verificó; sí hay controles de fallo de herramienta y
de resolución cuando existe un ejecutable homónimo en el proyecto. Los
comandos, diagnósticos y códigos se conservan en el log de la selección.

La regresión de liberación excepcional quedó registrada junto con esta nota.
Su ejecución dirigida previa está en
`A:/nsc-fixes-evidencia/asan-heap-targeted-20261111.log`: las variantes de
arreglos de estructuras y clases pasaron en O0 y O2 y ASan diagnosticó el
acceso posterior a la liberación. La prueba comprueba ese recorrido concreto,
no la ausencia general de fugas. El compilador usado contenía cambios locales;
no se ha validado el commit con un compilador construido desde un árbol limpio.
