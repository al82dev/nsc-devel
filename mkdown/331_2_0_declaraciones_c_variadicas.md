# 331 — Declaraciones C variádicas / Variadic C declarations

## Resultado / Outcome

NativeSharp puede describir imports C cuyo prefijo fijo termina en ellipsis:

```nativesharp
extern C from "ucrtbase.dll"
{
    public int Printf(char* format, ...);
}
```

El `...` es parte de la declaración y de su identidad, no un parámetro ficticio. Se conserva desde
lexer y AST hasta el modelo de miembros, HIR, MIR, la declaración LLVM y NSMETA 1.19. Un consumidor
por `LibraryReference` reconstruye el mismo contrato.

NativeSharp can describe imported C functions whose fixed prefix ends in an ellipsis. The marker is
part of declaration identity rather than a synthetic parameter, and survives through LLVM and
NSMETA 1.19 into precompiled consumers.

## Frontera / Boundary

- debe existir al menos un parámetro fijo antes de `...`;
- el ellipsis debe ser el último elemento;
- sólo se admite en imports `extern C` sin cuerpo;
- una firma fija y una variádica no pueden atribuirse al mismo símbolo de la misma biblioteca;
- las llamadas permanecen rechazadas hasta el punto 332, que define promociones C.

## Evidencia / Evidence

`Item331VariadicCDeclarationTests.cpp` cubre maximal munch, AST, identidad, diagnósticos, HIR,
declaración LLVM, NSMETA 1.19, rechazo de downlevel, reconstrucción por `LibraryReference` y la
frontera de llamada. Las pruebas de lexer, metadata, toolchain y migration baseline permanecen
verdes.

## Mensaje de commit / Commit message

`Añade declaraciones C variádicas / Add variadic C declarations`
