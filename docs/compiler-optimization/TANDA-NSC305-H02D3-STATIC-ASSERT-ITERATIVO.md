# NSC 3.0.5 — H02D3: evaluación iterativa de static_assert

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de proceso al evaluar constantes HIR de `static_assert`. |
| Algoritmo | Frames explícitos postorden con selección perezosa para binary lógico y conditional. |
| Símbolos | `evaluateStaticAssertConstant` y descarte del HIR temporal en `lowerStatement`. |
| Incluido | Integer, floating, boolean, conversion, unary, binary y conditional ya aceptados. |
| Excluido | Cambiar folding, tipos, diagnósticos, sintaxis o la propiedad general de destrucción de árboles HIR. |
| Terminación | Profundidad 32,768, cortocircuito, regresión static-assert/HIR, `nsc` y runtime estático. |

## Rojo reproducido

El foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-static-assert-depth` construye una condición booleana
de 32,768 niveles alternando unary y conditional. Antes del cambio terminó con `-1073741571`
(`0xC00000FD`, agotamiento de pila).

La primera conversión del evaluador a frames eliminó su autorrecursión, pero el foco siguió rojo:
la condición HIR temporal se destruía al salir de `lowerStatement` mediante la cadena profunda de
`unique_ptr`. La tanda incluye por ello el descarte iterativo de ese árbol temporal. La destrucción
general de HIR permanece registrada como trabajo separado de soporte; no se declara resuelta aquí.

## Implementación

- El evaluador usa frames `{expression, stage, first}` y conserva recorrido postorden.
- Conversion, unary y binary reutilizan combinadores locales sin cambiar normalización, signo,
  anchos, comparaciones, arithmetic, shifts, división o diagnósticos.
- `&&` y `||` conservan cortocircuito exacto: el operando derecho no se visita cuando el izquierdo
  determina el resultado.
- Conditional evalúa primero la condición y sólo desciende a la rama elegida.
- El HIR temporal de la condición se libera con una worklist antes de cualquiera de las tres salidas
  de `static_assert`: no constante, assertion falsa o éxito.
- La búsqueda mecánica deja una definición y una llamada ordinaria de
  `evaluateStaticAssertConstant`; no queda autorrecursión.

No se modificaron sintaxis, API, ABI, NSMETA, formato HIR ni textos de error.

## Evidencia focal

- Profundidad 32,768: rojo previo `0xC00000FD`; después `PASS 2/2`.
- Cortocircuito `false && X` y `true || X` con operando no constante si se visita: `PASS 2/2`.
- Foco mixto H02C5: `PASS 2/2`.
- `nsc_static_assert_tests`: `PASS 4/4`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16`: exit `0`.
- `nsc.exe static host-runtime contract`: `passed`.
- `git diff --check`: limpio antes de incorporar este expediente.

H02 permanece abierto. No se ejecutó baseline, deploy, instalación, batería completa ni consumidor
externo.
