# NSC 3.0.5 — H02S15A: contador de stackalloc HIR iterativo

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada recursiva de `stackalloc T[count] → count`. |
| Naturaleza | Refactor interno no funcional del lowering AST→HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivo funcional | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Binding resuelto, descenso del contador, layout y límite constante de pila. |
| Excluido | Arrays, builders, construcción de objetos, MIR, LLVM, ABI y NSMETA. |
| Contratos | Tipo resultado/elemento, evaluación única, límite, diagnósticos y rango de error. |
| Ejecución | Sin build ni pruebas por instrucción expresa; sólo auditoría estática. |

## Cambio

La máquina común incorpora el frame `StackAllocation`. Sólo captura una expresión no addressable
que ya tenga `MethodCoreOperationKind::StackAllocation`, operando contador y binding completos. Los
casos incompletos o el uso en address context permanecen en la ruta histórica y conservan sus
diagnósticos.

El frame baja el contador mediante la misma pila explícita y reconstruye `StackAllocation` con el
mismo `resultTypeKey`, `elementTypeKey` e hijo único. Para un contador entero constante mantiene:

- el servicio canónico `valueLayoutForTypeKey`;
- el límite conservador de 1 MiB de la reserva PE predeterminada;
- la comparación por división, sin multiplicación susceptible a overflow;
- los tres diagnósticos históricos de layout ausente, tipo sin layout y reserva excesiva.

## Auditoría estática

- Cero reentrada por `StackallocExpression → count` en la ruta válida.
- El contador se baja exactamente una vez y siempre en value context.
- Tipo, element type, node kind y orden/cardinalidad de hijos permanecen idénticos.
- La comprobación constante conserva orden, aritmética, umbral y texto de diagnóstico.
- Los rechazos previos al frame se delegan a la implementación histórica.
- Ningún cambio en parser, Sema, MIR, LLVM, CLI, ABI, NSMETA o formatos.
- El test ajeno `ProjectMethodBodyModelTests.cpp` permanece fuera del commit.

## Validación diferida

No se compiló ni se ejecutaron pruebas. El build de `nsc`, el contrafactual profundo, la paridad HIR
y las regresiones focales quedan reservados para la tanda integral solicitada por el usuario.
