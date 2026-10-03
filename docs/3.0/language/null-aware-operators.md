# NativeSharp 3.0 — Operadores nulos / Null-aware operators

## Español

NativeSharp 3.0 tiene tres operaciones con evaluación corta y sin boxing implícito. Su matriz es
deliberadamente más estrecha que la de un runtime administrado:

| Operación | Carrier admitido | Resultado | Rama diferida |
|---|---|---|---|
| `left ?? fallback` | puntero, referencia de clase/interfaz, delegate, array, string | el mismo carrier físico | `fallback`, sólo si `left` es null |
| `optional ?? fallback` | `NativeSharp.Core.Optional<T>` | `T` | `fallback`, sólo si `HasValue()` es falso |
| `place ??= fallback` | almacenamiento direccionable de carrier físico anulable | carrier almacenado o existente | `fallback` y store, sólo si el valor actual es null |
| `receiver?.Member` | referencia de clase/interfaz | carrier físico anulable del miembro | acceso/getter/call y argumentos, sólo si el receptor no es null |

El operando izquierdo o receptor se evalúa exactamente una vez. En una llamada
`Make()?.Next(Mark())`, `Make()` siempre se ejecuta una vez; `Mark()` y la llamada sólo se ejecutan
si el resultado de `Make()` no es null. Esto vale para campos, getters, métodos directos,
virtuales, interfaces, temporales y cadenas como `a?.Next()?.Owner`.

`?.` no convierte silenciosamente un resultado valor ni `void` en `Optional<T>`. Tal conversión
necesitaría construir un carrier y sería una operación distinta visible en ABI y ownership; por
eso se diagnostica y no se boxea. El miembro usado con `?.` debe producir un carrier físicamente
anulable.

`Optional<T>` representa **vacío**, no null. Su coalescencia llama exclusivamente al protocolo
público sellado de `NativeSharp.Core.Optional<T>`: `HasValue(): bool` y
`UncheckedValue(): T`. Un struct de usuario con métodos del mismo nombre no sobrecarga `??`.
`??=` tampoco trata un Optional vacío como una ubicación null: `place ??=` opera sobre carriers
físicos, mientras `optional ?? fallback` extrae un valor.

En O0, HIR/MIR conserva nodos explícitos y LLVM emite bloques/PHI. No se usa `select`, no se
evalúa anticipadamente la rama diferida y no se introduce allocation ni boxing.

## English

NativeSharp 3.0 provides three short-circuiting operations with no implicit boxing. Their matrix
is intentionally narrower than a managed runtime's:

| Operation | Accepted carrier | Result | Deferred branch |
|---|---|---|---|
| `left ?? fallback` | pointer, class/interface reference, delegate, array, string | the same physical carrier | `fallback`, only when `left` is null |
| `optional ?? fallback` | `NativeSharp.Core.Optional<T>` | `T` | `fallback`, only when `HasValue()` is false |
| `place ??= fallback` | addressable storage of a physical nullable carrier | existing or stored carrier | `fallback` and store, only when the current value is null |
| `receiver?.Member` | class/interface reference | the member's physical nullable carrier | field/getter/call and arguments, only when the receiver is non-null |

The left operand or receiver is evaluated exactly once. In `Make()?.Next(Mark())`, `Make()` always
runs once; `Mark()` and the call run only when `Make()` returns non-null. This holds for fields,
getters, direct/virtual/interface methods, temporaries, and chains such as
`a?.Next()?.Owner`.

`?.` does not silently turn a value or `void` result into `Optional<T>`. That would construct a
carrier and create a distinct ABI/ownership operation, so it is diagnosed instead of boxed. A
member used through `?.` must return a physically nullable carrier.

`Optional<T>` represents an **empty state**, not null. Coalescing consumes only the sealed public
`NativeSharp.Core.Optional<T>` protocol: `HasValue(): bool` and `UncheckedValue(): T`. A user
struct with lookalike methods cannot overload `??`. `??=` does not treat an empty Optional as a
null location either: `place ??=` updates physical carriers, while `optional ?? fallback`
extracts a value.

At O0, HIR/MIR retains explicit nodes and LLVM emits blocks plus PHI nodes. It does not use an
eager `select`, evaluate the deferred branch early, allocate, or box.
