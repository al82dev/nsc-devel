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
