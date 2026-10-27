# Congelamiento 3.0 P3 — un local `ref` de tipo move-only

Fecha: 2026-10-26. Revisión de partida: `08bd66185`.

## Problema

P3 da acceso por referencia a los elementos para que un `T` move-only se pueda leer sin copiarlo. El enlace más
directo no compilaba:

```
ref Guard r = ref arr[0];             // NS3020: cannot be copied into local 'r'
foreach (ref Guard g in list) { … }   // NS3020 en g
```

Dos defectos del mismo origen. El local `ref` se registraba igual que un local por valor:

1. `analyzeVariable` lo marcaba `isMoveOnly`, y la regla de NS3020 para locales (un inicializador direccionable
   sin `move`) lo tomaba como copia.
2. `initializeVariableLocal` le ponía `requiresValueDrop`, así que al quitar el error el alias se destruía al
   salir del scope y el dueño lo destruía otra vez. Sonda G4 con solo el primer cambio: 602 (dos
   destrucciones); `H1ListRefAt` con `foreach (ref …)`: dos destrucciones de más.

## SCOPE LOCK

- **Cambia.** `StatementAnalyzer.cpp`: un local `ref` no es `isMoveOnly` ni `requiresValueDrop`. Es alias de un
  lugar que destruye su dueño.
- **Idéntico.** Locales por valor, `raii`, `owned` y `scope`; la regla de préstamos de los locales `ref`; los
  locales `ref` de tipos sin destructor.

## Validación

nsc Release recompilado. Sonda `A:\temp\nsc-dec\d15\G4RefLocalMoveOnly` (`ref Guard r = ref arr[0]; r.Id = 6`),
`-O0` y `-O2`: 601 (valor 6, una destrucción). Antes: NS3020.

Casos `nsc_component_tests` en Debug y Release: 96 PASS de referencias y 30 PASS de copia. Corpus de `await`:
igual que `d04`. (Corridos junto con el cambio de biblioteca del doc 28, que no toca el compilador.)

Prueba nueva: `Freeze30ContractTests::testMoveOnlyReferenceLocal`.
