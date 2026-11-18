# Campo de valor predeterminado en constructor fallido

En `daab4eb89`, la reproducción conservada
`normal_factory_base_value_default_failure` volvió a fallar: `Part` es un
struct de usuario sin inicializador de campo y su destructor no se ejecuta al
fallar el constructor; Rc O0 devuelve 3 en lugar del 42 esperado. El control
de salida normal devuelve 42 en O0/O2. El log es
`A:/nsc-fixes-evidencia/shared-default-field-current.log`.

Se ensayó marcar el inicio de vida de campos de valor sin inicializador en
orden de declaración. La reproducción pasó en Rc y Arc en O0/O2, así como 16
casos nuevos de orden y 32 controles de construcción parcial y movimiento
(`shared-default-field-ensayo.log`, `default-value-field-order.log` y
`default-value-field-controls.log`). Sin embargo, el control existente
`shared_default_identity.py` falló en Rc/direct/O0: destruyó un tipo de
usuario no inicializado (`default-value-field-identity.log`). Ese control
preserva la decisión expresa de reconocer un estado vacío destructible para
las cuatro familias del SDK sin alterar la regla de otras estructuras.

Por esa contradicción, se retiró el ensayo del HIR y backend, se conservó su
reproducción en `A:/nsc-fixes-evidencia/default-value-field-prototype.py` y se
recompiló incrementalmente el compilador. El control de identidad volvió a
pasar sus 16 combinaciones (`default-value-field-identity-restored.log`), y
ambos archivos de implementación quedaron sin diff. No hubo una corrección
funcional ni validación de SDK binario para este caso.

La expectativa de destrucción de `Part` durante la construcción fallida y la
regla anterior para otras estructuras necesitan reconciliarse; no se cambia
el contrato general ni se presenta el caso como resuelto. El ensayo también
coexistió con cambios concurrentes sin registrar, de modo que sus resultados
no certifican un árbol limpio. No se ejecutó la batería completa.

La selección se repitió con el compilador aislado SHA-256
`dd8ef173cd74345f8d888052033ec37b7d66dde9ced36fd2cb332d70d0b3785c`, usando
fuentes de las familias en esta copia y O2. El control de construcción normal
devolvió 42 con Rc y Arc; la construcción fallida devolvió 3 con ambas
familias, frente al 42 que espera el fixture. Los registros individuales son
`A:/nsc-fixes-evidencia/pointers-default-field-Rc-current.log` y
`A:/nsc-fixes-evidencia/pointers-default-field-Arc-current.log`; cada selección
terminó fallida. Esto confirma la observación para el árbol actual, sin decidir
que el destructor de una estructura ordinaria no inicializada deba ejecutarse:
esa expectativa sigue en conflicto con el control de identidad descrito arriba.
No se cambió la regla expresa del usuario para los cuatro tipos ni se regeneró
el SDK binario en esta comparación.

La regresión `shared_binary_field_constructor_failure.py` se amplió para
comprobar dos campos de la misma familia ya inicializados, fallo durante la
inicialización del segundo campo y un constructor que contiene simultáneamente
`Rc`, `WeakRc`, `Arc` y `WeakArc`. Con el compilador aislado SHA-256
`f8d6020ee7a69d327a21c7cc339eef43306001d8a67e2f25f542269a5afe8580` y el
Core binario preexistente SHA-256
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`, las
34 ejecuciones de la selección devolvieron 42 en O0/O2. En los escenarios de
dos campos, los contadores vuelven al valor del dueño restante; el escenario
mixto verifica ambos contadores fuertes y débiles. Comando y salida:
`A:/nsc-fixes-evidencia/shared-binary-field-constructor-current.cmd` y `.log`.

El primer fixture de fallo en el segundo campo omitía declarar `throws` en el
constructor y fue rechazado en análisis. Se corrigió esa declaración antes de
la ejecución indicada. Estas pruebas verifican contadores y captura en los
casos concretos, no todas las formas de inicialización ni la ausencia general
de fugas; el Core no se regeneró en esta tanda.

## Evidencia histórica adicional con Core binario

El commit `a66e283516b47da4383725feb0ff40d4fd745a46`, de la copia
`A:\nsc-devel-fixes-agente-2`, documentó otra repetición con un compilador en
curso y Core binario aislado. Los ocho programas compilaron: las cuatro
construcciones normales de Rc y Arc en O0/O2 devolvieron 42 y las cuatro
construcciones fallidas devolvieron 3. Los cuerpos procedían de
`shared_public_pointers.py`, con `using NativeSharp.Core;` añadido. Fuentes,
comandos, salidas y hashes antes y después están en
`A:/nsc-fixes-agente-2-evidencia/default-field-pointer-20261112/run-20261112T041528563716Z/results.json`.

Como contraste, se repitieron los ejemplos con `Part field = new Part(1)`:
construcción exitosa, fallo antes de inicializar y fallo después de inicializar,
para Rc y Arc en O0/O2. Las 12 ejecuciones devolvieron 42. La evidencia está en
`A:/nsc-fixes-agente-2-evidencia/explicit-field-pointer-20261112/run-20261112T041807519599Z/results.json`.

El commit registra el origen de esta evidencia; no identifica una revisión
limpia y exacta del compilador utilizado. Los resultados se obtuvieron con
trabajo en curso y no certifican la rama integrada ni el SDK actual. La primera
matriz confirma el resultado observado, pero no determina cuándo comienza la
vida de un campo ordinario sin inicializador. El contraste cubre únicamente
los campos con inicialización explícita y no resuelve la decisión pendiente
sobre la destrucción de otras estructuras.
