# NS304-T03-F03 — list patterns sobre colecciones nativas y protocolo usuario

Estado: `EN IMPLEMENTACIÓN` (los siete sustratos ejecutables pasan individualmente; permanecen
abiertos los oráculos contractuales de UTF-8, alias/no-copia observable y escape negativo).

## Evidencia focal de sustratos — 2026-10-13

- Casos canónicos independientes: `TL-LANG-310`, `TL-LANG-357`–`TL-LANG-362`.
- Matriz dirigida: **7/7 PASS** con `nsc_component_tests --case <ID>`.
- `TL-LANG-358` fuerza las dos operaciones de fixed array: local mutable → `Span<int>` y
  parámetro `in int[4]` → `ReadOnlySpan<int>`. Ambos slices empiezan en índice 1 y validan
  `Length()` e indexación; además cubre el límite vacío `start == extent`, `count == 0`.
- El lowering LLVM construye `{data,count}` sobre la dirección del almacenamiento inline, sin
  reservar ni copiar elementos, y valida `start/count` contra el extent del tipo fijo.
- Esta evidencia no certifica por sí sola todos los oráculos del expediente: el fixture string
  actual es ASCII, los casos positivos no observan alias mediante mutación posterior y todavía no
  existe aquí el negativo de escape desde `stackalloc`. Por ello F03 continúa abierto.

## Cierre parcial B1 — identidad de especializaciones

- `closedTypeKeyForTemplate` conserva la identidad canónica completa del argumento en la clave y
  el hash. El nombre terminal se usa únicamente para mantener legible el símbolo generado.
- Los argumentos que ya son especializaciones usan `generic:<specializationKey>`, igual que la
  ruta ordinaria de `ensureTypeSpecialization`; los builtins conservan su identidad histórica.
- Evidencia puntual 2026-10-13: `TF-FRONT-167` PASS, 141,403 us. Verifica estabilidad y ausencia
  de colisión entre tipos `Item` homónimos de módulos o namespaces diferentes.
- Este cierre no materializa Span/ReadOnlySpan desde cuerpos y no cierra `TL-LANG-310` ni F03.

## Cierre parcial B2 — materialización solicitada desde cuerpos

- `ProjectGenericModel` recoge las demandas estructuradas `list-pattern-rest` durante su recorrido
  serial del AST y las procesa en olas ordenadas antes de name/member binding y Sema.
- Cada demanda usa la clave canónica B1 y la caché ordinaria de especializaciones; solicitudes
  repetidas no crean tipos cerrados duplicados ni materializan tipos ad hoc desde Sema.
- Como la mutabilidad del receptor se decide en Sema, la fase previa materializa el conjunto finito
  `Span<T>`/`ReadOnlySpan<T>` y el análisis del cuerpo enlaza únicamente la vista correspondiente.
- Evidencia puntual 2026-10-13: `TF-FRONT-167` PASS, 113,509 us. El fixture llega a
  `MethodBodies`, solicita ambas vistas desde cuerpos y demuestra una sola especialización de cada
  tipo aunque la captura rest se repita.
- Este cierre no amplía `TL-LANG-310` a los seis sustratos ni certifica F03; el avance maestro se
  conserva en 24/147.

## Progreso registrado

- **Hecho (Parser + Sema, longitud)**: el lowering emite `Length()` (llamada marcada
  `list-pattern-length`) y `Slice` (marcada `list-pattern-rest`); `analyzeCall` devuelve el valor
  del miembro `.Length` para built-ins/campos y deja caer a resolución normal de método para
  Span/ReadOnlySpan/StringView. Esto habilita Span/ReadOnlySpan/StringView y el protocolo usuario
  (no-rest y rest), sin romper `TL-LANG-308`/`TL-LANG-309`.
- **Parcial (síntesis de vistas)**: rest sobre array/inline ya solicita y enlaza tipos
  `Span<T>`/`ReadOnlySpan<T>` materializados antes de Sema. Sigue pendiente cerrar el lowering
  ejecutable de todos los sustratos, incluido string, mediante nuevos
  `MethodCoreOperationKind` (`SpanFromArray`/`ReadOnlySpanFromArray`/`StringViewFromString` y
  variantes de inline), reutilizando el esqueleto de `StringCharacterPointer`/`ArrayLength`/
  `RawElement` a través de Sema→HIR→MIR→LLVM.

## 1. SCOPE LOCK

