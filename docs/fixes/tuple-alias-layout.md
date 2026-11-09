# Alias de tuplas y estado de namespace

Al preparar los controles de almacenamiento estático de los punteros se
reprodujeron tres fallos independientes de la API de punteros: un alias de tupla
perdía su marca estructural, un alias encadenado perdía la tupla de destino y
el estado de namespace de tipo tupla carecía de layout durante la emisión.
Las reproducciones se conservaron en `shared_public_pointers.py` y se añadieron
controles independientes en `tuple_value_drop.py`.

La expansión conserva ahora la marca y nombres de elementos de tupla; reconoce
un alias nominal que apunta a un alias estructural ya registrado en su ámbito y
normaliza ese destino en el ámbito de declaración. El importador MIR recopila
layouts de tuplas usadas por estados de namespace que tienen sintaxis disponible.
No se cambia la clasificación de un alias ordinario de namespace en los casos
ensayados.

El build incremental de `nsc` terminó correctamente. Las ejecuciones dirigidas
O0/O2 de tuplas locales, estáticas, anidadas, con nombres, cadenas de dos y tres
alias, sombreado y alias de namespace pasaron; se incluyen controles previos de
destrucción (`A:/nsc-fixes-evidencia/tuple-alias-regression.log`). Los controles
de `Rc` con campos de namespace y `WeakArc` también pasaron, y la variante
con `WeakRc` se rechazó según la regla de almacenamiento compartido
(`tuple-alias-shared.log`). La prueba dirigida de alias genéricos pasó 15
comprobaciones (`tuple-alias-unit.log`). Las pruebas se ejecutaron con el árbol
de trabajo que también contiene otros cambios todavía no registrados; no se
verificó un build separado de estos cambios.

Por error, la misma selección de pruebas existentes incluyó
`nsc.ns3.t04.type-aliases`, que contiene una declaración `extern C` y se ejecutó
en el build aislado. Su inclusión no correspondía al alcance; no se usa para
afirmar cobertura de interoperabilidad y debe excluirse de ejecuciones futuras.

El reconocimiento de la cadena se limita a alias estructurales ya registrados
al recorrer las declaraciones: no demuestra referencias adelantadas, ciclos
nominales, coincidencias ambiguas entre namespace y alias, ni todos los usos
de nombres de elementos. El importador se limita a declaraciones de namespace
con sintaxis fuente; no demuestra importación desde módulos precompilados. Las
familias de punteros, su validación binaria y las reglas de vida de vistas
siguen abiertas.
