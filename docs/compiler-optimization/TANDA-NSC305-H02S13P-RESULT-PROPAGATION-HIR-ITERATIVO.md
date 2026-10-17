# NSC 3.0.5 — H02S13P: result propagation HIR iterativo

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada recursiva del operando postfix `?`. |
| Naturaleza | Refactor interno no funcional del lowering AST→HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivo funcional | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Binding de propagación, structured return, cleanup y operando. |
| Excluido | Otros postfix, throw/return statements, parser, Sema, MIR, LLVM, ABI y NSMETA. |
| Contratos | Tags, factories, extractors, conversión de error, cleanup y tipo resultado. |
| Ejecución | Sin build ni pruebas por instrucción expresa; sólo auditoría estática. |

## Cambio

La máquina común incorpora el frame `ResultPropagation`. Sólo se activa para la ruta semánticamente
completa: binding presente, operando presente y structured exit de tipo `Return`. El frame conserva
ambos bindings, baja el operando mediante la pila explícita y reconstruye el nodo una sola vez.

La reconstrucción copia sin reinterpretación:

- tipos carrier/error de origen y retorno;
- tags success/error y return-error;
- ordinals de test, extractores, factory y conversión;
- regiones de cleanup y continuación `Return`;
- tipo resultado y operando único.

Los casos addressable o con binding incompleto permanecen en la ruta histórica para conservar
exactamente `postfix '?' produces a value...` y `T07-F01`.

## Auditoría estática

- Cero reentrada por `ResultPropagationExpression → operandSyntax` en la ruta válida.
- El operando se baja una sola vez en value context.
- Los once campos contractuales y la lista de cleanup se copian de los mismos bindings.
- Result type, node kind, child order y continuation kind permanecen idénticos.
- Ningún cambio en parser, Sema, MIR, LLVM, CLI, ABI, NSMETA o formatos.
- El test ajeno `ProjectMethodBodyModelTests.cpp` permanece fuera del commit.

## Validación diferida

No se compiló ni se ejecutaron pruebas. El build de `nsc`, el contrafactual profundo, la paridad HIR
y las regresiones focales quedan reservados para la tanda integral solicitada por el usuario.
