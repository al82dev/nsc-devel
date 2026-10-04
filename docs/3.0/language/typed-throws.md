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
llamada, `throw` y `throw;`: el error debe quedar cubierto por un `catch` activo o poder propagarse
al tipo declarado por la función envolvente.

`throws` es contextual después de la lista de parámetros. En edición 2 puede seguir usándose como
identificador; la cláusula sólo se habilita con `<Language>3</Language>`.

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

The declared type must be `NativeSharp.Core.Exception` or derive from it. Every call, written
`throw`, and rethrow must be accepted by an active typed/catch-all handler or by the enclosing
callable's declared error type.

`throws` is contextual after a parameter list. Edition 2 can still use it as an identifier; the
clause itself requires `<Language>3</Language>`.

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
