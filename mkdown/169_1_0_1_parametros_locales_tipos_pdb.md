# 169 — Parámetros, locales y tipos PDB / PDB parameters, locals and types

## Resultado / Outcome

El perfil completo `-g` conserva los nombres fuente y tipos de parámetros y variables locales,
y LLVM materializa ubicaciones y rangos CodeView consumibles por un depurador estándar. La
información sobrevive tanto en O0 como en O2 sin inventar hogares de pila: bajo optimización los
valores pueden vivir en registros o convertirse en constantes.

The full `-g` profile preserves source names and types for parameters and local variables, and
LLVM materializes standard debugger-consumable CodeView locations and ranges. The information
survives at both O0 and O2 without inventing stack homes: optimized values may live in registers
or become constants.

Esta entrega amplía, pero todavía no declara cerrado, `NSC-DEBUG-004`. Quedan para slices
posteriores los ámbitos léxicos anidados, miembros completos de structs, `this`, procedimientos
generados, checksums y remapeo reproducible de rutas.

This delivery advances, but does not yet close, `NSC-DEBUG-004`. Nested lexical scopes, complete
struct members, `this`, generated procedures, checksums and reproducible path remapping remain
for later slices.

## Archivos modificados / Files modified

### `include/nativesharp/Frontend/ProjectOopHirModel.h`

Agrega `ProjectOopHirFunction::parameterNames`. Los nombres son parte del modelo semántico que
viaja al backend y no se reconstruyen leyendo texto fuente.

Adds `ProjectOopHirFunction::parameterNames`. Names become part of the semantic model sent to the
backend instead of being reconstructed from source text.

### `include/nativesharp/Frontend/ProjectOopMirModel.h`

Agrega `ProjectOopMirFunction::parameterNames` para que el lowering LLVM reciba identidad fuente
junto con el tipo y ordinal ABI de cada parámetro.

Adds `ProjectOopMirFunction::parameterNames`, giving LLVM lowering source identity together with
each parameter's type and ABI ordinal.

### `src/Frontend/ProjectOopHirModel.cpp`

- copia nombres de parámetros ordinarios desde `ClassMemberSymbol::parameters`;
- copia nombres de parámetros lambda desde `MethodLambdaBinding`;
- conserva el nombre real de locales ordinarios, recursos `using` y variables `catch` en
  `ProjectOopHirNode::stringValue`;
- deja sin nombre los temporales sintéticos de ownership para que no aparezcan como variables del
  usuario.

- copies ordinary parameter names from `ClassMemberSymbol::parameters`;
- copies lambda parameter names from `MethodLambdaBinding`;
- preserves real names for ordinary locals, `using` resources and `catch` variables in
  `ProjectOopHirNode::stringValue`;
- leaves synthetic ownership temporaries unnamed so they do not appear as user variables.

### `src/Frontend/ProjectOopMirModel.cpp`

Transporta `parameterNames` de HIR a MIR. `lowerNode` ya copiaba `stringValue`, por lo que los
nombres locales siguen el mismo camino estable que sus tipos, sintaxis y ordinales.

Transports `parameterNames` from HIR to MIR. `lowerNode` already copied `stringValue`, so local
names follow the same stable path as their types, syntax and ordinals.

### `src/LLVM/LlvmCApi.h`

Amplía el subconjunto declarado de LLVM 23 con:

- tipos básicos, punteros, structs nominales opacos y tipos no especificados;
- `DILocalVariable` para parámetros y locales automáticos;
- `DIExpression` vacía para hogares directos;
- debug records `dbg.declare` del formato nuevo de LLVM.

Expands the declared LLVM 23 subset with:

- basic, pointer, opaque nominal struct and unspecified types;
- `DILocalVariable` metadata for parameters and automatic locals;
- an empty `DIExpression` for direct homes;
- new-format LLVM `dbg.declare` records.

Las codificaciones DWARF usadas por DIBuilder se fijan con los valores oficiales de LLVM; no son
un formato lateral ni una interpretación del depurador.

The DWARF encodings consumed by DIBuilder use LLVM's official values; they are neither a sidecar
format nor a debugger-specific interpretation.

