# Tanda correctiva: `match` contextual en edición 2

## Problema

El lexer conserva `match` como identificador contextual, pero el parser interpretaba cualquier
`match(` como expresión o sentencia de patrones antes de consultar la edición. En edición 2,
llamar un parámetro o método llamado `match` producía NS3002 y después una cascada sintáctica.

Esto rompía compatibilidad de fuente en `NativeSharp.Core.Collections.Generic`: sus predicados
históricos llamados `match` dejaron de poder invocarse aunque el proyecto siguiera correctamente
en edición 2.

## Cambios

- `Parser.cpp` reconoce la sintaxis de patrones `match(...)` sólo en edición 3.
- En edición 2, `match` continúa por la ruta ordinaria de expresión identificadora.
- `NativeSharp3T07ErrorTests.cpp` amplía la regresión de palabras contextuales para compilar y
  ejecutar juntas llamadas a métodos llamados `throws` y `match` bajo `<Language>2</Language>`.

## Archivos

- Modificado: `src/Parser/Parser.cpp`.
- Modificado: `tests/unit/NativeSharp3T07ErrorTests.cpp`.
- Nuevo: este documento de trazabilidad.

## Mejora

La edición 3 conserva su sintaxis `match`; la edición 2 recupera compatibilidad de identificadores
sin renombrar bibliotecas ni mezclar ediciones dentro del grafo de proyectos.

## Validación permitida

Sólo se construye y ejecuta `nsc_ns3_t07_error_tests`. Después, los tres proyectos edición 2 que
fallaron se verifican individualmente. No se ejecuta baseline, CTest general ni matrices.

Resultado:

- T07: 54/54 comprobaciones aprobadas.
- `c27_algoritmos` O0: emisión de objeto aprobada.
- `c35_sobrecarga_generica` O0: emisión de objeto aprobada.
- `c52_lazy_generic_members` O0: emisión de objeto aprobada.
- Baseline, CTest general y matrices: no ejecutados.
