# NSC 3.0.5 — H01E: extracción de expresiones HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Mover dispatcher, implementación y helpers directos de expresiones a `HirExpressionMachine.cpp`. |
| Naturaleza | Extracción mecánica byte por byte; ninguna conversión iterativa en H01. |
| Símbolos | `lowerExpression`, `lowerExpressionImpl`, `accessorGetterBinding`, `isNaturallyAddressable`, `stringCallLowering`, `isComparison`. |
| Protegido | Semántica, orden, diagnostics, calls, async, aggregates, conversions, HIR/MIR, ABI y NSMETA. |
| Terminación | Definiciones únicas, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado

- Se movieron aproximadamente 5,700 líneas sin reindentación ni reescritura.
- `git diff --check`: limpio.
- `nsc` construido; contrato de runtime estático aprobado.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.

H01E queda cerrado. El dispatcher conserva su algoritmo actual; H02 será responsable de frames,
ValueStack, contrafactual profundo y paridad exacta.
