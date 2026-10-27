# Congelamiento 3.0, elementos 115, 115a y 115b — `new` sin dueño (parámetro, retorno, campo)

Fecha: 2026-10-26. Revisión de partida: `e8dd0bbac`.

## Nota de trabajo

```text
HEAD: e8dd0bbac
Elemento: 115, 115a y 115b del plan de 144 (Use(new Box(3)) no destruye el objeto; `return new` con retorno no
  owned; campo no owned inicializado con `new`)
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
  src/Frontend/Bodies/MethodBodyBuildCoordinator.cpp
  tests/unit/Freeze30ContractTests.cpp
  tests/unit/NativeSharp3T01NullAwareOperatorTests.cpp, ProjectConstructionModelTests.cpp,
    Stage46NativeLibrariesTests.cpp (oráculos que contradecían la regla; autorizado por Alberto)
  tests/unit/DriverTests.cpp, MacroAOopTests.cpp, NativeSharp3T05TraitDeclarationTests.cpp,
    ProjectObjectLayoutModelTests.cpp, ProjectOopLoweringTests.cpp, Stage36RecoveryTests.cpp,
    Stage37StaticInitializationTests.cpp, Stage38PropertiesIndexersOperatorsTests.cpp,
    Stage44GenericsMonomorphizationTests.cpp, Stage47HostCAbiTests.cpp, Stage48_3CHostIntegrationTests.cpp,
    Stage48_4CppHostIntegrationTests.cpp, Stage59ClosedGenericMemberTests.cpp,
    Item322NoHeapPropagationTests.cpp (fixtures con `return new`/campo `new`; autorizado por Alberto)
  tests/cases/{backend,ir,project,projects}/... (9 fuentes .ns; autorizado por Alberto)
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

Al medir el impacto aparecieron dos destinos más con la misma hipótesis, que Alberto agregó como 115a y 115b
(sonda `d25\N12`):

- 115a: `return new T()` desde un callable sin retorno `owned`. Se rechaza con `LostOwnership`; el retorno `owned` es la
  forma válida.
- 115b: campo de instancia sin `owned` inicializado con `new T()`. Se rechaza con `LostOwnership`; el campo `owned` es
  la forma válida.

La autorización cubre sólo esos dos destinos. Un campo `static` inicializado con `new` (sonda `d25\F3`, 3) no
se reprodujo como defecto y no cambia.

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
- **Cambia (115a).** `return new T()` en un callable cuya declaración no tiene retorno `owned`
  (`rejectNewToNonOwnedReturn`): `the object created by 'new' reaches no owner through the return of 'Make',
  whose return type is not owned ...`.
- **Cambia (115b).** Un campo de instancia sin `owned` cuyo inicializador es `new T()` (`analyzeFieldInitializer`):
  `... through the field 'Item', which is not owned ...`.
- **Excepción.** Un llamado `__nsc_` es una traducción del compilador (la fábrica del resultado opaco
  `some Trait` reescribe `return new X()` como `__nsc_from_representation(new X())`); la regla del 115 no se
  aplica a ese parámetro sintético.
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
| R1 (115a) | `public static Box Make() { return new Box(3); }` | rechazada |
| R2 (115a) | `public static owned Box Make()` y `raii Box b = Make();` | 31 |
| F1 (115b) | `class Holder { public Box Item = new Box(3); }` | rechazada |
| F2 (115b) | `public owned Box Item = new Box(3);` | 31 |
| F3 | `public static Box Shared = new Box(3);` | 3 (sin cambio, fuera de la autorización) |
| N12 | `public Node Make() { return new Node(); }` y `public Node Field = new Node();` | rechazadas (115a y 115b) |

31 = `V * 10 + Freed`: el valor llega y el `Box` se destruye exactamente una vez.

### Proyectos que no son pruebas

`--check-project-oop-mir` (`d25\hist.cmd`). No se modificó ninguno (Alberto: los ejemplos quedan fuera por
ahora):

- `book_examples/08`, `11`, `13`, `15`: rechazados por el 115 (`Insert`/`AddBack`/`Enqueue`/`Add(new X(...))`)
- `book_examples/12_avl_tree`: rechazado por el 115 (`Program.ns:11`) y por el 115a (`AvlTree.InsertNode`)
- `examples/project_stage56_c6`, `tests/interop_contract/c6_struct_math`, `tests/cc1/c42`, `c50`, `c52`, `c63`, `c67`,
  `tests/integration/ns3_t41_ownership`: sin cambio
- `selfhost/nsc`: no medible aquí (falta `lib\NativeSharp.Core.IO\NativeSharpCore.lib`); sus `return new` ya
  son `owned`
- `lib/NativeSharp.Core`, `Collections.Generic`, `Threading`: sin cambio (compilan)

### Fixtures ajustados (autorización de Alberto)

Regla: si el callable conserva o consume el objeto, `owned` (y `move` desde un local); si sólo lo observa, un
local `raii` prestado. Ninguna aserción cambió.

- `nsc_ns3_t01_null_aware_operator_tests`: `Node(owned Node value) { next = move(value); Link = next; }`, campo `next`
  `owned`; sale `destroy tail` (la cabeza es dueña)
- `nsc_project_construction_model_tests`: `Node(owned Node child)`, campo y retornos `owned` en los casos positivos
  (Primary, PrivateSameType, DefaultApp/Core, PublicAlias, GlobalConstruction, ConstantRanking, NullReferenceArgument,
  ImplicitChain, TypedSiteOrdering)
- `nsc_stage46_native_libraries_tests`: metadata-only: `raii Item item` y `Invoke<Item>(item)`; genérico: `owned Box<T>
  Create`, `raii Box<int> box`, `borrowed ITaggedValue<int> tagged`
- `nsc_driver_tests`: fábricas de Stage28/29/30 con retorno `owned`
- `nsc_project_object_layout_model_tests`: fábricas de los casos positivos con retorno `owned`
- `nsc_project_oop_lowering_tests`: `Make`, `MakeAfterAdd`, `MakeEmpty` con retorno `owned`
- `nsc_stage37_static_initialization_tests`: `MakeLeaf`, `MakeDelegating` con retorno `owned`
- `nsc_stage38_properties_indexers_operators_tests`: operadores `+`, `-` y la conversión explícita `owned`; `raii` en
  `c`, `d`, `casted`, `borrowed IValue iv`; salen los `delete`
- `nsc_stage44_generics_monomorphization_tests`: `owned Box<T> Create`, `raii Box<int> box`, `borrowed ITaggedValue<int>
  tagged`
- `nsc_stage59_closed_generic_member_tests`: `owned Caja<T> Make`, locales `raii`; `Par` en un local `raii` prestado a
  `Envolver` (115)
- `nsc_ns3_t05_trait_declaration_tests`: `owned Number operator +` y `raii Number sum`
- `nsc_stage47_host_c_abi_tests`, `nsc_stage48_3_c_host_tests`, `nsc_stage48_4_cpp_host_tests`: `export extern C owned
  Counter ...Create` (como `examples/project_stage47`)
- `nsc_item322_noheap_propagation_tests`: `field_initializer`: `public owned Box Value = new Box();`
- `nsc_macro_a_oop_tests`, `nsc_stage36_recovery_tests`: fábrica con retorno `owned` (ver abajo)
- `tests/cases` stage29, 30, 31, 34, 36 (ir y backend), 38 (ir y backend): fábricas y operadores con retorno `owned`;
  stage38 queda igual a `examples/project_stage38` (`raii`/`borrowed`, sin `delete`)

Ejecutadas en `A:\temp\nsc-dec\bt` (`runut.cmd u2`): T01 5/5, T05 opaque 14/14, T05 trait 14/14, Stage47,
Stage48_3, Stage48_4 e Item322 sin fallas. Stage46: 453/456; quedan dos fallas DLL-P4 por un `throw` sin
`throws` (C-13, no relacionado) y la de `tagged`, corregida después en la fuente (sonda `d28\Stage44GIH`, misma
forma). `nsc_driver_tests`: las 40 verificaciones de Stage28–30 pasan; las 67 que siguen fallando usan fuentes
sin `new` (`int Main()` global, `class Widget {}`), igual que antes del ajuste. `nsc_ns3_t09`: 2 fallas por
`Span.RefAt` inexistente, no relacionadas.

No enlazan en `bt` (`PrecompiledSemanticSnapshotCodec::decode` sin definir): construction model, object
layout, macro A, stage35–38, stage44, stage59 y oop lowering. Sus fuentes ajustadas se compilaron con
`--check-project-oop-mir` (`d28\chk.cmd`): todas pasan salvo `MacroA`, que ya se rechazaba antes por la regla
de locales de C-01 (`Counter counter = new Counter(3);`). `Stage36` también se rechazaba antes por la misma
regla (sonda N13). Esos dos fallos no son de este elemento y no se tocaron más allá del retorno `owned`.

Los 27 proyectos de `tests/cases` (`d25\cases.cmd`, `--verify-project-oop-llvm`) verifican.

## No verificado

- Batería completa, unitarias y la matriz de proyectos: gate final.
- Un `new` que llega a un parámetro ordinario por un camino que no es argumento directo (dentro de un
  ternario o de un `??`): fuera de este elemento.

## Pruebas nuevas

- 115: `Freeze30ContractTests::testNewToOrdinaryParameterRejected` (N1–N9) y `testNewToOwnedParameterAccepted`
  (Y1–Y5 y D1, 31 cada una).
- 115a: `testNewToNonOwnedReturn` (rechazo y retorno `owned`, 31).
- 115b: `testNewToNonOwnedField` (rechazo y campo `owned`, 31).

Compilan en `A:\temp\nsc-dec\bt`; se ejecutan en el gate final.

## Observado, fuera del elemento 115

- Un resultado opaco `some Trait` que devuelve `new X()` pasa por `__nsc_from_representation`; si el
  envoltorio no es dueño de su representación el objeto no se destruye. No se verificó.
