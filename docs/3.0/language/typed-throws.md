# Typed throws / Errores tipados con `throws`

## Español

En edición 3, una función que puede terminar mediante una excepción nativa declara el tipo de error
después de sus parámetros:

```nativesharp
int Parse(string text) throws ParseError
{
    if (text.Length == 0) { throw new ParseError(); }
    return 42;
}
```

El tipo declarado debe ser `NativeSharp.Core.Exception` o derivar de él. NativeSharp comprueba toda
llamada, invocación de delegate, acceso a un accesor, `new`, inicializador `this(...)`/`base(...)` y
`await` de un future `async ... throws E`: el error debe quedar cubierto por un `catch` sin filtro
del `try` que rodea el sitio, o poder propagarse al tipo declarado por la función envolvente (el
mismo tipo o una base). Un `catch` con filtro no cuenta, porque su filtro puede rechazar el error;
el cuerpo de un `catch` y el de una lambda tampoco están cubiertos por el `try` que los rodea. Un
`throw` escrito y un `throw;` siguen la misma regla: la excepción debe quedar cubierta por un
`catch` del `try` que la rodea o por el `throws` de la función (congelamiento 3.0; antes un `throw`
no se comprobaba). Un destructor no puede declarar `throws`, así que atrapa lo que lance. Un error
tipado de generator que no es una excepción (por ejemplo `Iterator<T, int>`) no puede producirse:
`throw` y `throws` exigen `NativeSharp.Core.Exception`, así que no hay nada que comprobar.

`throws` es contextual después de la lista de parámetros. En edición 2 puede seguir usándose como
identificador; la cláusula sólo se habilita con `<Language>3</Language>`.

Los accesores declaran el contrato después de `get` o `set`. Cada accesor tiene su propio tipo de
error porque son funciones distintas en la API y en metadata:

```nativesharp
public int this[int index]
{
    get throws ArgumentOutOfRangeException { return Read(index); }
    set throws ArgumentOutOfRangeException { Write(index, value); }
}
```

```nativesharp
int Load() throws Error
{
    return Parse("data"); // ParseError deriva de Error.
}

int Main()
{
    try { return Load(); }
    catch (Error problem) { return 1; }
}
```

Un override puede eliminar el efecto o estrecharlo a una subclase, pero no introducirlo ni
ensancharlo. Los delegates administrados incluyen el error en su tipo; un method group sólo se
convierte cuando el error coincide exactamente. Las lambdas reciben el contrato del delegate
destino.

`extern C` y `extern vectorcall` no admiten `throws`. El adapter debe capturar la excepción y
convertirla explícitamente a un código, valor o protocolo aceptado por la ABI C.

En Windows x64, `throws` usa la EH nativa de NativeSharp y conserva la firma normal Win64. No usa
CLR ni excepciones administradas. El tipo de error forma parte de HIR, MIR 1.24 y NSMETA 1.37.

`Result<T,E>` representa un resultado inspeccionable y `?` realiza retorno temprano normal.
`throws E` representa control no local con unwind y cleanup. Elegir uno no cambia silenciosamente
al otro.

## English

In edition 3, a callable that may exit through a native exception declares its error type after the
parameter list:

```nativesharp
int Parse(string text) throws ParseError;
```

The declared type must be `NativeSharp.Core.Exception` or derive from it. Every call, delegate
invocation, accessor access, `new`, `this(...)`/`base(...)` initializer and `await` of an
`async ... throws E` future must be accepted by an unfiltered catch of the try around the site, or
propagate through the enclosing callable's declared error type (the same type or a base). A
filtered catch does not count, because its filter may decline the error; a catch body and a lambda
body are not covered by the try around them. A written `throw` and a `throw;` follow the same
rule: the exception must be accepted by a catch of the try around it or by the callable's `throws`
(freeze 3.0; before, a `throw` was not checked). A destructor cannot declare `throws`, so it catches
what it throws. A typed generator error that is not an exception (for example `Iterator<T, int>`)
cannot be produced: `throw` and `throws` require `NativeSharp.Core.Exception`, so there is nothing to
check.

`throws` is contextual after a parameter list. Edition 2 can still use it as an identifier; the
clause itself requires `<Language>3</Language>`.

Accessors place the contract after `get` or `set`. Each accessor has its own error type because
the getter and setter are distinct API and metadata functions:

```nativesharp
public int Value
{
    get throws ReadError { return Read(); }
    set throws WriteError { Write(value); }
}
```

An override may remove the effect or narrow it to a derived error. It cannot introduce an error on
a non-throwing contract or widen an inherited error. Managed delegates carry the error in their
type, method-group conversion requires exact error identity, and lambdas inherit the target
delegate's contract.

`extern C` and `extern vectorcall` cannot declare `throws`; an explicit non-throwing adapter must
catch and translate the error at the ABI boundary.

On Windows x64, typed throws uses NativeSharp native EH while preserving the normal Win64 function
signature. It does not use the CLR. The contract is versioned in HIR, MIR 1.24, cache identities and
NSMETA 1.37.

`Result<T,E>` is an inspectable in-band value and postfix `?` performs an ordinary early return.
`throws E` is nonlocal exceptional control flow with native unwind and cleanup. Neither mechanism
is silently lowered into the other.
