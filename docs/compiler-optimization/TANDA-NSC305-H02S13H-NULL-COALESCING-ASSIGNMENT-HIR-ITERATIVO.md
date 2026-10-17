# NSC 3.0.5 — H02S13H: null-coalescing assignment HIR iterativo

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada recursiva de target y fallback en `??=`. |
| Naturaleza | Refactor interno no funcional del lowering AST→HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivo funcional | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Dirección del target, fallback, conversión implícita y nodo asignación. |
| Excluido | `??`, otras asignaciones, setters, parser, Sema, MIR, LLVM, ABI y NSMETA. |
| Contratos | Orden target-address→fallback, evaluación única, tipo y diagnóstico exactos. |
| Ejecución | Sin build ni pruebas por instrucción expresa; sólo auditoría estática. |

## Cambio

La máquina común incorpora `NullCoalescingAssignment`. La primera etapa baja el target en address
context y conserva su dirección. La segunda baja el fallback en value context, aplica la misma
clasificación de conversión implícita y produce `NullCoalescingAssignment` con dirección y valor en
el mismo orden histórico.

El frame sólo se activa cuando la asignación tiene ambos operandos y el target posee tipo semántico.
La ausencia de tipo y el uso de `??=` en address context permanecen en la ruta anterior para emitir
sin cambios sus diagnósticos contractuales.

## Auditoría estática

- Cero reentrada por la arista válida `??= → target/fallback`.
- Target bajado una vez como dirección antes de bajar el fallback.
- Conversión RHS clasificada con el mismo tipo, constante entera y helper histórico.
- Conversión no identidad conserva la sintaxis del RHS como procedencia.
- Node kind, result type y orden de hijos permanecen idénticos.
- El diagnóstico de conversión `T01-F03` y su primer rango se preservan.
- Ningún cambio en parser, Sema, MIR, LLVM, CLI, ABI, NSMETA o formatos.
- El cambio ajeno en `ProjectMethodBodyModelTests.cpp` queda fuera del commit.

## Validación diferida

No se compiló ni se ejecutaron pruebas. El build de `nsc`, el contrafactual profundo, la paridad HIR
y las regresiones focales quedan reservados para la tanda integral solicitada por el usuario.
