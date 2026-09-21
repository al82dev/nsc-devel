# Tanda 47 — SH45: paridad low-level NativeSharp 2.0

| Campo | Valor |
|---|---|
| Estado | `STAGED-COMMITTED` — implementación y baseline aislado aprobados; integración lineal espera el cierre físico de SH44 |
| Base de trabajo | T45/SH43 `c99094a450bb` en worktree aislado `t47-staging` |
| Dependencia normativa | T46/SH44, incluida su atestación de segunda máquina |
| Gate | SH45 |
| Instalación de trabajo | `A:\native-sharp` |
| LLVM | `C:\clang-llvm` (sólo lectura) |

## Objetivo

Cerrar en el compilador autocontenido la superficie low-level exigida por NativeSharp 2.0:
`volatile`, atomics, unions, bitfields, layout explícito, placement construction, destrucción sin
liberar almacenamiento, `noheap` y rechazo contractual de referencias que escapan de su región.
El cierre exige código máquina observable en O0–O3, enlace por LLD y NSLink y un nuevo punto fijo
`nsc1 → nsc2 → nsc3` posterior a la implementación.

## Diseño e implementación

### Memoria, layout y bitfields

`SemanticAnalyzer.DeclareAggregate()` conserva `pack`, `size`, `at`, unions y contenedores de
bitfields en un único modelo de offsets. Los campos estáticos reciben layout físico sin participar
en el tamaño de instancia. Un ancho mayor que el tipo declarado es ahora un error semántico.

`LlvmBackend.LoadMemory()` y `StoreMemory()` centralizan el contrato de memoria. Aplican alineación
y `volatile` tanto a la carga como al read-modify-write. Los bitfields cargan el contenedor físico
i8/i16/i32/i64, extraen máscara y desplazamiento, extienden el signo cuando corresponde y truncan
`bool` a i1. La escritura ensancha `bool`, preserva los bits vecinos y reemplaza sólo su rango.

### Atomics

Los bindings LLVM incorporan `LLVMBuildAtomicRMW`, `LLVMBuildAtomicCmpXchg`,
`LLVMBuildExtractValue`, `LLVMBuildFence` y `LLVMSetOrdering`, además de
`LLVMAtomicRMWBinOp`. El backend reconoce `AtomicInt32`, `AtomicInt64`, `AtomicPointer`,
`Interlocked` y `AtomicFence`; emite load/store atómicos, exchange, add, compare-exchange y fences
con alineación explícita. La auditoría del objeto prohíbe los fallbacks `__atomic_load` y
`__atomic_store` y exige instrucciones `lock`/`xchg` en el desensamblado AMD64.

### Placement y destrucción

`Parser.ParseUnary()` acepta `new (storage) Type(args)` y crea
`PlacementConstructionExpression`. El análisis exige que `storage` sea exactamente `Type*`. El
backend inicializa en cero el almacenamiento suministrado y llama al constructor con ese puntero;
no solicita heap.

Los destructores ya no desaparecen durante el bootstrap. `DestructorDeclaration` tiene símbolo,
firma `void`, receptor implícito, reachability, declaración LLVM y cuerpo emitido. `destroy value`
valida el tipo, llama el destructor concreto y conserva el almacenamiento del llamador. La fixture
demuestra el efecto `42 → 7` sobre memoria obtenida con `stackalloc`; un `destroy int` se rechaza.

### `noheap` y escapes

Los símbolos conservan `IsNoHeap()` y el análisis de una función `noheap` rechaza allocation,
lambda, concatenación de strings y propagación a callables sin contrato `noheap`. El análisis de
escape rechaza `return ref` de locales, parámetros por valor y parámetros readonly, pero permite
la región del llamador representada por `ref`.

### Correcciones encontradas por ejecución

- el receptor de `->` se desreferenciaba dos veces; ahora el backend elimina el wrapper `arrow`
  antes de calcular el offset del campo;
- atomics sin alineación explícita producían llamadas de fallback; la alineación contractual evita
  esos símbolos y conserva las instrucciones nativas;
- `stackalloc` constante usa un `alloca` de tipo arreglo y no una cuenta dinámica;
- el primer prototipo de `destroy` era un no-op; la fixture con destructor observable impidió
  aceptarlo como paridad;
- los bitfields `bool` no pueden cargar i1 cuando comparten un byte; el backend usa el tipo del
  contenedor y convierte sólo en la frontera del valor.
- el lector NSMETA acepta de forma explícita cabeceras LF y CRLF;
- los oráculos de IR ya no incorporan fuentes generadas dentro de árboles `build-*`;
- la comparación AST normaliza sólo líneas vacías de ensamblador crudo, sin relajar su estructura.

## Archivos, clases y funciones

