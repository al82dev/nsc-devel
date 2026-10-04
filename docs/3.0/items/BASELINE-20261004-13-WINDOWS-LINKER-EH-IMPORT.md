# Baseline 2026-10-04: importación EH del ejecutable Windows

## Problema

La prueba Stage 19 prohibía cualquier dependencia de `ucrtbase.dll`. Desde que el runtime protege
`Main` con un funclet Win64, el ejecutable necesita resolver `__C_specific_handler`; la propia
batería Stage45 ya congela que ese símbolo procede del UCRT de Windows.

## Cambio

La prueba del enlazador exige ahora:

- presencia de `ucrtbase.dll`;
- importación concreta de `__C_specific_handler` desde esa biblioteca;
- ausencia de `vcruntime140.dll`, conservando el requisito de no depender del redistribuible de
  Visual C++.

## Archivo

- `tests/unit/WindowsLinkerTests.cpp`

## Mejora

El test distingue la personalidad SEH suministrada por Windows de una dependencia dinámica del
runtime de Visual C++, en vez de rechazar ambas como si fueran equivalentes.

## Validación dirigida

- `nsc.windows-linker`: 46/46 comprobaciones correctas con NSLink.
