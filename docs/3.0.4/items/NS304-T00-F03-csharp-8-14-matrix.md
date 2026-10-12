# NS304-T00-F03 — Matriz C# 8.0–14

## Estado

`CERRADO` el 2026-10-12 como matriz contractual. No declara paridad
funcional: todo `GAP_OBLIGATORIO` sigue pendiente hasta su ítem NS304 y todo
equivalente/alternativa requiere fixture terminal en T17.

## SCOPE LOCK

- Objetivo: clasificar todas las capacidades estables enumeradas para C# 8.0,
  9, 10, 11, 12, 13 y 14 como equivalente, alternativa NativeSharp,
  divergencia deliberada o gap obligatorio.
- Naturaleza: diseño/auditoría documental; sin cambio de producto.
- Revisión base: `5398f0944` en `development`.
- Archivos modificables: este expediente y el tracking maestro.
- Protegido: producto, runtime, CMake, toolchain, Builder, manifests y pruebas.
- Terminación: cobertura completa, una disposición por capacidad, vínculo a
  work item cuando exista gap y previews separados de la matriz estable.

## Fuentes y frontera

- Fuente oficial de enumeración: [The history of C#](https://learn.microsoft.com/dotnet/csharp/whats-new/csharp-version-history).
- Detalle C# 14: [What's new in C# 14](https://learn.microsoft.com/en-us/dotnet/csharp/whats-new/csharp-14/).
- Autoridad NativeSharp: `A:\NATIVESHARP-V3.0.4-PLAN-DE-DESARROLLO.md`,
  secciones 27–29, más F01/F02 de esta tanda.
- Se excluyen de la matriz estable `interceptors` (preview en C# 12) y
  `field` preview de C# 13; `field` sí aparece en C# 14 estable.
- Directivas de aplicaciones de archivo se clasifican como hosting/tooling y no
  como feature del núcleo lingüístico de esta matriz.

## Leyenda contractual

- `EQUIVALENTE`: existe superficie NativeSharp directa con el mismo propósito.
- `ALTERNATIVA`: NativeSharp cubre el propósito mediante un contrato nativo
  distinto; no promete sintaxis ni semántica CLR idénticas.
- `DIVERGENCIA`: se conserva deliberadamente una frontera distinta o la
  capacidad depende de IL/CLR y no forma parte del compromiso 3.0.4.
- `GAP_OBLIGATORIO`: el plan 3.0.4 lo compromete y la matriz enlaza el ítem
  responsable; no se considera implementado.

## Matriz exhaustiva

| C# | Capacidad estable | Disposición NativeSharp | Evidencia / dueño 3.0.4 |
|---:|---|---|---|
| 8.0 | Readonly members | `EQUIVALENTE` | readonly/const y verificación de mutación en miembros existentes |
| 8.0 | Default interface members | `GAP_OBLIGATORIO` | NS304-T02-F01..F06 |
| 8.0 | Switch expressions | `ALTERNATIVA` | `match` exhaustivo nativo (`NS3-T03-F03`) |
| 8.0 | Property patterns | `GAP_OBLIGATORIO` | NS304-T03-F04 |
| 8.0 | Tuple patterns | `EQUIVALENTE` | patterns fundamentales y tuplas nativas (`NS3-T03-F02`) |
| 8.0 | Positional patterns | `ALTERNATIVA` | deconstrucción/protocolo `Deconstruct` (`NS3-T02-F03`) |
| 8.0 | Using declarations | `ALTERNATIVA` | RAII, destructores y `defer` (`NS3-T06-F05`) |
| 8.0 | Static local functions | `GAP_OBLIGATORIO` | NS304-T04-F05/F06 |
| 8.0 | Shadowing relajado en lambdas/local functions | `DIVERGENCIA` | no comprometido; se conserva el scope NativeSharp hasta decisión explícita |
| 8.0 | Disposable ref structs | `ALTERNATIVA` | tipos stack-only, ownership y destrucción determinista |
| 8.0 | Nullable reference types | `ALTERNATIVA` | nullability nativa y operadores null-aware, sin modelo CLR |
| 8.0 | Async streams | `GAP_OBLIGATORIO` | NS304-T07-F01..F08 |
| 8.0 | Indices and ranges | `EQUIVALENTE` | `^`/`..` y slicing (`NS3-T01-F04`) |
| 8.0 | Null-coalescing assignment (`??=`) | `EQUIVALENTE` | operadores null-aware (`NS3-T01-F03`) |
| 8.0 | Unmanaged constructed types | `ALTERNATIVA` | agregados nativos por defecto, sin constraint CLR `unmanaged` |
| 8.0 | Stackalloc in nested expressions | `GAP_OBLIGATORIO` | NS304-T13-F03/F05 |
| 9 | Records | `ALTERNATIVA` | `record struct` nativo sin identidad administrada (`NS3-T02-F04`) |
| 9 | Init-only setters | `GAP_OBLIGATORIO` | NS304-T09-F02 |
| 9 | Top-level statements | `ALTERNATIVA` | entrypoint/procedimientos de namespace, sin modelo de script CLR |
| 9 | Relational and logical patterns | `EQUIVALENTE` | patterns relacionales/lógicos actuales |
| 9 | Native-sized integers (`nint`/`nuint`) | `EQUIVALENTE` | `builtin:nint`/`builtin:nuint` Win64 |
| 9 | Function pointers | `EQUIVALENTE` | `fn*`/calling conventions (`NS3-T08-F01`) |
| 9 | Suppress emitting `localsinit` | `DIVERGENCIA` | detalle IL/CLR; NativeSharp usa inicialización/unsafe nativos |
| 9 | Module initializers | `GAP_OBLIGATORIO` | NS304-T01-F03/F04/F05 |
| 9 | Expanded partial methods | `GAP_OBLIGATORIO` | NS304-T12-F02 |
| 9 | Target-typed `new` | `GAP_OBLIGATORIO` | NS304-T09-F07 |
| 9 | Static anonymous functions | `GAP_OBLIGATORIO` | NS304-T04-F05 |
| 9 | Target-typed conditional expressions | `GAP_OBLIGATORIO` | NS304-T04-F01/F07 |
| 9 | Covariant return types | `ALTERNATIVA` | override/ABI nativos; requiere fixture T17, no semántica CLR |
| 9 | Extension `GetEnumerator` for `foreach` | `ALTERNATIVA` | protocolo estático de iteración/traits |
| 9 | Lambda discard parameters | `EQUIVALENTE` | parámetros descartados de callable sin semántica runtime |
| 9 | Attributes on local functions | `GAP_OBLIGATORIO` | NS304-T04-F02/F06 |
| 10 | Record structs | `EQUIVALENTE` | `record struct` nativo (`NS3-T02-F04`) |
| 10 | Struct type improvements | `ALTERNATIVA` | agregados nativos, definite initialization y ownership |
| 10 | Interpolated string handlers | `GAP_OBLIGATORIO` | NS304-T10-F03/F04 |
| 10 | Global using directives | `ALTERNATIVA` | imports/módulos explícitos; sin `global using` textual |
| 10 | File-scoped namespaces | `ALTERNATIVA` | sintaxis de módulo/namespace existente |
| 10 | Extended property patterns | `GAP_OBLIGATORIO` | NS304-T03-F04/F05 |
| 10 | Natural type for lambdas/method groups | `GAP_OBLIGATORIO` | NS304-T04-F01/F07 |
| 10 | Explicit lambda return type | `GAP_OBLIGATORIO` | NS304-T04-F02 |
| 10 | Attributes on lambdas | `GAP_OBLIGATORIO` | NS304-T04-F02 |
| 10 | Constant interpolated strings | `GAP_OBLIGATORIO` | NS304-T10-F04 |
| 10 | Sealed `ToString` in records | `DIVERGENCIA` | síntesis administrada de records no adoptada |
| 10 | Improved definite assignment/null-state analysis | `ALTERNATIVA` | análisis NativeSharp de inicialización/nullability/ownership |
| 10 | Mixed declaration and assignment deconstruction | `EQUIVALENTE` | deconstrucción nativa (`NS3-T02-F03`) |
| 10 | `AsyncMethodBuilder` on methods | `DIVERGENCIA` | executor/Future explícitos; no builder CLR por atributo |
| 10 | `CallerArgumentExpression` | `ALTERNATIVA` | spans/source mapping y comptime tipado, sin atributo runtime |
| 10 | Enhanced `#line` pragma | `ALTERNATIVA` | source mapping/PDB e higiene de generación (NS304-T14-F04) |
| 11 | Raw string literals | `EQUIVALENTE` | `RawStringLiterals` implementado |
| 11 | Generic math | `ALTERNATIVA` | traits/associated items/operator traits nativos |
| 11 | Generic attributes | `GAP_OBLIGATORIO` | NS304-T11-F08 |
| 11 | UTF-8 string literals | `GAP_OBLIGATORIO` | NS304-T10-F01 |
| 11 | Newlines in interpolation expressions | `EQUIVALENTE` | parser/interpolación estática existente |
| 11 | List patterns | `GAP_OBLIGATORIO` | NS304-T03-F01..F08 |
| 11 | File-local types | `ALTERNATIVA` | visibilidad privada de módulo/archivo según módulo NativeSharp |
| 11 | Required members | `GAP_OBLIGATORIO` | NS304-T09-F01 |
| 11 | Auto-default structs | `ALTERNATIVA` | definite initialization explícita; no zeroing CLR implícito |
| 11 | Pattern match `Span<char>` on constant string | `GAP_OBLIGATORIO` | NS304-T03-F03/F05 + T11-F01/F02 |
| 11 | Extended `nameof` scope | `GAP_OBLIGATORIO` | NS304-T10-F07 |
| 11 | Native integer keyword aliases | `EQUIVALENTE` | `nint`/`nuint` son tipos builtin actuales |
| 11 | Ref fields and `scoped ref` | `GAP_OBLIGATORIO` | NS304-T11-F07 |
| 11 | Improved method group conversion | `GAP_OBLIGATORIO` | NS304-T04-F01/F07 |
| 11 | Warning wave 7 | `ALTERNATIVA` | catálogo diagnóstico/lints estable, sin versionado Roslyn |
| 12 | Primary constructors | `EQUIVALENTE` | `PrimaryConstructors` implementado / promoted fields |
| 12 | Collection expressions and spread | `GAP_OBLIGATORIO` | NS304-T09-F05 |
| 12 | Inline arrays | `GAP_OBLIGATORIO` | NS304-T09-F06 |
| 12 | Optional lambda parameters | `GAP_OBLIGATORIO` | NS304-T04-F04 |
| 12 | `ref readonly` parameters | `GAP_OBLIGATORIO` | NS304-T04-F03 + T11-F07 |
| 12 | Alias any type | `EQUIVALENTE` | aliases transparentes/opacos (`NS3-T04-F05`) |
| 12 | Experimental attribute | `ALTERNATIVA` | capabilities/availability y diagnósticos, sin atributo CLR |
| 13 | `params` collections | `GAP_OBLIGATORIO` | NS304-T11-F04 |
| 13 | New `lock` type and semantics | `ALTERNATIVA` | sin Monitor/CLR; sincronización/runtime nativos explícitos |
| 13 | Escape sequence `\e` | `GAP_OBLIGATORIO` | NS304-T10-F08 |
| 13 | Method group natural type improvements | `GAP_OBLIGATORIO` | NS304-T04-F01/F07 |
| 13 | Implicit index access in object initializers | `GAP_OBLIGATORIO` | NS304-T09-F05/F07 |
| 13 | `ref`/`unsafe` in iterators and async methods | `GAP_OBLIGATORIO` | NS304-T05-F05 + T06-F04/F05 |
| 13 | Ref structs implement interfaces | `GAP_OBLIGATORIO` | NS304-T11-F06 |
| 13 | Ref structs as generic arguments | `GAP_OBLIGATORIO` | NS304-T11-F05 |
| 13 | Partial properties and indexers | `GAP_OBLIGATORIO` | NS304-T12-F03 |
| 13 | Overload resolution priority | `GAP_OBLIGATORIO` | NS304-T04-F07 |
| 14 | Extension members | `GAP_OBLIGATORIO` | NS304-T08-F01..F07 |
| 14 | Null-conditional assignment | `GAP_OBLIGATORIO` | NS304-T10-F05/F06 |
| 14 | `nameof` on unbound generic types | `GAP_OBLIGATORIO` | NS304-T10-F07 |
| 14 | First-class span conversions | `GAP_OBLIGATORIO` | NS304-T11-F01..F03 |
| 14 | Modifiers on simple lambda parameters | `GAP_OBLIGATORIO` | NS304-T04-F03 |
| 14 | `field` backed properties | `GAP_OBLIGATORIO` | NS304-T09-F03 |
| 14 | Partial events and constructors | `GAP_OBLIGATORIO` | NS304-T12-F04/F05 |
| 14 | User-defined compound assignment operators | `GAP_OBLIGATORIO` | NS304-T08-F04 |

## Conteos

- Versiones: C# 9=16, C# 10=16, C# 11=15, C# 12=7, C# 13=10, C# 14=8, C# 8.0=16.
- Disposiciones: `ALTERNATIVA`=22, `DIVERGENCIA`=4, `EQUIVALENTE`=15, `GAP_OBLIGATORIO`=47.
- Total estable clasificado: **88/88**; omitidos=0; duplicados=0;
  disposiciones múltiples=0.

## Criterio de uso

Esta matriz congela el vocabulario comparativo, no el estado de implementación.
T17-F01 deberá sustituir cada fila relevante por una fixture NativeSharp o por
la evidencia de divergencia deliberada. Una fila `GAP_OBLIGATORIO` sólo cambia
de estado mediante su ítem NS304; una alternativa no puede describirse como
compatibilidad sintáctica con C#.

## Gate F03

```text
capacidades_oficiales_estables = 88
capacidades_clasificadas = 88
omitidas = 0
duplicadas = 0
previews_mezcladas = 0
gaps_sin_dueño = 0
```

Siguiente acción determinista: `NS304-T00-F04`, matriz Beef de memoria,
extensions, generics, comptime, interop y patterns.
