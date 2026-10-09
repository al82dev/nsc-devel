# Tanda 00.05 — Comparación de oráculos estructurados

## Resultado

`compare_structured` compara valores JSON por tipo, claves, longitud, índices y valores exactos. Cada divergencia
conserva un path estable y una razón: `type-mismatch`, `missing-key`, `unexpected-key`, `length-mismatch`,
`unsupported-type` o `value-mismatch`.

La comparación no normaliza texto, no ignora campos y no acepta equivalencias accidentales como `true == 1` o
`1 == 1.0`. Los modos tolerantes deberán ser contratos explícitos y separados cuando un componente los requiera.

## Algoritmo

El recorrido usa una pila iterativa de triples `(path, expected, actual)`. No replica la estructura mediante
llamadas recursivas y puede procesar oráculos profundamente anidados sin consumir el stack del proceso.

## Evidencia enfocada

`tests/compiler-suite/controller/tests/test_oracle.py` valida igualdad, paths anidados, tipos estrictos, claves,
longitudes y una estructura de 5,000 niveles procesada con worklist explícita.