- **Objetivo autorizado**: que el list pattern compile y ejecute sobre arrays nativos, inline
  arrays, slices, `Span<T>`/`ReadOnlySpan<T>`, strings UTF-8 y el protocolo de colección de usuario.
- **Naturaleza**: funcional (ampliar la superficie de list patterns). No es optimización ni refactor.
- **Representación aprobada por el usuario (opción A)**: protocolo canónico por métodos
  `Length()` + `Slice(int, int)` + indexador; para tipos sin `Slice` público, el compilador
  sintetiza una vista sin copia (array/almacenamiento mutable → `Span<T>`; fuente de solo lectura →
  `ReadOnlySpan<T>`; string → `StringView`).
- **Invariantes / comportamiento que debe permanecer idéntico**:
  - `TL-LANG-308` (formas base sobre array nativo) y `TL-LANG-309` (rest/slice prestado) siguen
    pasando sin modificar sus fixtures.
  - `TL-LANG-261..270` siguen pasando.
  - El protocolo de rango T01-F04 (`receiver[start..end]`) no cambia.
  - El protocolo `foreach` (array-mode / enumerator-mode) no cambia.
  - `Span<T>`, `ReadOnlySpan<T>`, `StringView`, `String` y las bibliotecas no se modifican.
- **Revisión base**: `development` @ `51ea26fbda286519f5f617bc54853d0f4d89dad5` (ancestro
  funcional `fe6b36eddd7d28d7d2ca049d27e085f556de6778` = T03-F02).
- **Archivos que pueden modificarse**:
  - `src/Parser/Parser.cpp` — lowering del caso `PatternKind::List` en `emitPatternBindings`.
  - `src/Frontend/ProjectMethodBodyModel.cpp` — resolución estructural de longitud/slice en el
    análisis de llamadas del list pattern.
  - Sólo si el diseño lo exige y sin ampliar el alcance: pares HIR/MIR del lowering del rest.
  - `tests/compiler-suite/host/modules/LanguageSurfaceModule.cpp` — registro de `TL-LANG-310`.
  - `tests/compiler-suite/manifest/cases.jsonl` y catálogos canónicos del caso nuevo.
  - `docs/3.0.4/items/NS304-T03-F03-list-pattern-collections.md` (este expediente) y el tracking
    maestro.
- **Archivos protegidos**: `lib/*.ns` (incluidos `Span.ns`, `StringView.ns`), CMake raíz y de suite,
  `nscbuilder3.py`, `nsc-builder/**`, manifiestos ajenos al ítem, `docs/DEVELOPMENT-COMMITS.md`,
  `docs/porting/**`, fixtures de `TL-LANG-308`/`TL-LANG-309`/`TL-LANG-261..270`.
- **Pruebas que deben pasar sin modificaciones**: `TL-LANG-308`, `TL-LANG-309`,
  `TL-LANG-261..270`.
- **Prueba nueva**: `TL-LANG-310` (integración; cubre las seis categorías y sus vistas).
- **Acciones prohibidas**: regenerar CMake; cambiar toolchain/LLVM; tocar `foreach`, rango T01-F04,
  o el sistema de tipos global; ejecutar batería completa; modificar guards cerrados; usar
  recursión; auto-invocar métodos con la grafía `.Length` fuera del list pattern.
- **Criterio de terminación**: `TL-LANG-310` PASS + guards `TL-LANG-308/309/261-270` PASS, sin
  batería completa; tracking avanza a 25/147 solo con esa evidencia; el commit queda listo para el
  usuario (compilación/commit de su autoría).

## 2. Auditoría — frontera real de Longitud / Indexación / Slice

El lowering actual (`src/Parser/Parser.cpp:11239-11283`) emite:

- longitud: `source.Length` (miembro) — `makeMemberExpression(source(), "Length")`;
- elemento: `source[index]` (indexador) — `makeIndexExpression`;
- rest: `source.Slice(prefix, source.Length - fijos)` (llamada a método).

La superficie es **heterogénea** y no intercambiable (`ProjectMethodBodyModel.cpp:13580-13622`
resuelve `Length` solo como miembro para built-ins; `Span`/`StringView` lo exponen como método):

