# 380 — Handles, delegates y function pointers / Handle, delegate, and function-pointer debugging

## Español

CodeView distingue ahora las tres representaciones que antes parecían direcciones opacas. Los
valores `nint` cuyo nombre declara que son handles se publican como punteros al tipo nominal opaco
`NativeSharp.Win32.HANDLE`; conservan sus 64 bits físicos, pero Visual Studio y WinDbg ya no los
confunden con enteros ordinarios.

Los delegates administrados siguen mostrando su celda ABI (`Code`, `Receiver`, `ContextDestroy` y
`CellFree`), pero `Code` tiene ahora la firma lógica del delegate en vez de `void*`. Un
`extern C delegate`, cuya representación real es un puntero nativo, aparece directamente como
puntero a procedimiento con tipos de retorno y parámetros completos.

El corpus O0 ejecuta llamadas por delegate administrado y por function pointer, conserva un local
`nativeHandle` y examina el PDB final. La prueba exige la celda administrada, un miembro `Code`
tipado, el puntero `int (int)*`, el tipo nominal de handle y los tres locales correspondientes.

## English

CodeView now distinguishes the three representations that previously looked like opaque
addresses. `nint` values whose names identify handles are emitted as pointers to the opaque nominal
type `NativeSharp.Win32.HANDLE`; they retain their physical 64-bit representation, while Visual
Studio and WinDbg no longer confuse them with ordinary integers.

Managed delegates still expose their ABI cell (`Code`, `Receiver`, `ContextDestroy`, and
`CellFree`), but `Code` now carries the delegate's logical signature instead of `void*`. An
`extern C delegate`, whose real representation is a native pointer, appears directly as a
procedure pointer with complete return and parameter types.

The O0 corpus executes both a managed-delegate call and a function-pointer call, retains a
`nativeHandle` local, and inspects the final PDB. The test requires the managed cell, a typed `Code`
member, the `int (int)*` pointer, the nominal handle type, and all three corresponding locals.
