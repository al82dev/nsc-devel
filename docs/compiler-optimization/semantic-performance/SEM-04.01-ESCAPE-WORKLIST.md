# SEM-04.01 - Propagación de escape por worklist

## Problema

El cierre de resúmenes usaba `while (changed)` y recorría todas las llamadas en cada iteración.
Además volvía a calcular el origen de cada argumento en cada pasada. Una cadena de llamadas larga
convertía la propagación monotónica en rescaneos globales repetidos.

## Solución

- Se crea una celda estable por `(callableOrdinal, parameterOrdinal)`.
- Cada llamada resuelta clasifica el origen de sus argumentos una sola vez.
- Se construyen aristas inversas `callee cell -> caller cell`.
- Las aristas se ordenan y deduplican.
- Una cola FIFO propaga únicamente una celda cuyo valor aumentó en el lattice:
  `NoEscape < Return < CallerOwned < Global < Unknown`.
- La validación diagnóstica posterior conserva el orden de los vectores originales de llamadas.

El algoritmo es iterativo y no usa recursión. Su trabajo es proporcional a celdas, aristas y
aumentos reales del lattice, no a llamadas multiplicadas por pasadas globales.

## Determinismo

Las celdas se enumeran por callable y parámetro; las aristas se ordenan; el worklist comienza en
orden de celda. El orden de hash, punteros o terminación de workers no participa.

## Validación dirigida

- Compilar y ejecutar `nsc_stage50_7_borrow_escape_analysis_tests`.
- Compilar y ejecutar `nsc_project_method_body_model_tests`.
- Sólo targets individuales y O0; sin baseline, CTest general, O1, O2 ni O3.
