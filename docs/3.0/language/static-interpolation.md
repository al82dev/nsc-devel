# Interpolación estática / Static interpolation

## Español

Una interpolación cuyos holes son constantes `bool` o integer se resuelve por completo durante la
compilación. El ejecutable recibe un literal inmutable; no llama a `String`, no concatena en
runtime, no asigna heap y no consulta cultura global.

```nativesharp
string text = $"answer={42:D4} hex={255:X2} enabled={true}";
```

Los formatos built-in son vacío, `D`/`d` y `X`/`x`, con ancho opcional de 0 a 64. En decimal, el
signo forma parte del ancho: `{-42:D5}` es `-0042`. `-0` se normaliza a cero. `X` y `x` controlan
el casing de forma determinista.

El parser produce una secuencia ordenada de partes. `StaticInterpolationHandler` recorre esa
secuencia una sola vez y recibe cada literal/hole en orden textual. El handler built-in genera el
literal ordinario; un handler elegido por el compilador para un tipo usuario puede implementar el
mismo contrato sin que la interpolación dependa de `NativeSharp.Core.String`.

`{{` y `}}` representan llaves literales. Un hole vacío, una llave sin pareja o un formato no
aceptado recibe un diagnóstico causal. El resultado constante puede usarse dentro de un callable
`noheap`.

Cuando un hole es una expresión runtime, la forma se desazucara a la superficie explícita de
`NativeSharp.Core.String`. Esa ruta preserva evaluación única y orden textual, pero puede asignar;
por ello `noheap` la rechaza. Si el proyecto no referencia una superficie de formatting, la
compilación falla cerrada con el handler ausente.

## English

An interpolation whose holes are constant booleans or integers is resolved completely at compile
time. The executable receives immutable literal data: it does not call `String`, concatenate at
runtime, allocate heap storage, or consult global culture.

The built-in formats are empty, `D`/`d`, and `X`/`x`, with an optional width from 0 through 64. A
decimal sign participates in width, so `{-42:D5}` renders `-0042`; `-0` canonicalizes to zero.
`X` versus `x` controls deterministic casing.

The parser produces an ordered sequence of parts. `StaticInterpolationHandler` traverses it once
and receives each literal/hole in source order. The built-in handler emits an ordinary literal; a
compiler-selected handler for a user type can implement the same contract without depending on
`NativeSharp.Core.String`.

`{{` and `}}` denote literal braces. Empty holes, unmatched braces, and unsupported formats receive
causal diagnostics. A fully static result is valid inside a `noheap` callable.

When a hole is a runtime expression, syntax lowers to the explicit `NativeSharp.Core.String`
surface. That path preserves single evaluation and textual order, but may allocate and is rejected
by `noheap`. A project without a formatting surface fails closed instead of acquiring a hidden
runtime dependency.
