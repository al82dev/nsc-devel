# Item 303 — Órdenes de memoria públicas / Public memory orders

## Español

### Objetivo

Publicar un vocabulario único y ABI-estable para que las siguientes entregas puedan expresar el
orden de cargas, stores, operaciones RMW, compare-exchange y fences sin inventar nombres o valores
distintos en cada API.

### Archivos y funciones

- `lib/NativeSharp.Core.Threading/Atomics.ns`
  - Agrega el enum público `MemoryOrder` con `Relaxed`, `Acquire`, `Release`, `AcquireRelease` y
    `SequentiallyConsistent`.
  - Conserva sin cambios la semántica secuencialmente consistente de los métodos sin parámetro.
- `examples/project_stage54_2/Main.ns`
  - Agrega cinco comprobaciones ejecutables que congelan los valores públicos 0–4.
- `tests/unit/Stage54_2AtomicsTests.cpp`
  - Actualiza la evidencia del ejemplo de 27 a 32 comprobaciones.
- `docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md`
  - Define `NS-MEM-004`, la identidad pública y la obligación de rechazar órdenes ilegales.
- `docs/1.0.1/language/NativeSharp_1_0_1_conformance.map`
  - Conecta el contrato con la biblioteca y la evidencia ejecutable.
- `docs/1.0.1/CURRENT_STATE.md`, `docs/1.0.1/CHANGELOG.md` y
  `docs/1.0.1/NATIVESHARP_2_0_ITEMS_FINAL.md`
  - Actualizan estado, historial y seguimiento del roadmap.

### Decisiones de diseño

- Los valores 0–4 son propios de NativeSharp y no copian los números internos de LLVM. Esto evita
  que la API pública dependa accidentalmente de una enumeración de backend.
- Se usa `AcquireRelease` en vez de abreviaturas como `AcqRel` para que la superficie sea explícita.
- No se agrega `Consume`: el backend y el contrato no ofrecen una semántica distinta y portable que
  justifique publicarlo.
- Este commit no agrega overloads. Separar el vocabulario de su uso permite certificar después las
  reglas válidas de load/store/RMW/compare-exchange de manera independiente.

### Evidencia enfocada

El test `nsc.stage54-2.atomics` compila el proyecto real de threading en O0 y O2 y ejecuta el ejemplo;
los cinco valores del enum forman parte de sus 32 condiciones discriminantes.

## English

### Goal

Publish one ABI-stable vocabulary that subsequent deliveries can use for ordered loads, stores,
read-modify-write operations, compare-exchange, and fences without defining different names or
values in each API.

### Files and implementation

- `lib/NativeSharp.Core.Threading/Atomics.ns` adds the public `MemoryOrder` enum while preserving
  the sequentially consistent behavior of existing parameterless methods.
- `examples/project_stage54_2/Main.ns` executes five checks that freeze public values 0 through 4.
- `tests/unit/Stage54_2AtomicsTests.cpp` updates the executable evidence count.
- The language specification, conformance map, current-state document, changelog, and roadmap record
  the new contract and its implementation status.

### Design decisions

- Values 0–4 belong to NativeSharp and deliberately do not mirror LLVM's internal enum ordinals.
- `AcquireRelease` is explicit and avoids an abbreviated public API.
- `Consume` is omitted because the backend and language contract do not provide a distinct portable
  semantic for it.
- Ordered overloads remain separate items so load, store, RMW, and compare-exchange validity rules
  can each be implemented and certified independently.

### Focused evidence

`nsc.stage54-2.atomics` builds the real threading project in O0 and O2 and executes all 32 checks,
including the five stable enum identities.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Publica órdenes de memoria estables / Publish stable memory orders`

**Descripción / Description**

`Agrega MemoryOrder como vocabulario ABI propio de NativeSharp y certifica sus cinco identidades sin cambiar aún la semántica seq_cst de las operaciones existentes. / Add MemoryOrder as NativeSharp's own ABI vocabulary and certify its five identities without changing the seq_cst semantics of existing operations yet.`
