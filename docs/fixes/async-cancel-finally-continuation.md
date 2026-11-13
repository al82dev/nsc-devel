# Continuación de finally durante la cancelación async

Estado: implementación en curso, sin certificación de todas las formas de
`yield` ni del abandono de un hijo pendiente. El árbol de referencia antes de
este ensayo es `2470df502`.

El control `async-cancel-finally-current-20261113.log` devolvió 22 al cancelar
antes de entrar al `finally` y 23 al cancelar cuando ya había comenzado, en
O0 y O2. La ejecución normal devolvió 42. Se conserva la reproducción original.

El ensayo mantiene el motivo de cancelación mientras un `await` en `finally`
se suspende, reanuda la limpieza y descarta los valores cuya propiedad puede
identificar. En las rutas de clase aceptadas, un dueño transferido se destruye
una vez y un alias estático no se destruye. La forma directa `move` se admite
solo para un local dueño que el MIR identifica como transferencia y anula.
El alias de un dueño local acreditado se evalúa sin destruirlo: su región de
cleanup conserva la responsabilidad. La reproducción devolvía 22 en O0/O2;
la regresión incluye cancelación antes y durante el finally y ejecución normal
en `async-cancel-local-owner-alias-regression-20261113.log`.
Un alias no dueño copiado de un campo estático también devolvía 22. La prueba
de binding acredita `LocalValue` sin propiedad ni transferencia; la regresión
`async-cancel-local-borrow-alias-regression-20261113.log` verifica cancelación
antes y durante finally, ejecución normal y que el objeto no se destruya.
Los resultados escalares, las referencias locales y los temporales de valor
probados tienen rutas propias; las formas desconocidas no se declaran cubiertas.

`async-cancel-mixed-move-test-20261113.log` registra diez casos de propiedad y
alias O0/O2; `async-yield-eh-discard-controls-20261113.log` repite los controles
tras el último cambio. Los archivos `async-cancel-reference-test-20261113.log`,
`async-cancel-scalar-types-after-20261113.log` y
`async-cancel-value-owned-test-20261113.log` registran las pruebas dirigidas de
referencias, escalares y estructuras. El wrapper con concatenación comprueba
efectos y orden, no acredita por sí solo la liberación de la cadena.

Una llamada que lanza después de un primer `yield`, seguida de un `finally`
que produce un literal entero, terminaba anormalmente en O0/O2 antes de
entregar ese valor (`async-cancel-exception-pending-probe-20261113.log`). El
puente de excepción rechazaba esa forma de `yield`. El ensayo la admite y
conserva el dueño de la excepción hasta la reanudación, la cancelación o el
`Drop`. `async-yield-eh-owner-nested-20261113.log` registra nueve casos O0/O2,
incluidas excepciones de un hijo suspendido, reemplazo y finally exterior.
Los controles dirigidos anteriores de excepciones y de yield desde finally
pasaron en `async-yield-eh-controls-20261113.log` y
`async-yield-eh-generator-controls-20261113.log`.
El control del ejemplo 88 conservado pasó sus cuatro casos en O0 y O2
(`async-example88-continuation-control-20261113.log`).

Tras reorganizar los helpers y completar el tipo interno del plan, el build
incremental pasó en `async-cancel-plan-refactor-build-20261113.log`. Se volvieron
a ejecutar los controles dirigidos de propiedad mixta, valores, referencias y
excepciones: `async-plan-refactor-mixed-20261113.log`,
`async-plan-refactor-value-20261113.log`,
`async-plan-refactor-reference-20261113.log` y
`async-plan-refactor-exception-20261113.log`. Todos pasaron en O0 y O2; estas
ejecuciones no constituyen una batería completa del compilador.
Las regresiones enfocadas quedaron registradas como
`nsc.compiler.async-cancel-finally-regressions` en CTest. Su orquestador usa
un directorio temporal independiente y, por el contrato actual de esos
runners, se omite fuera de Windows o si el compilador y el temporal no están
en A. El test registrado pasó con el build aislado en
`async-cancel-finally-ctest-confirm-20261113.log`. `CMakeLists.txt` es un
archivo compartido que debe revisarse al integrar con interoperabilidad.

El `move` directo de una estructura local funciona en los casos dirigidos con
y sin campo dueño, constructor fallido, ejecución normal y cancelación después
de producir el valor: once casos O0/O2 en
`async-cancel-value-move-owned-started-20261113.log`. En una reproducción
distinta el verificador LLVM sigue rechazando el `move` de estructura por
dominancia de un funclet; esa forma no se declara corregida. Un ensayo inicial
provocó un error fatal de LLVM en esa reproducción y se limitó para recuperar
el diagnóstico anterior (`async-cancel-owned-move-guard-20261113.log`).
Siguen pendientes las demás formas de operando sin evidencia de propiedad.
El IR rechazado muestra una entrada directa desde el despacho de la corrutina
a un bloque que usa un pad SEH no dominante. La copia de cleanup contiene
además un `invoke` anidado en ese funclet. La solución debe preservar la
limpieza durante excepciones extranjeras sin suspender dentro del funclet;
retirar el token del pad por sí solo no demuestra corrección. El IR y el
ensayo temporal están en `async-value-move-ir-probe-20261113.log`; la
instrumentación se retiró después de capturarlo.
`Drop` ejecutado mientras un `finally` espera a un hijo suspendido devuelve
antes de completar la limpieza: la traza observada es 12 frente a 1234 en O0
y O2 (`async-cancel-drop-await-trace-20261113.log`). El drop libera después el
frame del iterador; no se ha verificado la liberación del frame hijo. No se
considera resuelto hasta definir y validar el ciclo de vida sin introducir una
espera síncrona indefinida. Los archivos y resultados de evidencia están bajo
`A:/nsc-fixes-evidencia` y no sustituyen los ejemplos originales.
