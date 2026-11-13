# Campo de valor con drop y constructor delegado

Con el compilador aislado de `64e56451b` y Core binario de la copia, una
clase con campo struct inicializado y destructor compiló y capturó su fallo de
construcción directa con el drop esperado, en O0/O2. La variante mediante
`MakeRcThrowing` fue rechazada por la barrera de limpieza parcial. Las fuentes,
comandos y resultados se conservan en
`A:/nsc-fixes-evidencia/shared-value-field-direct-20261112/` y
`A:/nsc-fixes-evidencia/shared-value-field-rc-20261112/`.

Al investigar esa barrera se encontró un prerrequisito independiente: si el
constructor exterior delega en otro que inicializa el campo y después lanza,
la construcción directa no destruye el campo. Esa reproducción devolvió 0
en vez de 42 en O0/O2; los controles directos sin delegación, con reemplazo y
con move devolvieron 42. Evidencia en
`shared-value-field-delegate-direct-20261112/`,
`shared-value-field-replace-direct-20261112/` y
`shared-value-field-move-direct-20261112/` dentro de la misma carpeta de
evidencia. Una variante con constructor de campo declarado lanzador fue
rechazada en análisis, por lo que no acredita su limpieza.

Se ensayó activar los flags exteriores después de que retorna el constructor
delegado. Los casos simples pasaron, pero si el constructor delegado mueve el
campo antes de retornar, el exterior lo vuelve a tratar como vivo. Ese control
falló para clase y struct en O0/O2; evidencia en
`delegated-value-field-move-delegate-20261112.log`. Se retiró el ensayo y se
reconstruyó incrementalmente el compilador. El fixture experimental está en
`A:/nsc-fixes-evidencia/delegated_constructor_value_field_cleanup_candidate.py`.

La corrección necesita conservar el estado real de vida de cada campo a través
de la delegación. Hasta demostrarlo, siguen pendientes tanto la delegación de
valores con drop como la aceptación del payload compartido; no se ha ampliado
la barrera ni se afirma seguridad de ownership o liberación física.

## Corrección de la forma estructural demostrada

Con `72f723cc7`, la reproducción original se copió sin modificar y volvió a
devolver 0 en O0/O2 (`delegated-value-current-20261113.log`). Su MIR muestra
que el constructor delegado llama primero a la base y luego asigna una única
construcción de valor al campo; no mueve el campo antes de retornar
(`delegated-value-mir-20261113.log`). El constructor exterior no registraba ese
campo ni activaba su flag cuando la delegación terminaba.

El backend reconoce únicamente un constructor local con identidad exacta y
cuerpo formado por una llamada base opcional anterior y una asignación directa
de `ValueConstruct` al campo propio, con argumentos literales. Excluye campos
volátiles, placement, layouts struct superpuestos, control de flujo, llamadas
adicionales, delegaciones anidadas y cuerpos importados o desconocidos. El flag
del campo se activa solo después del retorno exitoso del delegado; la limpieza
ya existente lo consulta ante una excepción posterior. La búsqueda usa el
índice MIR de nombre y comprueba además miembro y tipo declarador. No cambia
el ABI ni la barrera de seguridad de las fábricas compartidas.

La reproducción original devolvió 42 O0/O2 con el compilador incremental
(`delegated-value-indexed-original-20261113.log`). La prueba dirigida
`delegated_value_field_proven.py` ejecutó 19 casos O0/O2: clase y struct,
campo simple y campo con dueño, éxito, throw exterior, reemplazo y move;
también comprueba que mover el campo dentro del delegado no lo destruya dos
veces y que fallar en la base no destruya el campo aún no construido. El CTest
`nsc.compiler.delegated-value-field-proven` pasó en
`delegated-value-proven-ctest-20261113.log`. Se omite fuera de Windows o si el
compilador y el temporal no están en A.

Un productor NativeSharp separado, que contiene la reproducción sin cambiar
su lógica y exporta una función, se consumió como biblioteca junto con Core
binario: las cuatro combinaciones O0/O2 de productor y consumidor devolvieron
42 (`delegated-value-binary-probe-20261113.log`). También pasó el control de
construcción implícita de punteros compartidos
(`delegated-value-shared-control-20261113.log`). `CMakeLists.txt` es compartido
con otras ramas y debe revisarse al integrar. Los logs y temporales están bajo
`A:/nsc-fixes-evidencia`; no se ejecutó la batería completa.