### `src/LLVM/ProjectOopLlvmLowerer.cpp`

#### `initializeDebugInformation`

Activa explícitamente el formato nuevo de debug records de LLVM 23 antes de insertar declaraciones
de variables.

Explicitly enables LLVM 23's new debug-record format before variable declarations are inserted.

#### `createDebugSubprogram`

Construye la firma depurable con tipo de retorno y parámetros. Esto produce registros PDB de
procedimiento como `int (int, int)`, no sólo un nombre de función sin firma.

Builds the debuggable signature from return and parameter types. This produces PDB procedure
records such as `int (int, int)`, rather than a function name without a signature.

#### `debugNamedType` y `debugType`

Centralizan la traducción de claves NativeSharp a tipos de depuración:

- escalares builtin conservan nombre, ancho y signo;
- punteros conservan su pointee y tamaño Win64;
- enums conservan su nombre nominal y el ancho/signo subyacente;
- structs conservan identidad, tamaño y alineación del layout MIR;
- clases, interfaces, delegates, strings y arreglos se representan como referencias de 64 bits;
- tipos todavía no expandidos usan un nodo nominal no especificado en vez de fingir campos.

Centralize translation from NativeSharp type keys to debug types:

- builtin scalars preserve name, width and signedness;
- pointers preserve their pointee and Win64 size;
- enums preserve their nominal name and underlying width/signedness;
- structs preserve MIR layout identity, size and alignment;
- classes, interfaces, delegates, strings and arrays are represented as 64-bit references;
- types not expanded yet use an unspecified nominal node instead of fabricated fields.

#### `emitDebugParameter`

Crea un parámetro PDB con nombre fuente, ordinal empezando en uno, archivo, línea y tipo. Se llama
después de establecer el hogar real en cada ruta ABI: capture cell, puntero de agregado indirecto
o alloca ordinaria.

Creates a PDB parameter with source name, one-based ordinal, file, line and type. It runs after the
real home is established in every ABI path: capture cell, indirect aggregate pointer or ordinary
alloca.

#### `emitDebugLocal`

Crea la variable automática en el punto de declaración y enlaza su almacenamiento real mediante
un debug record. Evita duplicados por ordinal y omite temporales sin nombre. También se usa para
recursos `using` y variables tipadas de `catch`.

Creates an automatic variable at its declaration point and binds its real storage through a debug
record. It deduplicates by ordinal and skips unnamed temporaries. It also covers `using` resources
and typed `catch` variables.

### `tests/integration/debug_pdb/Main.ns`

Amplía el fixture con `Compute(int left, int right)`, el local `total`, el struct nominal
`Pair`, `Sum(Pair pair)` y los locales `answer`/`pair` en `Main`. Esto comprueba parámetros y
locales escalares, un agregado indirecto Win64 y la identidad/tamaño real de un tipo NativeSharp.

Extends the fixture with `Compute(int left, int right)`, local `total`, nominal struct `Pair`,
`Sum(Pair pair)`, and locals `answer`/`pair` in `Main`. This verifies scalar parameters and locals,
a Win64 indirect aggregate, and the real identity/size of a NativeSharp type.

### `cmake/VerifyNativeSharpDebugArtifacts.cmake`

Endurece `nsc.debug.codeview-pdb` para exigir:

- `Compute`, `Sum` y `Main` con sus líneas ejecutables;
- `left` y `right` como `S_LOCAL` marcados `param` y tipados `int`;
- `total` como local y `answer` como local o constante optimizada;
- `pair` como parámetro/local, `DebugFixture.Pair` como `LF_STRUCTURE` de 8 bytes y la firma
  `int (DebugFixture.Pair)`;
- al menos un `S_DEFRANGE` real;
- ausencia de `S_LOCAL`/`S_DEFRANGE` en el PDB `-gline-tables-only`.

Hardens `nsc.debug.codeview-pdb` to require:

