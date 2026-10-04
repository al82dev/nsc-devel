# NS3-T10-F08 — Primitivas de conteo de referencias

## Español

### Resultado

`NativeSharp.Core.Threading.ReferenceCounts` es un `struct` embebible para el bloque de control de
un `Rc`/`Arc`. No crea un objeto auxiliar ni activa un recolector. Contiene un contador strong y
uno weak de 64 bits; el weak incluye la referencia implícita que mantiene vivo el bloque mientras
exista al menos un strong.

La API productiva incluye:

- `TryRetainStrong` y `TryRetainWeak`, que detectan cero y overflow antes del CAS;
- `ReleaseStrong` y `ReleaseWeak`, con resultados tipados `Invalid`, `Retained` y `Final`;
- consultas acquire de los contadores observables;
- `ReleaseStrongAndDestroy`, con hooks explícitos de destrucción y desasignación;
- `ReleaseWeakAndDeallocate` para el último weak externo.

Retain usa CAS relaxed. Release publica con `Release`; únicamente el hilo que cambia el contador a
cero ejecuta un fence `Acquire` y recibe `Final`. Por ello dos releases concurrentes no pueden
ejecutar dos veces el destructor. El compilador no descubre destructores, allocators ni GC: el
propietario del bloque proporciona ambos hooks.

### Costos observables

La construcción y las operaciones de `ReferenceCounts` no asignan. Cada retain/release puede
reintentar un CAS bajo contención. Los callbacks sólo se invocan en el último release. Crear el
bloque de control que contiene este struct sigue siendo responsabilidad explícita de la biblioteca
o aplicación.

### Prueba dirigida

`nsc_ns3_t10_reference_count_tests` compila y ejecuta exclusivamente O0/NSLink. Comprueba CAS,
release/acquire final, strong/weak, overflow, no resurrección, underflow, ausencia de allocation,
dos releases concurrentes con destructor único y consumo desde una DLL/NSMETA. Resultado local:
**12/12**.

No se ejecutó baseline, minibaseline ni suite general.

## English

### Result

`NativeSharp.Core.Threading.ReferenceCounts` is an embeddable control-block `struct` for `Rc`/`Arc`
implementations. It creates no helper object and enables no collector. It contains 64-bit strong
and weak counters; weak includes the implicit owner that keeps the control block alive while a
strong owner exists.

The production API provides:

- `TryRetainStrong` and `TryRetainWeak`, detecting zero and overflow before CAS;
- `ReleaseStrong` and `ReleaseWeak`, returning typed `Invalid`, `Retained`, or `Final` states;
- acquire observations of both counters;
- `ReleaseStrongAndDestroy`, with explicit destroy and deallocate hooks;
- `ReleaseWeakAndDeallocate` for the last external weak owner.

Retain uses relaxed CAS. Release publishes with Release; only the thread changing a count to zero
executes an Acquire fence and observes `Final`. Consequently, two concurrent releases cannot run
the destructor twice. The compiler discovers no destructor, allocator, or GC: the control-block
owner supplies both hooks explicitly.

### Observable costs

Construction and operations do not allocate. Each retain/release may retry CAS under contention.
Callbacks run only on final release. Allocating the enclosing control block remains an explicit
library/application responsibility.

### Directed verification

`nsc_ns3_t10_reference_count_tests` compiles and runs only O0/NSLink. It covers CAS, final
release/acquire, strong/weak counts, overflow, no resurrection, underflow, allocation absence, two
concurrent releases with exactly one destructor, and consumption through a DLL/NSMETA boundary.
Local result: **12/12**.

No baseline, mini-baseline, or general suite was run.
