# Tanda correctiva histórica: `match` contextual

> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

## Problema

El lexer conserva `match` como identificador contextual, pero el parser interpretaba cualquier
`match(` como expresión o sentencia de patrones antes de consultar la etiqueta de proyecto. En la
configuración histórica, llamar un parámetro o método llamado `match` producía NS3002 y después
una cascada sintáctica.

Esto rompía compatibilidad de fuente en `NativeSharp.Core.Collections.Generic`: sus predicados
históricos llamados `match` dejaron de poder invocarse en aquella configuración.

## Cambios históricos

- La corrección de entonces seleccionaba entre la sintaxis de patrones y la expresión identificadora
  según una etiqueta de proyecto. Esa selección por edición fue retirada.
- `NativeSharp3T07ErrorTests.cpp` amplió la regresión de palabras contextuales para compilar y
  ejecutar juntas llamadas a métodos llamados `throws` y `match`.

## Archivos

- Modificado: `src/Parser/Parser.cpp`.
- Modificado: `tests/unit/NativeSharp3T07ErrorTests.cpp`.
- Nuevo: este documento de trazabilidad.

## Mejora

La campaña recuperó compatibilidad de identificadores sin renombrar bibliotecas. El lenguaje actual
no selecciona reglas mediante etiquetas de edición.

## Validación permitida

La campaña construyó y ejecutó `nsc_ns3_t07_error_tests` y verificó individualmente los tres
proyectos que fallaron. No ejecutó baseline, CTest general ni matrices.

Resultado:

- T07: 54/54 comprobaciones aprobadas.
- `c27_algoritmos` O0: emisión de objeto aprobada.
- `c35_sobrecarga_generica` O0: emisión de objeto aprobada.
- `c52_lazy_generic_members` O0: emisión de objeto aprobada.
- Baseline, CTest general y matrices: no ejecutados.
