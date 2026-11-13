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