Esta prueba estructural no demuestra el estado de campos después de cuerpos
con condicionales, llamadas, múltiples campos o delegaciones encadenadas. La
variante que mueve un campo dentro del delegado se mantiene fuera de la prueba
de vida; no se activa su flag por inferencia. La limpieza general y la
aceptación de payloads compartidos con campos de valor siguen pendientes.

## Dos campos directos

En `1eac7d473`, el caso de clase y struct con dos campos de valor construidos
por el delegado y fallo en el constructor exterior devolvía 0 en O0/O2,
cuando debía destruirlos en orden inverso y devolver 42. La reproducción y
salidas originales se conservaron en
`A:/nsc-fixes-evidencia/delegated-two-fields-before-20261113/` y su log.

La prueba estructural admite ahora hasta dos asignaciones directas a campos
distintos con las mismas restricciones anteriores. Rechaza el reconocimiento
si cualquier sentencia adicional, campo repetido o valor no acreditado impide
probar el estado; los flags se activan después del retorno normal. No se
habilitan constructores desconocidos, control de flujo, campos superpuestos ni
delegaciones encadenadas.

El caso original devolvió 42 para clase y struct en O0/O2; el comando está
en `delegated-two-fields-exact-after-20261113.cmd`. La regresión dirigida
registrada `nsc.compiler.delegated-value-field-proven` pasó con la ampliación
que comprueba dos campos, dueño anidado, fallo exterior, éxito, reemplazo,
movimiento posterior, fallo de base y fallo del segundo campo. Su log es
`delegated-two-fields-ctest-20261113.log`; los 74 controles precedentes y los
cuatro casos de fallo del segundo campo se conservan por separado en los logs
`delegated-two-fields-base-regression-20261113.log` y
`delegated-two-fail-second-20261113.log`. La primera versión del último control
fue rechazada semánticamente por declarar un inicializador de campo lanzador;
ese resultado está en `delegated-two-fields-final-20261113.log`. El control
posterior ejecuta las construcciones dentro del cuerpo que declara `throws`.

Con Core binario y un productor NativeSharp separado, las ocho combinaciones
de clase/struct y productor/consumidor O0/O2 devolvieron 42; comandos y salida
están en `delegated-two-fields-binary-20261113.log`. No se demuestra con ello
la vida de campos con movimientos dentro del delegado, llamadas arbitrarias,
tres o más asignaciones ni la reserva física del bloque compartido. Todas las
rutas de evidencia citadas están bajo `A:/nsc-fixes-evidencia`.

## Cantidad de campos acreditados

Sobre `f18530010`, la reproducción `delegated-many-before-20261113` mostró
que tres o cuatro campos de valor con drop no se limpiaban tras fallar el
constructor exterior: clase y struct devolvieron 1 en O0/O2, frente a 42
esperado. El límite de dos asignaciones se retiró para esta misma forma
estructural. La comprobación usa un conjunto de ordinales para rechazar
miembros repetidos y mantiene las demás condiciones y exclusiones anteriores.

Las ocho ejecuciones de la reproducción original devolvieron 42 después del
build incremental (`delegated-many-exact-after-20261113.cmd`). La regresión
dirigida ejecutó 174 combinaciones, incluidas tres, cuatro y ocho asignaciones,
clase y struct, owner anidado, fallo, éxito, reemplazo y move posterior, en
O0/O2 (`delegated-many-final-20261113.log`). Después del último ajuste pasó
el CTest dirigido `nsc.compiler.delegated-value-field-proven`
(`delegated-many-final-ctest-20261113.log`). Productor y consumidor binarios
con Core aislado pasaron 24 combinaciones adicionales con tres, cuatro y ocho
campos (`delegated-many-binary-final-3-20261113.log`, `-4-` y `-8-`).
La prueba no cubre campos movidos dentro del delegado, condiciones, llamadas
arbitrarias, tipos solapados o delegación encadenada; esos caminos permanecen
fuera de esta prueba estructural. Las evidencias están en
`A:/nsc-fixes-evidencia`.
