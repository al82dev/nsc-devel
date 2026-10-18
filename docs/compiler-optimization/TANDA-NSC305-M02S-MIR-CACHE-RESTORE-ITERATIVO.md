# Tanda NSC 3.0.5 M02S — restore iterativo del codec de caché MIR

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02S migra exclusivamente `readMirNode` desde recursión nativa a frames explícitos y completa la
emisión del marcador que el codec ya reservaba para un hijo nulo. Es un refactor interno del
round-trip C04: conserva el stream preorden, campos, child-count, marcadores, nulabilidad,
ownership y resultado booleano de lectura.

No activa la caché MIR, no conecta un consumidor nuevo, no sustituye funciones construidas por
funciones restauradas y no afirma incrementalidad real. Tampoco modifica claves, fingerprints,
schema, campos serializados, sintaxis, diagnósticos, API pública, ABI, NSMETA, HIR, MIR observable,
LLVM ni cachés de objetos. Quedan protegidos R02B5D1 y
`tests/unit/ProjectMethodBodyModelTests.cpp`. No se ejecutará baseline ni batería completa.

## Call graph y contrato observable

`restoreMirFunctionCore` entrega el remanente del payload a `restoreMirNodeTree`; éste crea un
`MirByteReader` y llama `readMirNode`. `restoreMirFunctions` sólo valida hoy que el payload pueda
round-trip: conserva la función ya construida y descarta el objeto restaurado.

El contrato histórico exacto es:

- `0xFFFFFFFF` abre un nodo y `0xFFFFFFFE` representa raíz o hijo nulo;
- los campos se leen en el mismo orden que `appendMirNodeFields`;
- el child-count crea exactamente los slots propietarios declarados;
- los hijos se consumen en preorden, de izquierda a derecha;
- marcador inválido o payload truncado devuelven `false` sin sustituir el árbol de salida;
- un fallo después de construir nodos descarta el árbol parcial iterativamente, sin reintroducir
  recursión durante el rollback;
- un marcador nulo válido sí resetea el árbol de salida;
- el marcador terminal global de un árbol no nulo puede quedar sin consumir, como antes; M02S no
  introduce una validación EOF incompatible;
- cada nodo restaurado pertenece exclusivamente al nuevo árbol `unique_ptr`.

## Corrección inseparable del serializador

El serializador ya usaba una worklist y su comentario declaraba que el sentinel distinguía un
nodo de un hijo nulo. Sin embargo, insertaba punteros nulos y después los desreferenciaba. M02S
emite `0xFFFFFFFE` cuando extrae uno de esos slots y conserva el marcador terminal global.

Esto no altera los bytes de árboles que no contienen slots nulos. Para un slot nulo no existía
payload válido previo: el proceso caía antes de terminar la serialización. La corrección pertenece
a la misma hoja porque es la mitad productora del contrato de nulabilidad que `readMirNode` ya
intentaba consumir.

## Algoritmo y complejidad

`readMirNodeRecord` lee transaccionalmente un marcador, los campos y el child-count de un único
nodo. `readMirNode` mantiene un stack de pares `(node,nextChild)`: llena cada slot en orden,
desciende sólo cuando el hijo está presente y publica la raíz al llamador únicamente al completar
todo el árbol. Si un registro posterior falla, `discardMirNodeTree` mueve los hijos a una worklist
y vacía cada vector antes de destruir el nodo; esta limpieza se limita al árbol parcial del codec
y no sustituye la migración general de ownership reservada para M02V.

La complejidad permanece `O(P+N)` para `P` bytes y `N` nodos. El ownership final sigue costando
`O(N)`. La pila nativa `O(H)` se sustituye por un vector en heap `O(H)`, donde `H` es la altura;
el algoritmo deja de depender del stack del proceso. `readMirNodeRecord` tiene 20 líneas físicas,
`discardMirNodeTree` 13, `readMirNode` 30 y ninguno se llama a sí mismo.

## Contrafactual profundo

El focus `mir-cache-restore-depth` construye una cadena de 32,768 nodos MIR. Cada nivel conserva
kind, `typeKey`, `integerValue` y presencia/valor alterno de `localOrdinal`; además declara dos
slots: la continuación profunda y un hijo nulo. La prueba serializa y restaura el árbol, recorre
iterativamente el resultado y exige campos, child-count, preorden y nulabilidad exactos.

Después muta el árbol restaurado para demostrar ownership independiente. También cubre raíz nula,
marcador inválido y payload truncado; los dos fallos deben conservar árboles centinela preexistentes.
La destrucción de los dos árboles profundos usa el helper iterativo de pruebas para no mezclar esta
hoja con M02V.

## Infraestructura y validación

La prueba reutiliza `nsc_project_oop_lowering_tests`, `nsc.project-oop-lowering`, su selector por
variable de entorno y la ruta default. No agrega fuente, target, `add_test`, catálogo, manifiesto
ni caso Builder. `MirBuildCoordinator.cpp` permanece registrado exactamente una vez en CMake raíz
y una vez en compiler-suite.

Los gates se ejecutaron con `TEMP`/`TMP` y logs en `A:\temp`:

- build focal de `nsc`: PASS; contrato de host runtime estático: PASS;
- build `nsc_project_oop_lowering_tests`: PASS;
- focus `mir-cache-restore-depth`: PASS 3/3;
- CTest `nsc.project-oop-lowering`: PASS 1/1;
- `validate_source_registration.py`: PASS, salida 0.

La primera ejecución instrumentada del focus terminó en `0xC00000FD`: el reader ya era iterativo,
pero el rollback de un payload truncado dejaba que el destructor recursivo liberara el árbol
parcial. `discardMirNodeTree` corrigió exclusivamente esa ruta; rebuild y repetición pasaron 3/3.

La evidencia final quedó en:

- `A:\temp\m02s-rebuild-nsc-after-rollback-fix.log`;
- `A:\temp\m02s-rebuild-lowering-after-rollback-fix.log`;
- `A:\temp\m02s-focus-cache-restore-depth.log`;
- `A:\temp\m02s-focus-cache-restore-depth.err.log`;
- `A:\temp\m02s-ctest-project-oop-lowering.log`;
- `A:\temp\m02s-source-registration.log`.

No se asignaron casos TL como prueba de restore porque ningún consumidor productivo llama hoy
`setMirCacheRoot`/`restoreMirFunctions`, y aun esa función sólo valida y descarta el objeto
restaurado. Estos resultados certifican el codec y su rollback; no certifican incrementalidad ni
reutilización real de funciones MIR.
