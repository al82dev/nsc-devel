# NS304-T00-F02 — Matriz de evidencia por capas

## Estado

`CERRADO` el 2026-10-12 como clasificación de evidencia. La matriz no
recertifica funcionalidad: distingue deliberadamente una ruta actual de una
afirmación o ejecución histórica.

## SCOPE LOCK

- Objetivo: clasificar los 55 elementos de F01 por parser/AST, semántica,
  MIR, LLVM, NSMETA, runtime y ejecución.
- Naturaleza: auditoría documental y estática; sin cambio funcional.
- Revisión base: `69f2d2b3e` en `development`.
- Archivos modificables: este expediente y el tracking maestro.
- Protegido: producto, CMake, toolchain, Builder, manifests y pruebas.
- Terminación: 55/55 filas clasificadas, leyenda inequívoca, conteos
  reproducibles, retiradas visibles y ningún hueco convertido en éxito.

## Leyenda

- `A`: existe al menos un archivo actual de esa capa relacionado por el
  expediente, la fila F01 o el commit histórico. Es ancla, no recertificación.
- `D`: el expediente histórico afirma la capa, pero no localiza una ruta actual
  específica para esa capa.
- `T/H`: existe artefacto de prueba actual y el expediente conserva resultado
  histórico; la prueba no se volvió a ejecutar en F02.
- `H`: sólo se conserva resultado histórico, sin archivo de prueba separado.
- `R`: la vertical fue retirada después de su cierre histórico.
- `—`: F02 no encontró evidencia atribuible. No significa automáticamente
  defecto ni `N/A`; esa decisión pertenece al contrato de cada feature.

Capas: `P` parser/lexer/AST; `S` semántica/frontend; `M` MIR; `L` LLVM;
`N` NSMETA/metadata; `Rtm` runtime; `E` ejecución.

## Matriz completa

