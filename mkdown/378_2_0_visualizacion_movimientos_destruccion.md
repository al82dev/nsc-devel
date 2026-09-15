# 378 — Visualización de movimientos y destrucción / Move and destruction debugging

## Español

El MIR conserva ahora si un owner tiene destrucción automática de alcance. El backend CodeView lo
publica como una vista nominal `raii T*`, distinta de `owned T*`: al detenerse en `move(owner)`, el
depurador muestra el owner fuente con su tipo de ciclo de vida y con el valor nulo que deja el
lowering de movimiento, mientras el parámetro destino aparece como `owned T*`.

Las entradas generadas de destrucción y la destrucción sólo-del-objeto mantienen nombres legibles
en el PDB, junto con el destructor escrito por el usuario. Esto permite seguir el traspaso, la
limpieza del parámetro owned y la destrucción final sin exponer símbolos mangled.

La fixture O0 mueve un `raii Counter` a `Consume(owned Counter)`, comprueba la ejecución con salida
42 e inspecciona en el PDB la vista `raii`, el consumidor, `~Counter`, las entradas generadas de
destrucción y el local posterior al movimiento.

## English

MIR now retains whether an owner has scope-bound automatic destruction. The CodeView backend emits
that contract as a nominal `raii T*` view, distinct from `owned T*`: when stopped at
`move(owner)`, the debugger shows the source owner's lifecycle type and the null value left by move
lowering, while the destination parameter appears as `owned T*`.

Generated destruction and object-only destruction entries retain readable PDB names alongside the
user-written destructor. This makes it possible to follow transfer, owned-parameter cleanup, and
final destruction without exposing mangled symbols.

The O0 fixture moves a `raii Counter` into `Consume(owned Counter)`, verifies execution with exit
code 42, and inspects the PDB for the `raii` view, consumer, `~Counter`, generated destruction
entries, and the post-move local.
