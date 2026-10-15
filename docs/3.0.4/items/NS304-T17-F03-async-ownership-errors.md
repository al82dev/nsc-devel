# NS304-T17-F03 — Async + ownership + errors + DLL + cancellation

- **Estado**: COMPLETO — combinación async + ownership + error type + cancellation verificada.
- **Rama**: development

## Verificado O0

- `TL-LANG-514` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — `AsyncIterator<int,int>`
  con `try/finally`, `Cancel()` ⇒ `MoveNextAsync` false y `finally` corre una vez (exit 7).

## Cobertura de las cinco aristas

| arista | evidencia |
|---|---|
| async | `AsyncIterator`/`MoveNextAsync` (`TL-LANG-514`, `TL-LANG-455/456`) |
| ownership | cleanup `finally` determinista sobre el frame del generador (`TL-LANG-514`) |
| errors | error type `int` del `AsyncIterator<T,E>` (`TL-LANG-514`); `failure:`/`Result` (`TL-LANG-495..498`) |
| DLL | async-iterator producer/consumer sin fuentes (`TL-LANG-470`) |
| cancellation | `Cancel()` + terminal false (`TL-LANG-514`, `TL-LANG-455`) |

## Nota de defecto encontrado — CORREGIDO en T17-F09

- La combinación `owned` local (p.ej. `byte[]`) + `try/finally` + `yield` condicional dentro del
  generador disparaba `Stage 45.10 structured exit references an inactive cleanup region`.
- **Causa raíz**: en `ProjectOopLlvmLowerer.cpp`, la rama `cancelled` del `AsyncYieldValue` ejecutaba
  `emitOneStructuredCleanup`, que muta el estado compartido `activeCleanupRegions_` (y, vía el cuerpo
  del `finally`, `funcletStack_`). Esa rama es un bloque LLVM independiente: las ramas entry/resume y
  los hermanos que le siguen (el `if` con el segundo `yield`) se bajan después, así que la mutación se
  filtraba y el segundo yield encontraba sus regiones ya desapiladas.
- **Corrección**: snapshot/restore de `activeCleanupRegions_` y `funcletStack_` alrededor del bucle de
  cleanup de la rama `cancelled` (igual que `lowerStructuredExit`), sin debilitar la validación ni
  omitir cleanup.
- **Regresión**: `TL-LANG-520` (terminación natural, cancelación tras primer punto, suspensión sin
  cleanup, excepción tras reanudar).

