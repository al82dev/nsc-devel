# NSC 3.0.5 — H02C4: expresiones conditional iterativas

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de proceso en árboles `ConditionalExpression`. |
| Algoritmo | Máquina DFS con frames explícitos para condición, rama verdadera, rama falsa y combinación postorden. |
| Símbolos | `lowerExpression` y `buildConditionalExpression`. |
| Incluido | Conditional de valor sin `coreBinding`, `callBinding`, delegate ni lambda. |
| Excluido | Contexto de dirección y expresiones con bindings especiales; conservan el dispatcher existente. |
| Terminación | Profundidad 32,768, regresiones numéricas/HIR, `nsc` y runtime estático. |

## Rojo reproducido

El foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-conditional-depth`, con una cadena de 32,768
condicionales anidados por la rama falsa, terminó antes del cambio con `-1073741571`
(`0xC00000FD`, stack overflow).

## Implementación

- La ruta iterativa sólo admite `ConditionalExpression` sin bindings especiales y fuera de
  `addressContext`; cualquier otra variante conserva el dispatcher previo.
- Cada frame procesa estrictamente condición → rama verdadera → rama falsa, preservando el orden
  observable y la construcción postorden del HIR.
- Los subárboles que no son conditional admisibles se bajan como hojas mediante `lowerExpression`.
- `buildConditionalExpression` concentra la combinación que usan tanto la máquina iterativa como
  el fallback existente.
- Se conservaron el tipo resultante decidido por `MethodExpressionTypeBinding`, el fallback de
  delegate, la clasificación de constantes enteras, las conversiones implícitas de ambas ramas,
  los rangos de sintaxis y el orden de los tres hijos HIR.
- Un fallo de hoja conserva el rango del frame conditional más interno cuando la hoja no aporta
  un rango de error.

No se modificaron sintaxis, ABI, NSMETA, reglas de conversión, diagnósticos públicos ni formatos.

## Evidencia focal

- Foco `hir-conditional-depth`: rojo previo `0xC00000FD`; después `PASS 2/2`.
- Profundidad probada: 32,768; se verificaron condición, ambas ramas, tipo común y literal hoja.
- Regresiones H02C1/H02C2/H02C3: `PASS 2/2`, `PASS 2/2` y `PASS 2/2`.
- `nsc_ns3_t14_numeric_tests`: `PASS 21/21`.
- `nsc_ns3_t14_numeric_conversion_tests`: `PASS 10/10`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato estático de runtime de `nsc.exe`: aprobado.
- `git diff --check`: limpio antes de incorporar este expediente.

H02 permanece abierto para null-coalescing, calls/aggregates y statement/block. No se ejecutó
baseline, deploy, instalación ni batería completa.
