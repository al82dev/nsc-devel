# NS304-T03-F02 — captura rest/slice prestada

Estado: `CERRADO`.

## Contrato

El elemento `..` puede portar un subpattern de captura. Una captura como
`.. var middle` representa la porción comprendida entre el prefijo y el
sufijo fijos. La porción se obtiene del origen mediante `Slice(start, length)`:
no se materializa otra colección ni se copian sus elementos, y el resultado
conserva el lifetime prestado declarado por el tipo de origen.

## Implementación

El parser conserva el subpattern escrito después de `..`. Durante el lowering
de bindings sintetiza una única llamada `Slice(prefixCount,
Length-fixedCount)` y entrega su resultado al binding ordinario. Los checks de
longitud y de los elementos fijos siguen precediendo la captura, por lo que un
origen demasiado corto nunca ejecuta el slice.

## Evidencia puntual

- `TL-LANG-309`: vista `copy borrowed` con `Length`, indexador y `Slice`;
  `[10, .. var middle, 40]` observó `middle` como la vista `(20, 30)`, terminó
  con código `52` y pasó en 388498 microsegundos.
- El negativo del mismo caso intentó devolver una captura enraizada en
  `stackalloc`; el compilador la rechazó con `borrows from local 'storage' and
  cannot be returned`.
- `TL-LANG-308` y `TL-LANG-261`..`TL-LANG-270`: 11/11 `PASS`; no se ejecutó
  batería completa.
- Build focal: `Parser.cpp` compilado, `nsc_frontend` rearchivado con sus 142
  objetos, `nsc.exe` enlazado con 98 inputs y runner cerrado en 69+2+2 aristas
  canónicas, sin regenerar CMake.
