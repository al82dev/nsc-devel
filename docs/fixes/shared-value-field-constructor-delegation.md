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