- `Compute`, `Sum`, and `Main` plus their executable lines;
- `left` and `right` as `int`-typed `S_LOCAL` records marked `param`;
- `total` as a local and `answer` as either a local or optimized constant;
- `pair` as a parameter/local, `DebugFixture.Pair` as an 8-byte `LF_STRUCTURE`, and the
  `int (DebugFixture.Pair)` signature;
- at least one real `S_DEFRANGE`;
- no `S_LOCAL`/`S_DEFRANGE` in the `-gline-tables-only` PDB.

## Por qué esta implementación / Why this implementation

Los debug records se atan al mismo almacenamiento que usa el programa. En O0 LLVM/PDB publica
hogares relativos al frame; en O2 recalcula rangos después de optimizar y publica registros o
constantes. Esto evita el error peligroso de mantener una dirección de pila obsoleta sólo para que
el depurador siempre muestre algo.

Debug records bind to the same storage used by the program. At O0 LLVM/PDB publishes frame-relative
homes; at O2 it recomputes ranges after optimization and publishes registers or constants. This
avoids the dangerous mistake of retaining a stale stack address merely so the debugger always
shows a value.

Transportar nombres dentro de HIR/MIR conserva una sola fuente semántica y cubre genéricos y
lambdas sin reparsear. Los structs se publican primero como tipos nominales con layout verdadero;
sus miembros se agregarán cuando exista un gate que compruebe offsets uno por uno.

Transporting names inside HIR/MIR preserves one semantic source and covers generics and lambdas
without reparsing. Structs are initially published as nominal types with truthful layout; their
members will be added together with a gate that verifies every offset.

## Evidencia enfocada / Focused evidence

- build de `nsc` y los 135 ejecutables de prueba;
- `nsc.project-oop-lowering`, `nsc.macro-a-oop`, `nsc.stage43.lambdas-closures` y
  `nsc.debug.codeview-pdb`: `4/4` verdes;
- PDB O0: parámetros `left`/`right` y locales `total`/`answer` con
  `S_DEFRANGE_FRAMEPOINTER_REL`;
- PDB O2: `left` en `ECX`, `right` en `EDX`, `total` en `EAX` y `answer` como constante 42;
- PDB O0: `DebugFixture.Pair` conserva identidad nominal, `sizeof 8` y participa en la firma
  `int (DebugFixture.Pair)`; `pair` aparece tanto como parámetro como local;
- PDB line-tables-only: procedimientos y líneas presentes, variables y defranges ausentes.

## Baseline oficial / Official baseline

Corrida `20260909-001145-a3e93c80`, construida e instalada desde el contenido actual del disco:

- CTest lld-link: `307/307`;
- fixtures lld-link: `148/148`;
- triage lld-link: `18/18`;
- CTest NSLink: `307/307`;
- fixtures NSLink: `148/148`;
- triage NSLink: `18/18`;
- Gate 0: `LANDED`;
- duración: `773.63 s`;
- reporte: `_nscbuilder/history/20260909-001145-a3e93c80/report.md`.

Run `20260909-001145-a3e93c80`, built and installed from the current on-disk sources:

- lld-link CTest: `307/307`;
- lld-link fixtures: `148/148`;
- lld-link triage: `18/18`;
- NSLink CTest: `307/307`;
- NSLink fixtures: `148/148`;
- NSLink triage: `18/18`;
- Gate 0: `LANDED`;
- duration: `773.63 s`;
- report: `_nscbuilder/history/20260909-001145-a3e93c80/report.md`.

## Commit de referencia / Reference commit

**Título ES:** Agrega parámetros, locales y tipos al PDB estándar

**Title EN:** Add parameters, locals and types to the standard PDB

**Descripción ES:** Transporta nombres fuente de HIR a MIR, modela tipos NativeSharp con DIBuilder,
emite declaraciones de parámetros y locales sobre sus hogares ABI reales y endurece el gate para
validar tipos y rangos en O0/O2 sin contaminar line-tables-only.

**Description EN:** Transports source names from HIR to MIR, models NativeSharp types with DIBuilder,
emits parameter and local declarations over their real ABI homes, and hardens the gate to validate
types and ranges at O0/O2 without polluting line-tables-only.
