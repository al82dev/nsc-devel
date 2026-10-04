# NS3-T14-F04 — Semántica y modos de punto flotante / Floating-point semantics and modes

## Español

NativeSharp 3.0 consolida tres contratos de fuente que pueden fijarse en una función o en un
bloque léxico:

```nativesharp
double Dot(double a, double b) fp(strict) { return a * b; }

fp(fast) { acumulado = acumulado + x * y; }
```

Un bloque interno reemplaza el modo de su función solamente dentro del bloque. Fuera de él se
restaura el contrato exterior. Sin cláusula, la política del proyecto o de la línea de comandos
sigue siendo la autoridad. `--fp-mode=strict` es ahora una opción de primera clase;
`deterministic` continúa aceptándose por compatibilidad.

| Modo | NaN y payload | `-0` | FMA | Reasociación | Denormales | Consteval/runtime |
|---|---|---|---|---|---|---|
| `strict` | preservados | preservado | prohibido | prohibida | preservados | mismo resultado IEEE |
| `precise` | preservados | preservado | permitido | prohibida | preservados | puede diferir por una contracción |
| `fast` | no prometidos | no prometido | permitido | permitida | preservados | puede diferir dentro de las libertades fast |

`strict` baja las operaciones a intrínsecos LLVM constrained con redondeo nearest-even y sin
contracción. `precise` no agrega banderas que relajen NaN o signed zero, pero marca operaciones
contraíbles. `fast` usa las banderas fast-math completas. El modo efectivo alcanza operaciones
escalares, comparaciones, negación y operaciones SIMD reconocidas por el compilador.

El payload de NaN es una garantía del modo `strict`/`precise` en el target congelado Windows x64,
no una promesa portable a una futura plataforma. Ningún modo habilita FTZ/DAZ por sintaxis; los
modos de fuente preservan denormales. Las opciones explícitas de toolchain para denormales siguen
siendo un contrato de construcción separado.

Los dumps de AST, miembros y MIR publican `fp=strict|precise|fast`. Así, cachés, auditorías y el
backend no tienen que inferir el modo a partir del texto. La prueba dirigida realiza diez
comprobaciones: contratos, opción strict, ejecución O0 con NSLink, NaN, signed zero,
consteval/runtime, constrained FP, contracción precise, fast scalar/SIMD, MIR y diagnóstico de
modo inválido.

## English

NativeSharp 3.0 consolidates three source contracts selectable per function or lexical block:

```nativesharp
double Dot(double a, double b) fp(strict) { return a * b; }

fp(fast) { accumulated = accumulated + x * y; }
```

An inner block overrides its function mode only for that block, after which the outer contract is
restored. Without a clause, the project or command-line policy remains authoritative.
`--fp-mode=strict` is now first class; `deterministic` remains accepted for compatibility.

| Mode | NaN and payload | `-0` | FMA | Reassociation | Denormals | Consteval/runtime |
|---|---|---|---|---|---|---|
| `strict` | preserved | preserved | forbidden | forbidden | preserved | same IEEE result |
| `precise` | preserved | preserved | allowed | forbidden | preserved | may differ by one contraction |
| `fast` | unspecified | unspecified | allowed | allowed | preserved | may differ within fast freedoms |

`strict` lowers operations through LLVM constrained intrinsics with nearest-even rounding and no
contraction. `precise` adds no NaN or signed-zero relaxations, but marks contractable operations.
`fast` applies the complete fast-math flag set. The effective mode reaches scalar operations,
comparisons, negation, and compiler-recognized SIMD operations.

NaN payload preservation in `strict`/`precise` is a guarantee for the frozen Windows x64 target,
not a portability promise for a future platform. No source mode enables FTZ/DAZ; source modes
preserve denormals. Explicit denormal toolchain options remain a separate build contract.

AST, member, and MIR dumps publish `fp=strict|precise|fast`, so caches, audits, and lowering do not
infer policy from source text. The directed test performs ten checks covering contracts, the
strict option, O0/NSLink execution, NaN, signed zero, consteval/runtime behavior, constrained FP,
precise contraction, scalar/SIMD fast math, MIR visibility, and invalid-mode diagnostics.