| ID histórico | Feature | P | S | M | L | N | Rtm | E |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| `NS3-T00-F01` | Ediciones de lenguaje / Language editions | R | R | R | R | R | R | R |
| `NS3-T00-F02` | Modelo de etapas / Compilation-stage model | — | A | D | — | — | — | T/H |
| `NS3-T00-F03` | Categorías de valores / Value categories | — | A | — | D | — | — | T/H |
| `NS3-T00-F04` | Contextos de efectos / Effect contexts | — | A | A | A | D | — | T/H |
| `NS3-T00-F05` | Catálogo estable de diagnósticos / Stable diagnostic catalog | — | — | — | — | — | — | T/H |
| `NS3-T00-F06` | Harness de conformidad / Conformance harness | — | — | D | — | — | — | H |
| `NS3-T01-F01` | Argumentos nombrados / Named arguments | D | A | D | D | D | — | T/H |
| `NS3-T01-F02` | Parámetros variádicos tipados / Typed parameter packs | D | A | D | D | D | — | T/H |
| `NS3-T01-F03` | Operadores nulos / Null-aware operators | — | A | A | A | — | D | T/H |
| `NS3-T01-F04` | Operadores de rango e índice / Range and index operators | D | A | A | A | — | D | T/H |
| `NS3-T01-F05` | Inicializadores generalizados / Generalized initializers | D | A | D | D | — | — | T/H |
| `NS3-T01-F06` | Interpolación estática / Static string interpolation | A | A | — | D | — | D | T/H |
| `NS3-T01-F07` | Expresiones with y actualización no destructiva / Non-destructive mutation | D | A | D | D | — | D | T/H |
| `NS3-T02-F01` | Tipos tupla nativos / Native tuple types | A | A | — | — | — | D | T/H |
| `NS3-T02-F02` | Literales y conversión de tuplas / Tuple literals and conversions | — | A | — | D | — | D | T/H |
| `NS3-T02-F03` | Deconstrucción / Deconstruction | A | A | — | D | — | — | T/H |
| `NS3-T02-F04` | Registros nativos / Native records | D | A | — | A | A | D | T/H |
| `NS3-T02-F05` | Campos y parámetros promovidos / Promoted fields and parameters | A | A | D | D | — | — | T/H |
| `NS3-T02-F06` | Spreads de agregados / Aggregate spreads | D | A | A | A | — | — | T/H |
| `NS3-T03-F01` | Variants etiquetados / Tagged variants | A | A | — | A | A | — | T/H |
| `NS3-T03-F02` | Patterns fundamentales / Fundamental patterns | A | A | — | — | — | D | T/H |
| `NS3-T03-F03` | Expresión match exhaustiva / Exhaustive match expression | A | A | A | A | D | D | T/H |
| `NS3-T03-F04` | if let, while let y let else / Conditional patterns | A | A | D | — | — | D | T/H |
| `NS3-T03-F05` | Guards y refinamiento / Guards and narrowing | A | A | A | A | — | — | T/H |
| `NS3-T03-F06` | Variants compatibles con C / C-compatible unions | A | A | — | — | D | — | T/H |
| `NS3-T04-F01` | Constraints declarativos / Declarative constraints | A | A | — | — | D | — | T/H |
| `NS3-T04-F02` | Parámetros const genéricos / Const generics | A | A | — | — | — | D | T/H |
| `NS3-T04-F03` | Packs de tipos y valores / Type and value packs | A | A | — | — | D | D | T/H |
| `NS3-T04-F04` | Inferencia bidireccional / Bidirectional inference | A | A | — | — | — | — | T/H |
| `NS3-T04-F05` | Alias de tipo y alias opacos / Type aliases and opaque aliases | A | A | D | D | D | D | T/H |
| `NS3-T04-F06` | Especialización controlada / Controlled specialization | A | A | — | — | A | D | T/H |
| `NS3-T04-F07` | Límites de monomorfización / Monomorphization limits | D | A | — | — | — | D | T/H |
| `NS3-T05-F01` | Declaraciones trait / Trait declarations | A | A | — | — | A | D | T/H |
| `NS3-T05-F02` | Implementaciones y coherencia / Implementations and coherence | A | A | — | — | A | D | T/H |
| `NS3-T05-F03` | Tipos y constantes asociados / Associated types and constants | A | A | — | — | A | D | T/H |
| `NS3-T05-F04` | Existenciales opacos / Opaque result types | A | A | — | — | A | D | T/H |
| `NS3-T05-F05` | Objetos trait explícitos / Explicit trait objects | — | A | D | — | A | D | T/H |
| `NS3-T05-F06` | Extensiones estáticas / Extension methods | A | A | D | A | A | D | T/H |
| `NS3-T05-F07` | Operadores mediante traits / Trait-based operators | A | A | — | D | D | D | T/H |
| `NS3-T06-F01` | Tipos move-only y Copy explícito / Move-only types and explicit Copy | A | A | — | — | A | — | T/H |
| `NS3-T06-F03` | Reborrow y préstamos divididos / Reborrowing and split borrows | A | A | — | — | — | — | T/H |
| `NS3-T06-F04` | Lifetimes nombrados e inferidos / Named and inferred lifetimes | A | A | — | — | A | D | T/H |
| `NS3-T06-F05` | Destructores, defer y orden de salida / Destructors, defer, and exit order | A | A | — | — | — | D | T/H |
| `NS3-T06-F06` | Pinning y direcciones estables / Pinning and stable addresses | A | A | A | A | — | D | T/H |
| `NS3-T06-F07` | Análisis de escape interprocedural / Interprocedural escape analysis | — | A | — | — | A | — | T/H |
| `NS3-T06-F08` | Drop glue genérico e indirecto / Generic and indirect drop glue | A | A | A | A | A | — | T/H |
| `NS3-T07-F01` | Protocolo Result y operador ? / Result protocol and propagation | A | A | A | A | — | D | T/H |
| `NS3-T07-F02` | Funciones throws nativas / Native throwing functions | A | A | A | A | A | D | T/H |
| `NS3-T07-F03` | try, catch tipado y finally / Typed try-catch-finally | A | A | A | A | — | D | T/H |
| `NS3-T07-F04` | Expresiones divergentes y tipo never / Diverging expressions and never type | — | A | D | A | — | D | T/H |
| `NS3-T07-F05` | Loop expressions y break con valor / Loop expressions | A | A | A | A | — | D | T/H |
| `NS3-T07-F06` | Assertions y panic profiles / Assertions and panic profiles | A | A | A | A | — | A | T/H |
| `NS3-T08-F01` | Tipos función completos / Complete function types | A | A | — | A | A | D | T/H |
| `NS3-T08-F02` | Lambdas sin captura / Non-capturing lambdas | — | A | A | A | — | D | T/H |
| `NS3-T08-F03` | Closures con captura explícita / Explicit-capture closures | A | A | A | A | — | — | T/H |

## Conteos reproducibles

- Parser/AST: `—`=11, `A`=35, `D`=8, `R`=1.
- Semántica: `—`=2, `A`=52, `R`=1.
- MIR: `—`=27, `A`=15, `D`=12, `R`=1.
- LLVM: `—`=23, `A`=20, `D`=11, `R`=1.
- NSMETA: `—`=30, `A`=15, `D`=9, `R`=1.
- Runtime: `—`=21, `A`=1, `D`=32, `R`=1.
- Ejecución: `H`=1, `R`=1, `T/H`=53.

## Lectura causal

1. `NS3-T00-F01` aparece `R` en todas las capas porque
   `0bea623fbf108aa1336f89586d0bf347287812e9` retiró la selección por edición,
   sus diagnósticos y su transporte semántico. No se cuenta como vertical
   productiva.
2. `A` sólo prueba localización en el árbol actual. F02 no ejecutó esas rutas y
   no hereda los resultados históricos como certificación de 3.0.4.
3. `D` identifica exactamente la deuda de localización que debe resolverse al
   trabajar el contrato afectado; no se completa por inferencia.
4. `—` mantiene visible la falta de evidencia por capa. T00-F03–F08 posteriores
   decidirán aplicabilidad y contrato sin reescribir el pasado.

## Gate F02

```text
inventariados = 55
clasificados = 55
omitidos = 0
desconocidos = 0
retirados_visibles = 1
resultados_actuales_inferidos_de_historia = 0
```

Siguiente acción determinista: `NS304-T00-F03`, matriz C# 8–14 con
equivalencia, alternativa, divergencia o gap obligatorio.
