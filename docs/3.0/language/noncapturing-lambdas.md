# Non-capturing lambdas / Lambdas sin captura

Status: normative for NativeSharp language edition 3

Platform profile: Windows x64

Tracking item: `NS3-T08-F02`

## Contextual form / Forma contextual

ES: Una lambda sin captura puede convertirse directamente a un `fn*` cuando el contexto fija la
firma completa y la calling convention. Los tipos de parámetros pueden omitirse sólo en esa forma
contextual. EN: A non-capturing lambda can convert directly to `fn*` when its context supplies the
complete signature and calling convention. Parameter types may be omitted only in that contextual
form.

```nativesharp
fn* C(int) -> int callback = (value) => value + 1;
```

The target controls the ABI. `C`, `stdcall`, `fastcall`, `vectorcall`, and `native` remain distinct
type identities even when Windows x64 maps some of them to the same physical convention. A lambda
with captured state cannot convert to `fn*`, because a raw function pointer has no environment
channel.

## Unconverted form / Forma no convertida

ES: Sin un tipo destino, una lambda no capturante con parámetros explícitamente tipados y cuerpo de
expresión obtiene un tipo nominal anónimo propio. Cada expresión lambda tiene una identidad distinta,
aunque su firma coincida con otra. Su tamaño es cero, su alineación es uno y no crea environment,
celda delegate ni asignación de heap. EN: Without a target type, a non-capturing lambda with
explicitly typed parameters and an expression body receives its own anonymous nominal type. Every
lambda expression has a distinct identity even when signatures match. Its size is zero, its
alignment is one, and it creates no environment, delegate cell, or heap allocation.

```nativesharp
var increment = (int value) => value + 1;
int direct = increment(41);              // direct call to generated static body
fn* native(int) -> int pointer = increment;
```

Direct invocation lowers to the generated static callable. A later implicit conversion is permitted
only to a compatible raw `fn*`; it never converts implicitly to `fn&`. The unconverted signature uses
the NativeSharp convention. A foreign convention therefore requires an immediate contextual target.

An unconverted block-bodied lambda is rejected because edition 3 does not infer one return type from
arbitrary control flow. Supplying an explicit `fn*` target keeps block-bodied lambdas available.

## Inference and composition / Inferencia y composición

- overload resolution may select a `fn*` overload from a contextual lambda;
- a generic method may infer its type argument from another argument and then apply that completed
  `fn*` context to the lambda;
- compile-time `#run(...)` preprocessing is valid inside the lambda body;
- an anonymous lambda value may initialize static storage through a compatible `fn*` field;
- `sizeof` of the unconverted nominal lambda value is zero.

ES: Este item no implementa sintaxis de lambda genérica; eso pertenece a T08-F04. La cobertura
genérica de F02 es inferencia contextual al pasar una lambda ordinaria a un método genérico. EN: This
item does not implement generic-lambda syntax; that belongs to T08-F04. F02 generic coverage is
contextual inference when an ordinary lambda is passed to a generic method.

## Deliberate boundaries / Límites deliberados

- captures and explicit `copy`/`move`/`ref` modes belong to T08-F03;
- generic-lambda parameters and monomorphization belong to T08-F04;
- `static` local variables remain deferred to language edition 4 because their once-initialization,
  reentrancy, destruction, and module-lifetime contract is not yet defined;
- this contract is Windows x64 only.