| Categoría | `Length` | Indexación | `Slice` |
|---|---|---|---|
| array nativo `T[]` | `.Length` propiedad sintetizada (`ArrayLength`) | `[i]` core (`ArrayElement`) | ausente |
| inline array `T[n]` | `.Length` (`FixedArrayLength`, constante) | `[i]` (`RawElementAddress`) | ausente |
| string nativo | `.Length` (`StringLength`, bytes UTF-8) | `[i]` → `builtin:char` | ausente (`StringView.Slice`) |
| `Span<T>`/`ReadOnlySpan<T>` | **`Length()` método** | `this[i]` indexer | `Slice(int,int)` método |
| `StringView` | **`Length()` método** | `this[i]` indexer | `Slice(int,int)` método |
| struct usuario (F02) | `Length` **campo público** | `this[i]` indexer | `Slice(int,int)` método |

Consecuencia: hoy el lowering `source.Length` (miembro) resuelve para arrays/strings/inline y
structs con campo, pero **falla** para `Span`/`ReadOnlySpan`/`StringView` (método). Y `Slice` no
existe en arrays/strings/inline. Confirmada la advertencia del handover: propiedad `Length` y
método `Length()` **no** son intercambiables.

## 3. Reconciliación del guard F02 (decisión de diseño registrada)

F02 cerró con un struct usuario cuyo `Length` es **campo** (`LanguageSurfaceModule.cpp:1748-1768`).
El protocolo por métodos de la opción A canoniza `Length()`; un lowering que emita exclusivamente
`Length()` rompería el guard `TL-LANG-309` (inmutable).

Resolución: el lowering resuelve la longitud de forma **estructural** — primero como método
`Length()` (canónico para usuario y vistas), y, si no hay método, como miembro `.Length`
(campo/propiedad o core de array/string/inline). Es **un** algoritmo de resolución, no un
`switch` por tipo; conserva la opción A (método canónico) y el guard F02 intacto. Análogamente, el
rest resuelve `Slice(int,int)` como método o, si no existe, sintetiza la vista.

## 4. Diseño de implementación

### 4.1 Parser (`emitPatternBindings`, caso `List`)

1. Longitud: `source.Length` → `source.Length()` (`makeZeroArgCallExpression`), con
   `auxiliaryText = "list-pattern-length"`.
2. Rest: `source.Slice(start, length)` conserva la forma de llamada y marca
   `auxiliaryText = "list-pattern-rest"`.
3. Elementos (`source[index]`) sin cambios.

### 4.2 Sema (`analyzeCall`, tras los intrinsics de aserción)

Interceptar las dos llamadas marcadas antes de la resolución ordinaria:

- **`list-pattern-length`** (0 args, callee miembro `Length`):
  1. resolver `Length()` como método ordinario (cubre `Span`/`ReadOnlySpan`/`StringView`/usuario);
  2. si no resuelve, resolver `.Length` como miembro: array nativo (`ArrayLength`), string
     (`StringLength`), inline (`FixedArrayLength`) o campo/propiedad del struct (guard F02).
- **`list-pattern-rest`** (2 args `int`, callee miembro `Slice`):
  1. resolver `Slice(int,int)` como método ordinario (cubre vistas/usuario);
  2. si no resuelve, sintetizar la vista del origen:
     - array nativo o inline mutable → `Span<T>`;
     - array nativo o inline solo-lectura (`readonlyReceiver`) → `ReadOnlySpan<T>`;
     - string nativo → `StringView`.

La vista se construye sin copia reutilizando el mecanismo de reconocimiento de vistas ya existente
(`Span<T>`/`ReadOnlySpan<T>`/`StringView` como `borrowed struct`; fábricas `Spans.*` y
`StringView.Of`). Se conserva la evaluación única del valor gobernante (snapshot canónico de
`match`).

## 5. Plan de prueba

- `TL-LANG-310` (integración, `nsc_component_tests`): un fixture por categoría — array nativo,
  inline array, `Span<T>`, `ReadOnlySpan<T>`, `StringView`, string — con `[prefijo, .. var rest,
  sufijo]`; verifica longitud, elementos ordenados, `rest.Length()/Length`, y salida exacta.
  Incluye un negativo de escape desde `stackalloc` para el view prestado.
- Guards reejecutados: `TL-LANG-308`, `TL-LANG-309`, `TL-LANG-261..270`.
- Build focal con LLVM/Clang 23; sin regenerar CMake; sin batería completa.

## 6. Hueco documental

`A:\NATIVESHARP-V3.0.4-PLAN-DE-DESARROLLO.md` (fuente 2 del handover) no existe en disco. Por
instrucción del usuario se continúa con tracking maestro + handover como autoridad y el plan queda
marcado como hueco, sin inventar su contenido.
