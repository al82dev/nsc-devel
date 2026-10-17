# NSC 3.0.5 — H02C2: overflow policy iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de proceso en cadenas de `OverflowPolicyExpression`. |
| Algoritmo | Recolección de frames AST y reconstrucción HIR de adentro hacia afuera. |
| Símbolo | `ProjectOopHirModel::lowerExpression`. |
| Protegido | `addressContext`, tipo efectivo del operando, política y orden de wrappers. |
| Fuera de alcance | Unary, binary, conditional, calls, aggregates y statements. |
| Terminación | Profundidad 32,768, regresión C1/numérica/HIR, `nsc` y runtime estático. |

## Rojo reproducido

El foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-overflow-policy-depth`, con 32,768 wrappers alternando
`checked` y `unchecked`, terminó antes del cambio con `-1073741571` (`0xC00000FD`, stack overflow).

## Implementación

- `lowerExpression` recolecta únicamente frames `OverflowPolicyExpression` sin bindings de llamada,
  delegate ni lambda.
- La hoja se baja una sola vez por el dispatcher existente y recibe el mismo `addressContext` que
  atravesaba la implementación recursiva.
- Cada `ProjectOopHirNodeKind::OverflowPolicy` se reconstruye en postorden con el `typeKey` del hijo
  ya bajado y el `auxiliaryText` exacto de su frame AST.
- Un fallo de hoja conserva como rango subsidiario el wrapper más interno.
- No se mezcló este corte con la ruta unary; las dos especializaciones tienen fronteras separadas.

No se modificaron sintaxis, ABI, NSMETA, reglas de overflow, diagnósticos públicos ni formatos.

## Evidencia focal

- Foco `hir-overflow-policy-depth`: rojo previo `0xC00000FD`; después `PASS 2/2`.
- Profundidad probada: 32,768; se verificaron todos los wrappers, políticas alternadas y literal hoja.
- Regresión `hir-builtin-unary-depth`: `PASS 2/2`.
- `nsc_ns3_t14_numeric_tests`: `PASS 21/21`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato estático de runtime de `nsc.exe`: aprobado.
- `git diff --check`: limpio.

H02 permanece abierto para las familias ramificadas de expression y para statement/block. No se
ejecutó baseline, deploy, instalación ni batería completa.
