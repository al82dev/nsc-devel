# Ítem 333 — ABI Win64 de varargs / Win64 variadic ABI

## Resultado / Result

La llamada emitida conserva el tipo LLVM `retorno (prefijo, ...)` y deja la colocación física al
target `x86_64-pc-windows-msvc`. Eso activa la regla que duplica un argumento flotante situado en
los primeros cuatro slots tanto en XMM como en el registro entero correspondiente. Los argumentos
posteriores se colocan en slots de ocho bytes después del shadow space.

The emitted call retains its LLVM `return (prefix, ...)` type and delegates physical placement to
the `x86_64-pc-windows-msvc` target. Floating values in the first four slots are duplicated into
the matching XMM and general-purpose registers; later arguments use eight-byte stack slots after
the shadow space.

## Oráculo independiente / Independent oracle

`VerifyNativeSharpWin64VarargAbi.cmake` construye dos DLL sin CRT con `C:\clang-llvm`: una desde
C11 y otra desde C++20 con símbolo `extern C`. Cada callee recorre `va_list` y comprueba:

- `float -> double` en un slot de registro;
- `bool`, `byte` y `short` promovidos a `int`, tanto en registros como en stack;
- un `double` directo en otro slot de registro;
- `uint`, `long`, un puntero crudo y un callback `extern C` en la cola;
- el valor apuntado y la ejecución real del callback.

El compilador NativeSharp genera y ejecuta un consumidor contra cada DLL en O0 y O2. Los cuatro
procesos devuelven 42. Esta prueba ejecutable observa la secuencia de llamada final; no se limita a
inferirla desde IR.

## Frontera / Boundary

El soporte se certifica para Windows x64/MSVC. Una futura ABI de otro target deberá aportar su
propia regla y su propio oráculo físico; no puede heredar esta certificación por semejanza.
