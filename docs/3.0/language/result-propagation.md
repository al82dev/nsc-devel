# Result propagation (`?`) / Propagación de Result (`?`)

## Español

En edición 3, `expresion?` consume una `variant` compatible:

```nativesharp
variant Result<T, E> { Ok(T), Error(E) }

Result<int, PublicError> Read()
{
    int value = Load()?;
    return Result<int, PublicError>.Ok(value);
}
```

El contrato es estructural y cerrado: debe haber exactamente dos casos con payload, `Ok(T)` y
`Error(E)`. El compilador no reconoce el tipo por el nombre `Result`; reconoce los miembros de
variant generados y versionados por el compilador.

La expansión semántica equivalente es:

```nativesharp
var temporary = Load();
if (!temporary.IsOk())
{
    SourceError source = temporary.__extract_Error();
    PublicError converted = explicit PublicError(source);
    return Result<int, PublicError>.Error(move(converted));
}
int value = temporary.__extract_Ok();
```

Los nombres `__extract_*` son descriptivos; no forman parte de la sintaxis pública y no pueden
invocarse directamente. La implementación evalúa `Load()` una sola vez.

Si `E` es idéntico al error del tipo de retorno no hay conversión. Si difiere, debe existir un
`explicit operator` exacto de `E` al error de retorno. Las conversiones implícitas no bastan. Si el
carrier está en una variable move-only se escribe `move(result)?`; un temporal se consume sin
ceremonia adicional.

La rama `Error` es un `return` estructurado normal: ejecuta `defer` y todos los cleanups léxicos
activos. `?` no se implementa con `throw`, no crea una excepción y no añade unwind oculto.

El `?` postfix y el ternario `?:` conviven. El parser conserva el ternario cuando encuentra el
`:` correspondiente al mismo nivel de anidación.

## English

In edition 3, `expression?` consumes a compatible two-case variant with exactly `Ok(T)` and
`Error(E)` payload cases. The contract is structural; the type does not need to be named `Result`.

The carrier is evaluated once. The Ok edge yields `T`. The Error edge extracts `E`, applies an
exact declared explicit conversion when the callable returns a different error type, constructs
the destination Error case, and performs an ordinary structured return. Active `defer` and lexical
cleanup run exactly as they do for a written `return`.

A named move-only carrier requires `move(result)?`; a temporary carrier is consumed directly.
Propagation emits branches and protocol calls, not `throw`, hidden allocation, or implicit unwind.
