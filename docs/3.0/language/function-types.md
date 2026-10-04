# Complete function types / Tipos función completos

Status: normative for NativeSharp language edition 3  
Platform profile: Windows x64  
Tracking item: `NS3-T08-F01`

## Syntax / Sintaxis

```nativesharp
fn* C(int, byte*) -> int
fn* stdcall(nint, nint) -> int
fn*? vectorcall(Float32x4) -> Float32x4
fn&[request](Request) -> Response noheap throws RequestError
fn&?(int) -> int unsafe
fn* C(char*, ...) -> int
```

The grammar is:

```text
function-type := "fn" carrier ["?"] [lifetime] [calling-convention]
                 "(" parameters [", ..."] ")" "->" result
                 {effect} ["throws" error-type]
carrier       := "*" | "&"
calling-convention := "native" | "C" | "stdcall" | "fastcall" | "vectorcall"
effect        := "unsafe" | "noheap" | "async"
```

ES: `fn*` es un puntero de código crudo. `fn&` es una referencia callable nativa con canal de
receptor/environment. `?` pertenece al carrier y permite `null`; no se infiere por ser un puntero.
Una lifetime nombrada sólo es válida en `fn&`. EN: `fn*` is a raw code pointer. `fn&` is a native
callable reference with a receiver/environment channel. `?` belongs to the carrier and permits
`null`; pointer shape alone does not imply nullability. A named lifetime is valid only on `fn&`.

`fn&` always uses the NativeSharp calling convention. Foreign calling conventions require `fn*`.
Variadicity requires a foreign `fn*`, at least one fixed parameter, and `...` as the final item.
A foreign pointer cannot carry NativeSharp typed errors across its boundary.

## Type identity / Identidad de tipo

Function types are structural. Identity contains, in order:

1. carrier (`ptr` or `ref`);
2. calling convention;
3. variadic, unsafe, noheap, async, and nullable flags;
4. erased lifetime identity;
5. ordered parameter type identities;
6. result type identity;
7. typed-error identity, if present.

The compiler serializes this contract as a version-independent, length-prefixed canonical key.
Length prefixes make nested function, tuple, generic, and module-qualified types unambiguous; no
consumer may split a key on punctuation. The same key feeds overload resolution, generic
specialization, mangling, MIR/backend typing, cache identity, and NSMETA.

ES: Los nombres de parámetros no forman parte de la identidad. El orden y todos los componentes
anteriores sí. Dos tipos que sólo difieren en carrier, ABI, efectos, nullability, lifetime o error
son distintos. EN: Parameter names are not part of identity. Order and every component above are.
Types differing only in carrier, ABI, effects, nullability, lifetime, or error are distinct.

## Implicit compatibility / Compatibilidad implícita

| Source / Origen | Target / Destino | Result / Resultado |
|---|---|---|
| identical complete type | identical complete type | allowed / permitida |
| non-null | same nullable type | allowed / permitida |
| nullable | non-null | rejected / rechazada |
| safe | same `unsafe` slot | allowed / permitida |
| `unsafe` | safe slot | rejected / rechazada |
| `noheap` | same ordinary slot | allowed / permitida |
| ordinary | `noheap` slot | rejected / rechazada |
| nonthrowing | same `throws E` slot | allowed / permitida |
| `throws E` | nonthrowing slot | rejected / rechazada |
| different error, carrier, ABI, parameters, result, lifetime, variadicity, or async bit | any | rejected / rechazada |

Calling an `unsafe` function value requires an explicit `unsafe { ... }` block. Calling a typed
throwing value must satisfy the same catch-or-propagate rule as a direct throwing call. Variadic
arguments receive the established C default promotions and must remain by-value C ABI values.

## Representation and ABI / Representación y ABI

- `fn*` is one pointer-sized raw address and never allocates.
- `fn&` uses the established managed callable-cell representation; copying and cleanup follow the
  same ownership path as nominal NativeSharp delegates.
- A noncapturing method/lambda converted to foreign `fn*` uses the guarded callback thunk so native
  exceptions do not escape through C frames.
- On Windows x64, `C`, `stdcall`, and `fastcall` retain their distinct type identities even where
  the physical platform convention converges. `vectorcall` remains physically distinct.
- Structural function keys are preserved verbatim in NSMETA, while every nested nominal type is
  included and verified as a metadata dependency.

## Examples / Ejemplos

```nativesharp
private static int Increment(int value) { return value + 1; }

private static int Apply(fn* C(int) -> int callback, int value)
{
    return callback(value);
}

fn* C(int) -> int raw = Increment;
fn&(int) -> int managed = Increment;
```

Overloads may differ by complete function type. Function types may be generic arguments and public
NSMETA parameter/return types. A DLL import may accept a foreign callback directly:

```nativesharp
extern C from "user32.dll"
{
    public int EnumWindows(fn* stdcall(nint, nint) -> int callback, nint state);
}
```

## Deliberate boundaries / Límites deliberados

This item defines and implements function types; it does not claim completion of explicit closure
capture modes, generic lambdas, async state machines, or `must_tail`. Those remain separate T08/T10
items. Nominal `delegate` declarations remain source-compatible and keep their established ABI.

