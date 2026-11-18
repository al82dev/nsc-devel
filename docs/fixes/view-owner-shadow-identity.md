# Identidad del propietario de una vista

El análisis rechazaba mover o reemplazar un local independiente cuando una
vista activa pertenecía a otro local con el mismo nombre en un ámbito exterior.
La comparación de `viewRootLocalName` usaba el nombre sin resolverlo en el
ámbito de la vista. La reproducción con Rc está en
`A:/nsc-fixes-evidencia/shadow-owner-before.log`.

`hasActiveBorrowOfOwnerLocal` resuelve el nombre en el ámbito donde se declaró
la vista y compara el ordinal del símbolo con el dueño de la operación. La
corrección conserva el rechazo si la vista pertenece al dueño interior o al
exterior y evita confundir un parámetro con un local del mismo nombre. No cambia
la política de vida de vistas, la representación ni los contratos públicos.

La prueba `nsc.compiler.readonly-pattern-loan` contiene un dueño move-only y
una vista mínima independientes del SDK público. Los casos `shadow_unrelated_*`
ejecutaron 42 en O0 y O2, y `shadow_inner_borrowed` y
`shadow_outer_borrowed` se rechazaron. Los controles anteriores de lectura,
escritura, movimiento, duración y patrones también pasaron. La selección pública
con Rc y Arc produjo el mismo resultado y consta en
`A:/nsc-fixes-evidencia/shadow-owner-identity.log`.
La salida independiente está en
`A:/nsc-fixes-evidencia/shadow-owner-independent.log`.

`clang-tidy --checks=clang-analyzer-*` sobre el archivo actual terminó con
código 0 y seis advertencias (`shadow-owner-analyze.log`): cinco de padding en
tipos incluidos, ajenas al cambio aislado, y una de posible puntero nulo en
`viewRootLocalOfExpression`, fuera del símbolo modificado. El trayecto señalado
atraviesa una guarda que descarta `current == nullptr`; el analizador informa
después que ese mismo puntero es nulo. Se registra como falso positivo en esa
ruta, no como prueba de ausencia de otros defectos ni análisis de una revisión
aislada del commit.

Las pruebas se ejecutaron con el compilador incremental construido desde el
árbol de trabajo que contiene otros cambios provisionales; no se ha construido
una revisión aislada de este commit. Los casos no demuestran el tratamiento de
todas las fuentes de vistas, campos, alias ni nombres sombreados. No se modificó
la interoperabilidad C/C++ ni se ejecutó una batería completa.
