# NSC 3.0.5 — H02C1: expresiones unary builtin iterativas

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de proceso en cadenas de operadores unary builtin. |
| Algoritmo | Recolección de frames AST y reconstrucción HIR de adentro hacia afuera. |
| Símbolo | `ProjectOopHirModel::lowerExpression`. |
| Incluido | `UnaryExpression` builtin sin bindings de llamada, delegate ni lambda. |
| Excluido | `&`, `*`, `++`, `--`, postfix y operadores de usuario. |
| Terminación | Profundidad 32,768, regresiones numérica/HIR, `nsc` y runtime estático. |

## Rojo reproducido

El foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-builtin-unary-depth`, con 32,768 operadores `-`
sobre un literal entero tipado, terminó antes del cambio con `-1073741571` (`0xC00000FD`, stack
overflow).

## Implementación

- `lowerExpression` reconoce únicamente una cadena segura de `UnaryExpression` builtin.
- Cada frame debe carecer de `MethodCallBinding`, `MethodDelegateBinding` y `MethodLambdaBinding`.
- Las operaciones con contrato especial (`&`, `*`, `++`, `--`) no entran en el fast path.
- La hoja se baja una sola vez por el dispatcher existente.
- Los nodos `Unary` se reconstruyen en postorden con el `typeKey`, token y syntax de cada frame.
- El error de la hoja conserva precedencia; un operando nulo conserva como rango el frame unary más
  interno, igual que el desenrollado recursivo.
- Las comprobaciones `noheap` observables no cambian: los frames aceptados no asignan heap y no
  tienen binding de llamada.

No se modificaron sintaxis, ABI, NSMETA, reglas numéricas, diagnósticos públicos ni formatos.

## Evidencia focal

- Foco `hir-builtin-unary-depth`: rojo previo `0xC00000FD`; después `PASS 2/2`.
- Profundidad probada: 32,768 nodos; todos los operadores y el literal hoja se preservan.
- `nsc_ns3_t14_numeric_tests`: `PASS 21/21`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato estático de runtime de `nsc.exe`: aprobado.
- `git diff --check`: limpio.

H02 permanece abierto; las familias binary, conditional, aggregate/call y statement/block todavía
deben auditarse y migrarse sin huecos. No se ejecutó baseline, deploy, instalación ni batería
completa.
