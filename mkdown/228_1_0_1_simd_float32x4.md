# Entrega 228 — SIMD `Float32x4` / Delivery 228 — `Float32x4` SIMD

## Español

### Objetivo

Incorporar una primera familia SIMD pequeña, estable y útil para matemáticas, audio, color y
procesamiento de píxeles, sin convertir NativeSharp en un espejo de todos los intrinsics de cada
CPU.

### Archivos y funciones

- `lib/NativeSharp.Core/Float32x4.ns`: crea el valor público de 128 bits, su constructor y las
  operaciones `Add`, `Subtract` y `Multiply`.
- `ProjectMethodBodyModel`: reconoce únicamente la identidad
  `NativeSharp.Core.Numerics.Float32x4` y las tres firmas exactas; métodos homónimos no reciben
  semántica especial.
- `ProjectOopHirModel` y `ProjectOopMirModel`: conservan cada operación como un nodo explícito e
  inspeccionable, incluidos sus nombres en los volcados.
- `ProjectOopLlvmLowerer`: reinterpreta el carrier agregado de 16 bytes como `<4 x float>`, emite
  `fadd`, `fsub` o `fmul`, y devuelve otra vez el carrier público.
- `LlvmCApi.h`: añade exclusivamente `LLVMVectorType`, la única ampliación necesaria de la
  frontera C de LLVM.
- `Stage68SimdTests.cpp`: certifica a O0 que el módulo verifica y contiene las tres operaciones
  vectoriales.
- Especificación y mapa de conformidad 1.0.1: añaden `NS-SIMD-001`.

### Decisiones de diseño

El tipo público sigue siendo un struct ordinario con campos `X`, `Y`, `Z` y `W`. Esto hace que un
depurador, NSMETA y una frontera nativa vean el mismo layout independientemente de SSE, AVX o de la
legalización elegida por LLVM. La vectorización ocurre sólo dentro de la operación. No se promete
una instrucción concreta: el contrato es la operación por cuatro carriles con semántica IEEE.

No se añadieron cargas o almacenes basados en punteros: el lenguaje ya puede leer y escribir el
struct por su ABI normal, y abrir una segunda API de memoria habría duplicado reglas de alineación.

### Validación prevista

El test de entrega inspecciona IR O0 para `<4 x float>` y `fadd/fsub/fmul`. Conforme a la secuencia
de trabajo acordada, se integra ahora pero sólo se ejecutará dentro del baseline posterior a la
entrega 234. Entre entregas se reconstruye únicamente el compilador desde cero.

## English

### Goal

Introduce a small, stable first SIMD family useful for math, audio, color, and pixel processing
without turning NativeSharp into a mirror of every CPU intrinsic catalog.

### Files and functions

- `lib/NativeSharp.Core/Float32x4.ns` defines the public 128-bit value, constructor, and `Add`,
  `Subtract`, and `Multiply` operations.
- `ProjectMethodBodyModel` recognizes only the exact public type identity and signatures.
- The HIR and MIR retain explicit, dumpable operation nodes.
- `ProjectOopLlvmLowerer` bridges the 16-byte aggregate carrier to `<4 x float>`, emits vector
  arithmetic, and bridges the result back without changing the public ABI.
- `LlvmCApi.h` adds only `LLVMVectorType`.
- `Stage68SimdTests.cpp` certifies the O0 LLVM shape.
- The 1.0.1 specification and conformance map define `NS-SIMD-001`.

### Design decisions

The public representation remains a regular `{ X, Y, Z, W }` struct, keeping layout, NSMETA, PDB,
and native ABI target-independent. LLVM may select native instructions or legalize the vector, but
must preserve four-lane strict IEEE behavior. Pointer load/store intrinsics were intentionally not
duplicated because the existing aggregate ABI already provides the memory contract.

### Planned validation

The delivery test inspects O0 IR for `<4 x float>` and `fadd/fsub/fmul`. It is intentionally queued
for the single baseline after delivery 234; only a clean compiler rebuild follows this commit.
