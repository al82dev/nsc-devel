# NS3 baseline: entrada PE estable para T37

## Problema

T37 enlazaba `Program.Main`, una función que devuelve `int`, directamente como entry point PE y
esperaba que retornar 42 se convirtiera siempre en el código de salida del proceso. Win64 no define
ese contrato para un entry point freestanding: la terminación estable debe invocar `ExitProcess`.
La dependencia accidental produjo una corrida que salió con 0 aunque el mismo cuerpo calculaba 42.

## Cambio

La fixture exporta `DebugValuesT37Start`, un shim `void` de ABI C que entrega el resultado de
`Program.Main()` a `NativeSharp.Core.Environment.Exit`; el lowering normal conecta esa operación
con `__nativesharp_environment_exit` y finalmente con `ExitProcess`. La fixture conserva su edición
histórica y NSLink usa el shim como `/entry`; el método `Main`, sus argumentos, el `out`, el catch y
los globals que inspecciona el PDB no cambian.

## Archivos

- `tests/integration/debug_values_t37/Main.ns`
- `cmake/VerifyNativeSharpDebugValuesT37.cmake`

## Mejora

La prueba deja de depender del comportamiento incidental del retorno del loader y mide de forma
determinista que el programa devuelve 42, igual que el contrato de arranque normal de NativeSharp y
la compuerta de handoff NSLink/NSDBG.
