# NS3-T10-F07 — Memoria atómica y `volatile` consolidados

## Español

### Resultado

NativeSharp 3.0 tiene una sola autoridad productiva para su modelo de memoria. El análisis
semántico y el backend LLVM consultan `MemoryModel` en vez de mantener matrices independientes de
órdenes. Los valores públicos de `MemoryOrder` permanecen separados de los ordinales LLVM.

El contrato fija:

- una carrera sobre memoria ordinaria es comportamiento indefinido;
- `Load` admite `Relaxed`, `Acquire` y `SequentiallyConsistent`;
- `Store` admite `Relaxed`, `Release` y `SequentiallyConsistent`;
- las operaciones read-modify-write admiten los cinco órdenes;
- un fence rechaza `Relaxed`;
- el orden de fallo de compare-exchange es de carga y no puede ser más fuerte que el de éxito;
- `volatile` conserva accesos observables, pero nunca publica datos ni sincroniza threads;
- un acceso `volatile` desalineado o mayor de 64 bits puede desgarrarse en Windows x64.

### Integración

`ProjectMethodBodyModel` usa la matriz central al validar llamadas atómicas y fences.
`ProjectOopLlvmLowerer` vuelve a validar la metadata antes de convertirla a órdenes LLVM. Así, una
metadata corrupta no puede saltarse el contrato del frontend.

### Prueba dirigida

`nsc_ns3_t10_memory_model_tests` compila un programa edición 3 en O0, inspecciona LLVM para cargas,
stores, RMW, fence y accesos `volatile`, lo enlaza con NSLink y ejecuta el resultado. También prueba
diagnósticos de órdenes inválidos. Resultado local: **12/12**.

No se ejecutó baseline, minibaseline ni suite general.

## English

### Result

NativeSharp 3.0 now has one production authority for its memory model. Semantic analysis and the
LLVM backend query `MemoryModel` rather than maintaining independent ordering matrices. Public
`MemoryOrder` values remain explicitly decoupled from LLVM ordinals.

The contract establishes:

- an ordinary-memory data race is undefined behavior;
- `Load` accepts Relaxed, Acquire, and SequentiallyConsistent;
- `Store` accepts Relaxed, Release, and SequentiallyConsistent;
- read-modify-write operations accept all five orders;
- a fence rejects Relaxed;
- compare-exchange failure order is load-like and cannot exceed success order;
- `volatile` preserves observable accesses but never publishes data or synchronizes threads;
- a misaligned or wider-than-64-bit volatile access may tear on Windows x64.

### Integration

`ProjectMethodBodyModel` uses the central matrix for atomic-call and fence validation.
`ProjectOopLlvmLowerer` validates the carried metadata again before mapping it to LLVM orderings,
so malformed metadata cannot bypass the frontend contract.

### Directed verification

`nsc_ns3_t10_memory_model_tests` compiles an edition-3 program at O0, inspects LLVM for atomic
loads/stores/RMW/fences and volatile accesses, links it with NSLink, and executes it. It also checks
precise diagnostics for invalid orders. Local result: **12/12**.

No baseline, mini-baseline, or general suite was run.
