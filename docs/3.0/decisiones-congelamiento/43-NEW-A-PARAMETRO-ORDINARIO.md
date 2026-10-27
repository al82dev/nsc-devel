# Congelamiento 3.0, elemento 115 — `new` entregado a un parámetro ordinario

Fecha: 2026-10-26. Revisión de partida: `e8dd0bbac`.

## Nota de trabajo

```text
HEAD: e8dd0bbac
Elemento: 115 del plan de 142 (Use(new Box(3)) no destruye el objeto; corrección, prueba focal y commit)
Contrato vigente: C-01 (todo objeto creado tiene un dueño verificable; si no lo tiene, LostOwnership)
Documento histórico que podría contradecirlo: ninguno de decisiones-congelamiento; el diagnóstico de C-01
  sólo cubría locales, asignaciones y descartes (testLostOwnershipRejected)
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  include/nativesharp/Frontend/ProjectConstructionModel.h
  src/Frontend/Bodies/StatementAnalyzer.cpp
  src/Frontend/Bodies/CallAnalyzer.cpp
  src/Frontend/ConstructionModel/ConstructionBinding.cpp
  src/Frontend/ConstructionModel/ConstructorInitializers.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

`Use(new Box(3))` con `Use(Box b)` compilaba y el `Box` nunca se destruía: el parámetro no es dueño y nadie
más lo es (sondas `A:\temp\nsc-dec\d23` E1). Destruirlo al terminar la expresión no es una salida: si el
parámetro ordinario guarda la referencia (`Kept = b;`, sonda E5), la destrucción deja un puntero colgante y
el uso posterior lee memoria liberada.

## Decisión (Alberto)

Rechazar con C-01. Un `new` directo no puede entregarse a un parámetro ordinario porque no aparece ningún
dueño verificable.

| Destino | Regla |
|---|---|
| Parámetro `owned` | Recibe ownership; la llamada es válida. |
| Parámetro `borrowed` | Sólo recibe un préstamo de un dueño existente; el `new` directo sigue rechazado. |
| Parámetro ordinario | No recibe ownership; el `new` directo se rechaza con `LostOwnership`. |
| Constructor que conserva el objeto | Campo `owned`, parámetro `owned` y transferencia explícita (`move`). |
| Función que sólo observa | Crear antes un local `raii`/`owned` y prestarlo durante la llamada. |

Es un defecto de C-01, no una limitación documentada.

## SCOPE LOCK

- **Cambia.** Un argumento que es una `ObjectCreationExpression` con dueño fresco, sin transferencia, de un
  tipo con destrucción (`typeKeyIsOwnable`), entregado a un parámetro sin calificador de ownership y por
  valor (no `in`/`ref`/`out`, no un pack), se rechaza con `DiagnosticId::LostOwnership`:

  ```
  error: the object created by 'new' reaches no owner through the parameter 'b' of 'Use', which is not
  owned and would leak; store it in a raii or owned local, pass it to an owned parameter, or return it
  through an owned return type
  ```

  Se revisa después de elegir la sobrecarga en: llamadas a métodos (estáticos, de instancia y virtuales),
  funciones de namespace, invocación de delegates (`... of delegate 'Work' ...`), `new T(...)` y el
  inicializador `: base(...)` / `: this(...)`. Un método genérico instanciado se nombra por su nombre de
  fuente (`'Invoke'`, no `'Invoke__g1__Item__h...'`).
- **Idéntico.** Los argumentos a parámetros `owned` y `borrowed`, los structs (no tienen dueño de objeto),
  los strings frescos (siguen con su temporal oculto `OwnedTemporaryBorrow`), los locales `raii`/`owned`
  prestados a un parámetro ordinario y la resolución de sobrecargas.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas `A:\temp\nsc-dec\d25`, `-O0` y
`-O2`:

| Sonda | Forma | Ahora |
|---|---|---|
| N1 (E1) | `Use(new Box(3))`, `Use(Box b)` | rechazada, `LostOwnership` |
| N2 (E5) | `Keep(new Box(3))` y `Keep` guarda `b` en un static | rechazada |
| N3 | `new Node(new Box(3))`, `Node(Box item)` | rechazada |
| N4 | `: base(new Box(3))`, `Base(Box b)` | rechazada |
| N5, N6 | método de instancia y virtual | rechazadas |
| N7 | delegate `Work(Box b)` | rechazada |
| N8 | `new` como segundo argumento | rechazada |
| N9 | `Use(borrowed Box b)` | rechazada por la regla de `borrowed` (sin cambio) |
| Y1 | `Take(owned Box b)` | 31 |
| Y2 | `Node(owned Box item) { Item = move(item); }`, campo `owned` | 31 |
| Y3 | `raii Box b = new Box(3); Use(b);` con `Use(Box b)` | 31 |
| Y4 | lo mismo con `Use(borrowed Box b)` | 31 |
| Y5 | `: base(new Box(3))` con `Base(owned Box b)` y campo `owned` | 31 |
| D1 | delegate `Take(owned Box b)` | 31 |
| N10 | `Invoke<Item>(new Item())`, `Invoke<T>(T value) where T : class` | rechazada |
| N14, N15 | `new Derived()` a un parámetro `IBase` / `Root` | rechazadas |
| N16, N17 | `new Holder(new Derived())`, `Holder(IBase value)` / `Holder(Root value)` | rechazadas |
| Y6 | `Len("ab" + "cde")`, string fresco a parámetro ordinario | 5 (sin cambio) |

31 = `V * 10 + Freed`: el valor llega y el `Box` se destruye exactamente una vez.

### Proyectos existentes que ahora se rechazan

`--check-project-oop-mir` (`d25\hist.cmd`) sobre los proyectos donde `findstr` encontró un `new` como
argumento. Sólo cambia el diagnóstico nuevo; no se modificó ninguno:

| Proyecto | Resultado | Llamadas |
|---|---|---|
| `book_examples/08_binary_search_tree` | rechazado | 13 × `tree.Insert(new Student(...))` (`StudentTreeFactory.ns`) |
| `book_examples/11_deque_and_palindrome` | rechazado | 5 × `AddBack`/`AddFront(new WorkItem(...))` |
| `book_examples/12_avl_tree` | rechazado | `tree.Insert(new AvlRecord(...))` |
| `book_examples/13_binary_heap_priority_queue` | rechazado | `queue.Enqueue(new Job(...))` |
| `book_examples/15_hash_table` | rechazado | `table.Add(new Symbol(...))` |
| `examples/project_stage56_c6`, `tests/interop_contract/c6_struct_math` | sin cambio | structs |
| `tests/cc1/c42`, `c50`, `c52`, `c63`, `c67`, `tests/integration/ns3_t41_ownership` | sin cambio | — |
| `selfhost/nsc` | no medible aquí (1) | — |

(1) Falta `lib\NativeSharp.Core.IO\NativeSharpCore.lib`. Sus `new` como argumento son de `FileId` y
`SourcePosition`, que son structs, así que la regla no los alcanza.

En los cinco libros la colección recibe el objeto por un parámetro que no es `owned`. Su corrección (parámetro
y campo `owned` con `move`, como en Y2) queda a decisión de Alberto.

### Pruebas existentes cuyo oráculo contradice la regla

No se modificó ninguna. Búsqueda de `new` como argumento en `tests/**/*.cpp`; las que pasan una clase a un
parámetro ordinario:

- `nsc_ns3_t01_null_aware_operator_tests`: `probeValue = new Node(new Node(null));` con `Node(Node value)`.
  Ejecutada: 3 de 5 checks fallan con el diagnóstico nuevo.
- `nsc_project_construction_model_tests` (`testTypedSiteOrdering`): `return new Node(new Node());` con
  `Node(Node child)`. Sonda N11 con la misma fuente: rechazada.
- `nsc_stage46_native_libraries_tests` (consumidor metadata-only): `Algorithms.Invoke<Item>(new Item())`.
  Sonda N10 equivalente: rechazada.

`nsc_stage36_recovery_tests` (`new Holder(new Derived())`) ya se rechaza por la regla de locales de C-01
(`Holder holder = new ...`, `IAdvanced advanced = new ...`; sonda N13); el diagnóstico nuevo no se suma ahí.
`nsc_project_construction_model_tests` y `nsc_stage36_recovery_tests` no enlazan en `A:\temp\nsc-dec\bt`
(`PrecompiledSemanticSnapshotCodec::decode` sin definir) y `nsc_stage46_native_libraries_tests` no se compiló;
por eso se midieron con sondas. Las demás coincidencias son structs, parámetros `owned`/`borrowed`, receptores
o pruebas de rechazo.

Regresión focal: `all.cmd f115` (96 y 30 casos, `awcheck` sólo cambia en `a4`), `asynccases.cmd f115b`
(86 PASS), `loopcases.cmd f115` (10 PASS) y `gencases.cmd f115` (76 PASS).

Pruebas nuevas: `Freeze30ContractTests::testNewToOrdinaryParameterRejected` (N1–N9) y
`testNewToOwnedParameterAccepted` (Y1–Y5 y D1, 31 cada una). Compilan en `A:\temp\nsc-dec\bt`; se ejecutan en
el gate final.

## No verificado

- Batería completa, unitarias y la matriz de proyectos: gate final.
- Un `new` que llega a un parámetro ordinario por un camino que no es argumento directo (dentro de un
  ternario o de un `??`): fuera de este elemento.

## Observado, fuera del elemento 115

Sonda N12 (`--check-project-oop-mir`): `public Node Make() { return new Node(); }` (retorno no `owned`) y
`public Node Field = new Node();` (campo no `owned`) se aceptan. Son la misma pregunta de C-01 en otros
destinos; no se tocaron.
