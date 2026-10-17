# NSC 3.0.5 — H02S13D: null-coalescing HIR iterativo

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada recursiva de operandos `NullCoalescingExpression`. |
| Naturaleza | Refactor interno no funcional del lowering AST→HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivo funcional | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Operando izquierdo, RHS, conversión implícita y carrier opcional de `??`. |
| Excluido | `??=`, operadores binarios, parser, Sema, MIR, LLVM, ABI y NSMETA. |
| Contratos | Orden left→right, semántica short-circuit del HIR, tipo y diagnóstico exactos. |
| Ejecución | Sin build ni pruebas por instrucción expresa; sólo auditoría estática. |

## Cambio

La máquina común incorpora el frame `NullCoalescing`. El frame baja primero el operando izquierdo,
lo conserva y baja el RHS antes de reconstruir el nodo. Una cadena right-deep deja de reentrar en
`lowerExpression` por cada operador y utiliza la pila explícita ya compartida por la máquina.

Al reconstruir se conserva la misma clasificación de conversión implícita del RHS. Si no existe
conversión, se emite exactamente el diagnóstico `T01-F03` con los mismos tipos y el rango del nodo
interno que falló. Las conversiones no identidad conservan `wrapImplicitConversion` y la sintaxis
del RHS como procedencia.

Cuando existe `MethodOptionalCarrierBinding`, el frame produce `OptionalCoalescing` y copia
`carrierTypeKey`, `hasValueMemberOrdinal` y `uncheckedValueMemberOrdinal`. Sin carrier produce el
mismo `NullCoalescing` histórico. En ambos casos conserva los hijos left/RHS en ese orden.

## Conservación deliberada

El classifier excluye address context y operandos ausentes. Esos casos permanecen en la ruta
histórica para conservar el diagnóstico de no-addressability y cualquier error estructural previo.
No se modificaron `NullCoalescingAssignment`, los bindings ni la semántica de ejecución.

## Auditoría estática

- Cero reentrada por la arista `NullCoalescingExpression → left/right` en entradas válidas.
- Evaluación de lowering left→right y un solo lowering por operando.
- Tipo resultado, conversión, node kind, metadata opcional y orden de hijos idénticos.
- Primer rango de error preservado para fallas dentro de una cadena profunda.
- Ningún cambio en parser, Sema, MIR, LLVM, CLI, diagnósticos públicos, ABI, NSMETA o formatos.
- `tests/unit/ProjectMethodBodyModelTests.cpp` permanece ajeno y fuera del commit.

## Validación diferida

No se compiló ni se ejecutaron pruebas. El build de `nsc`, el contrafactual profundo, la paridad HIR
y las regresiones focales se acumulan para la tanda integral solicitada por el usuario después de
completar toda la implementación pendiente.