| Archivo | Responsabilidad principal |
|---|---|
| `selfhost/nsc/Parser/Parser.ns` | placement, `return ref`, tipos `ref` y destructores |
| `selfhost/nsc/Symbols/Symbols.ns` | `Destructor`, flags `volatile`/`noheap` y layout de bitfields |
| `selfhost/nsc/Sema/SemanticAnalyzer.ns` | layout, placement, destroy, noheap y escape analysis |
| `selfhost/nsc/Sema/SemanticDumper.ns` | superficie verificable de flags, offsets y anchos |
| `selfhost/nsc/Sema/ModuleSet.ns` | identidad callable de destructores entre unidades |
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | lowering de memoria, bitfields, atomics, placement y destroy |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | funciones LLVM C requeridas por atomics |
| `lib/NativeSharp.LLVM.C/LLVMEnums.ns` | operaciones atomic RMW |
| `selfhost/contracts/llvm-c-api.contract.json` | hashes y cobertura auditada del binding |
| `selfhost/contracts/low-level-parity.contract.json` | matriz positiva, negativos, metadata y código máquina |
| `selfhost/bootstrap/verify_low_level_parity.py` | emisión, auditoría, enlace, ejecución y reporte |
| `cmake/VerifyNativeSharpLowLevelParity.cmake` | gate reproducible configurable |
| `selfhost/tests/low-level-2.0/` | fixtures aisladas, integrada y negativas |
| `tests/selfhost/test_bootstrap_infrastructure.py` | congelamiento estructural del contrato |

Métodos críticos: `DeclareAggregate`, `DeclareFunction`, `AnalyzeExpression`, `ValidateNoHeap`,
`ValidateEscapes`, `LoadMemory`, `StoreMemory`, `ApplyMemoryContract`, `EmitLValue`,
`EmitScalarExpression`, `MarkReachableNode`, `DestructorDeclaration`, `AtomicOrderingFor` y
`EmitCoreIntrinsic`.

## Gate dirigido

El compilador modificado volvió a cerrar SH41 antes de validar SH45.

| Medición | Resultado |
|---|---:|
| fuentes del compilador | 101 |
| perfiles de autocompilación | 4 |
| ejecuciones SH41 | 16 |
| objetos `nsc2`/`nsc3` idénticos | 4/4 |
| invocaciones de `nsc0` durante generación | 0 |
| compiladores productores probados | 4 (`nsc2` O0–O3) |
| perfiles por productor | 4 |
| proyectos positivos | 5 |
| linkers | 2 |
| ejecuciones low-level aprobadas | 160/160 |
| rechazos contractuales | 64/64 |
| pruebas de infraestructura | 46/46 |
| gate CMake | aprobado |
| fallos / omitidos | 0 / 0 |

SHA-256 de fuentes SH41: `213ac08ceb2b4ea3975d87fea7d63138c1c836e0bde1c4e2a42611198009556f`.
SHA-256 del contrato SH45: `cc337a752eb5304276ab482c764e4c01b36fe8aa577340e8f05bc79362a0fd38`.
La evidencia resumida está en `selfhost/evidence/gates/SH45/sh45-summary.json`.

## Baseline e integración

El baseline aislado `tanda-47-sh45-provisional-r3-20260921` terminó `PASSED` en 3866.614 s:
479/479 CTests y 166/166 fixtures por linker, triage completo y Gate 0 `LANDED`, sin fallos ni
omitidos. LLD consumió 1885.922 s y NSLink 1824.628 s. El baseline acumulado definitivo sobre la
línea principal no se declara todavía: T46/SH44 tiene implementación y baseline local verdes, pero
su gate exige una atestación física de la segunda PC. Para no alterar ni mezclar su worktree, T47
se desarrolló en `A:\nsc-devel-t47-worktree`. Cuando llegue
`machine-attestation.json`, el orden obligatorio es:

1. cerrar y commitear T46 sobre `development-claude`;
2. reubicar el único commit T47 encima de ese commit;
3. ejecutar el baseline completo LLD/NSLink de T47;
4. sustituir `STAGED-COMMITTED` por `LANDED`, registrar la corrida integrada y enmendar el único
   commit T47 antes de incorporar la pila a `development-claude`.

No se consideran evidencia los objetos, ejecutables, `.lib`, `.rsp`, logs ni directorios `out`.

## Criterios de cierre

- [x] volatile y alineación observables en LLVM;
- [x] atomics nativos sin fallback `__atomic_*`;
- [x] unions, pack, size, offsets y bitfields unsigned/signed/bool;
- [x] placement construction sin heap y destrucción explícita observable;
- [x] contratos `noheap` y escape analysis con negativos;
- [x] O0–O3, LLD y NSLink con los cuatro compiladores autocontenidos;
- [x] nuevo punto fijo SH41 posterior a los cambios;
- [ ] integración lineal después de SH44;
- [x] baseline completo aislado LLD/NSLink, 0 fallos y Gate 0 aprobado;
- [x] commit aislado `selfhost(tanda-47): close NativeSharp 2.0 low-level parity`;
- [ ] baseline acumulado definitivo e integración en `development-claude`.
