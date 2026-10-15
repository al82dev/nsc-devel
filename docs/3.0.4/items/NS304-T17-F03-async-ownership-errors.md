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

## Nota de defecto encontrado

- La combinación `owned` local (p.ej. `byte[]`) + `try/finally` + `yield` condicional dentro del
  generador dispara `Stage 45.10 structured exit references an inactive cleanup region`. Es un
  defecto del lowering async (T07), fuera del alcance de T17-F03; queda anotado para la corrección en
  T17-F09, sin stubs ni silenciarlo.
