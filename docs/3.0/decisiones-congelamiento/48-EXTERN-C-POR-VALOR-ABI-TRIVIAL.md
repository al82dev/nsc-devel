# Congelamiento 3.0, elemento 130b — un argumento `extern C` por valor debe ser ABI-trivial

Fecha: 2026-10-26. Revisión de partida: `c306d4e58`.

## Nota de trabajo

```text
HEAD: c306d4e58
Elemento: 130b del plan de 146 (hallazgo I1, reproducido al verificar interop con tipos destructibles, 129)
Contrato vigente: C-01; decisión de Alberto (2026-10-26) sobre I1, abajo
Documento histórico que podría contradecirlo: ninguno
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  src/Frontend/Bodies/CallAnalyzer.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

Un struct con destructor pasado por valor a una función `extern C` nunca se destruía. El valor pasaba al código
nativo, que no conoce ni puede correr su destructor (sondas `A:\temp\nsc-dec\d31` I1–I3: 1 destrucción de 2).
Pasarlo prestado ya se rechazaba por move-only (I4).

## Decisión de Alberto (2026-10-26)

Un parámetro `extern C` por valor debe ser ABI-trivial. Si el tipo tiene destructor o campos `owned`, la llamada
se rechaza con un diagnóstico claro, porque el código nativo no conoce ni puede cumplir su contrato de lifetime.
Siguen permitidos los structs POD/triviales por valor y los punteros o handles explícitos bajo las reglas
unsafe/FFI correspondientes.

## Contrato

Al resolver una llamada a un import `extern C from "…"` (`isImportedC`), cada argumento hacia un parámetro por
valor cuyo tipo requiere drop de valor (`typeRequiresValueDrop`: destructor o campos `owned`) se rechaza en el
argumento:

```text
extern C parameter 'milliseconds' of 'Sleep' takes 'type:…::Guard' by value, but native code cannot run its
destructor or release its owned fields; pass an ABI-trivial struct, a pointer or a handle
```

## SCOPE LOCK

- **Cambia.** Sólo las llamadas a imports `extern C` con un argumento por valor de tipo con drop de valor.
- **Idéntico.**
  - Structs triviales por valor.
  - Punteros (`Guard*`) y handles.
  - Retornos de `extern C` (un struct con destructor que vuelve se destruye una vez del lado NativeSharp).
  - Declaraciones `extern C` no llamadas (I1Unused).
  - Funciones exportadas a C.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas `A:\temp\nsc-dec\d33` en `-O0` y
`-O2`: I1Trivial 7 (`SetLastError` con un struct trivial y `GetLastError`), I1Pointer 13 (`lstrlenA` con
`Guard*`), I1Unused 5; I1Guard (`Sleep` con `Guard`) e I1Holder (`SetErrorMode` con un struct con campo `owned`)
rechazadas con el diagnóstico de arriba. Las sondas d31 I1–I3 se rechazan igual; las de punteros y `pin` (P1–P11)
no cambian.

Sin cambios en el resto (`seq.cmd f130b`): `all.cmd` idéntico al de `f121`, `asynccases` 86, `gencases` 76 y
`loopcases` 10 PASS; los 66 programas de Freeze30 en `-O0` y `-O2`; `tests/cases`, d28 y bibliotecas como antes.

Prueba nueva: `Freeze30ContractTests::testImportedCArgumentTrivial` (F130bTrivialStruct 7, F130bPointerToGuard
13, F130bGuardByValue y F130bOwnedFieldByValue rechazadas). Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el
gate final.

## No verificado

- Batería completa y unitarias: gate final.
