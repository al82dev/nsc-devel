# NS304-T03-F01 — formas base de list patterns

Estado: `CERRADO`.

## Contrato

Un list pattern evalúa primero la longitud y sólo después sus elementos. Sin
`..` exige longitud exacta; con `..` acepta un prefijo, un sufijo o ambos. `_`
descarta el elemento sin crear un binding. El valor gobernante se evalúa una
sola vez mediante el snapshot canónico de `match`.

## Implementación

El parser conserva una forma `List` privada durante el lowering temprano. La
forma se reduce a operaciones ordinarias ya productivas: `Length`, indexación,
comparaciones y branches cortocircuitados. Los elementos posteriores a `..`
se indexan desde `Length`; no se crea una colección temporal ni un backend
paralelo.

## Evidencia puntual

- `TL-LANG-308` cubre exacto, prefijo, sufijo y descarte sobre un array nativo:
  `PASS` en 361243 microsegundos.
- Compilación y ejecución del producto O0/debug: código de salida `42`.
- Guards `TL-LANG-261`..`TL-LANG-270`: 10/10 `PASS`.
- El runner se reconstruyó mediante sus 70 aristas canónicas con LLVM 23 y
  response-file interno de Ninja; no se regeneró CMake ni se ejecutó batería.

