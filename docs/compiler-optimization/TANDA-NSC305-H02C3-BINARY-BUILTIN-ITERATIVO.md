# NSC 3.0.5 — H02C3: expresiones binary builtin iterativas

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de proceso en árboles `BinaryExpression` builtin. |
| Algoritmo | Máquina DFS con frames de operando izquierdo y combinación postorden. |
| Símbolos | `lowerExpression` y `buildBuiltinBinaryExpression`. |
| Incluido | Binary sin `coreBinding`, `callBinding`, delegate ni lambda. |
| Excluido | Struct/string/pointer core operations y operadores de usuario. |
| Terminación | Profundidad 32,768, regresiones numéricas/strings/enums/HIR, `nsc` y runtime estático. |

## Rojo reproducido

El foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-builtin-binary-depth`, con un árbol left-deep de 32,768
sumas y 32,769 literales, terminó antes del cambio con `-1073741571` (`0xC00000FD`, stack overflow).

## Implementación

- La ruta iterativa sólo admite frames binary sin bindings especiales; cualquier core operation,
  binding de llamada, delegate o lambda conserva el dispatcher existente.
- Cada frame retiene el hijo izquierdo ya bajado y solicita después el derecho, preservando el orden
  observable izquierda→derecha y la asignación determinista de estado auxiliar.
- Los subárboles no builtin se bajan como hojas mediante `lowerExpression`; sus descendientes builtin
  pueden volver a entrar en la máquina iterativa sin duplicar contratos.
- `buildBuiltinBinaryExpression` es la única implementación de la combinación builtin tanto para la
  ruta iterativa como para el fallback del dispatcher.
- Se conservaron `flowRuntimeCheckElided`, conversión literal cero→enum, tipo numérico común,
  conversiones implícitas, token, syntax, tipo resultante y orden de hijos.
- Un fallo de hoja conserva el rango del frame binary más interno cuando la hoja no aporta uno.

No se modificaron sintaxis, ABI, NSMETA, reglas numéricas, diagnósticos públicos ni formatos.

## Evidencia focal

- Foco `hir-builtin-binary-depth`: rojo previo `0xC00000FD`; después `PASS 2/2`.
- Profundidad probada: 32,768; se verificaron postorden, ambos operandos, operadores y literal hoja.
- Regresiones H02C1/H02C2: `PASS 2/2` y `PASS 2/2`.
- `nsc_ns3_t14_numeric_tests`: `PASS 21/21`.
- `nsc_ns3_t14_numeric_conversion_tests`: `PASS 10/10`.
- `nsc_stage41_arrays_strings_core_tests`: `PASS 79/79`.
- `nsc_stage51_enum_oop_support_tests`: `PASS 26/26`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato estático de runtime de `nsc.exe`: aprobado.
- `git diff --check`: limpio.

`nsc_ns3_t05_operator_trait_tests` no se usa como evidencia: 16 checks internos pasaron y cuatro
casos intentaron usar NSLink/runtime de una instalación bajo `C:\Program Files (x86)\NativeSharp`,
fuera de la frontera focal A:. No se modificó ni preparó esa instalación.

H02 permanece abierto para conditional, null-coalescing, calls/aggregates y statement/block. No se
ejecutó baseline, deploy, instalación ni batería completa.
