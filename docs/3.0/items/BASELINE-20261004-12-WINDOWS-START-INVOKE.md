# Baseline 2026-10-04: invocación protegida de Main

## Problema

`WindowsRuntimeTests` todavía esperaba `call i32 @Main()`. El runtime actual usa `invoke` porque
el punto de entrada debe enviar un pánico de NativeSharp al `panic.catchpad` de arranque antes de
terminar con el código reservado.

## Cambio

La prueba exige `invoke i32 @Main()` y conserva todas las comprobaciones de `ExitProcess`, ABI,
Win32 directo, Unicode, asignación y emisión COFF.

## Archivo

- `tests/unit/WindowsRuntimeTests.cpp`

## Mejora

El contrato vuelve a verificar la transferencia real hacia `Main` sin degradar la frontera de
pánico que protege el proceso.

## Validación dirigida

- `nsc.windows-runtime`: 34/34 comprobaciones correctas.
