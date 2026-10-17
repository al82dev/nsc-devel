# NSC 3.0.5 — H02C5: máquina mixta de expresiones

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada de pila al alternar las familias ya migradas en H02C1–H02C4. |
| Algoritmo | Una máquina DFS de frames etiquetados para overflow, unary builtin, binary builtin y conditional. |
| Símbolos | `ProjectOopHirModel::lowerExpression`; los combinadores existentes binary y conditional. |
| Incluido | Transiciones arbitrarias entre las cuatro familias ya aprobadas. |
| Excluido | Bindings especiales, calls, aggregates, construction, member/index, statements y demás familias todavía recursivas. |
| Terminación | Contrafactual mixto de 32,768 niveles, regresiones C1–C4, numéricas/HIR, `nsc` y runtime estático. |

## Hueco probado

Las máquinas independientes de H02C1–H02C4 eliminaban profundidad homogénea, pero cada una llamaba
otra vez a `lowerExpression` al encontrar una familia distinta. Una cadena que alternaba
`UnaryExpression` → `BinaryExpression` → `ConditionalExpression` conservaba así una llamada de
proceso por transición.

El nuevo foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-mixed-depth`, con 32,768 niveles alternados,
terminó antes del cambio con `-1073741571` (`0xC00000FD`, stack overflow).

## Implementación

- `lowerExpression` clasifica las cuatro familias mediante una única función de admisión que
  conserva exactamente las exclusiones de H02C1–H02C4.
- Un solo vector de frames retiene clase, sintaxis, etapa y resultados parciales. Cambiar de familia
  ya no vuelve a entrar en `lowerExpression`.
- Overflow y unary se completan con un hijo; binary conserva izquierda→derecha; conditional conserva
  condición→rama verdadera→rama falsa.
- Los combinadores `buildBuiltinBinaryExpression` y `buildConditionalExpression` permanecen como
  autoridad única para conversiones, tipos, enum cero y forma de los hijos.
- `addressContext` se propaga al operando overflow y se fuerza a valor en unary, binary y conditional,
  igual que en las cuatro rutas anteriores.
- Una hoja no admitida conserva el dispatcher anterior. Si falla sin rango, el frame mixto más
  interno conserva la atribución del error.

No se modificaron sintaxis, API, ABI, NSMETA, bindings, conversiones, diagnósticos públicos ni
formatos.

## Evidencia focal

- Foco mixto de 32,768 niveles: rojo previo `0xC00000FD`; después `PASS 2/2`.
- Focos homogéneos H02C1–H02C4: cinco ejecuciones acumuladas, cada una `PASS 2/2` incluida la mixta.
- `nsc_ns3_t14_numeric_tests`: `PASS 21/21`.
- `nsc_ns3_t14_numeric_conversion_tests`: `PASS 10/10`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16`: exit `0`.
- `nsc.exe static host-runtime contract`: `passed`.
- `git diff --check`: limpio antes de incorporar este expediente.

## Frontera de cierre de H02

H02 permanece abierto. La auditoría posterior a C5 mantiene bajo su dueño las rutas aggregate,
deconstruction, `static_assert`, el SCC expresión/call/construction/statement/block, cinco walkers
del coordinador y la clasificación explícita de dumper y destrucción profunda del árbol HIR. Los
consumidores recursivos de HIR dentro de MIR pertenecen a M02.

No se ejecutó baseline, deploy, instalación, batería completa ni consumidor externo.
