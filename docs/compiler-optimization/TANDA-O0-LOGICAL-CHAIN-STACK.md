# Tanda: cadenas lógicas seguras con host O0

## Problema

Un NSC construido como `Release + O0` terminaba con `0xC00000FD` durante `method-bodies` al compilar
expresiones extensas formadas por `&&` o `||`. El caso se observó en `Aabb3.Intersects` de
`NativeSharp.Math.Geometry` y en comparaciones de matrices de `NativeSharp.Math.Domains`.

La causa estaba en `ProjectMethodBodyModel::analyzeBinary`: cada nodo izquierdo de una cadena lógica
invocaba recursivamente `analyzeExpression`. En el objeto O0 observado, `analyzeExpression` reservaba
54,344 bytes de pila y `analyzeBinary` reservaba 19,568 bytes adicionales por nivel.

## Corrección

Las cadenas homogéneas y asociativas a la izquierda de `&&` y `||` se recorren ahora iterativamente.
Para cada nodo se conserva:

- el orden de evaluación;
- el análisis condicional del operando derecho;
- los estados `whenTrue` y `whenFalse` del cortocircuito;
- el join de ownership y definite assignment;
- la validación de operandos booleanos;
- la entrada de flujo asociada a cada nodo de sintaxis.

No se aumentó el tamaño de pila, no se forzó O2 y no se modificaron las bibliotecas.

## Archivos

- `src/Frontend/ProjectMethodBodyModel.cpp`: recorrido iterativo de cadenas lógicas.
- `tests/unit/ProjectMethodBodyModelTests.cpp`: regresión con 129 operandos para `&&` y `||`.

## Verificación dirigida

- `nsc_project_method_body_model_tests.exe`: 59/59 checks.
- Construcción del target `nsc` con `CMAKE_BUILD_TYPE=Release` y `NSC_HOST_OPTIMIZATION=O0`: correcta.
- `NativeSharp.Math.Geometry`, O0, 12 trabajos: correcta.
- `NativeSharp.Math.Domains`, O0, 12 trabajos: correcta.

No se ejecutó baseline.
