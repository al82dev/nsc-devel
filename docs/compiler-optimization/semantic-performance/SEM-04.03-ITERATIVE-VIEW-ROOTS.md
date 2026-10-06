# SEM-04.03 — Raíces de vistas sin recursión

## Propósito

Eliminar crecimiento del stack nativo al resolver la raíz de una vista o de una cadena de
alias de punteros durante el análisis de escape.

## Cambio

`ProjectMethodBodyModel::viewRootLocalOfExpression` usa ahora una pila explícita alojada en
heap y un conjunto de nodos visitados. El orden observable anterior se conserva:

1. identificador actual;
2. inicializador de un alias de puntero;
3. `first`, `second`, `third`;
4. `children` en orden fuente.

El conjunto de visitados también impide que una cadena cíclica de inicializadores vuelva a
examinar indefinidamente los mismos nodos.

## Archivos

- `src/Frontend/ProjectMethodBodyModel.cpp`: reemplazo del descenso recursivo por worklist.
- `docs/compiler-optimization/semantic-performance/SEM-04.03-ITERATIVE-VIEW-ROOTS.md`:
  evidencia editorial del cambio.

## Mejora esperada

- consumo de stack constante respecto a la profundidad del árbol o de aliases;
- orden determinista idéntico al recorrido previo;
- terminación explícita ante ciclos de alias;
- ninguna ampliación del contrato del lenguaje.

## Validación dirigida

Se construye únicamente el objetivo que contiene el cambio y se ejecutan las pruebas
dirigidas de escape de Stage 50.7 en O0. No se ejecuta baseline, minibaseline, CTest general
ni optimizaciones O1/O2/O3.
