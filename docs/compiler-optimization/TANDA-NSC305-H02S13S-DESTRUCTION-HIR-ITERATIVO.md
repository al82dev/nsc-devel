# NSC 3.0.5 — H02S13S: destrucción HIR iterativa

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada recursiva del operando de `delete`, `delete[]` y `destroy`. |
| Naturaleza | Refactor interno no funcional del lowering AST→HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivo funcional | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Dirección de elementos, no-op de value types, ownership y destrucción dinámica. |
| Excluido | Parser, Sema, MIR, LLVM, ABI, NSMETA y semántica de diagnóstico. |
| Contratos | Tipos, flags, ownership, orden de hijos y evaluación única del operando. |
| Ejecución | Sin build ni pruebas por instrucción expresa; sólo auditoría estática. |

## Cambio

La máquina común incorpora el frame `Destruction`. El frame conserva el tipo semántico del
operando y calcula antes del descenso si debe pedir su dirección: una fila directa para `delete[]`
o un elemento directo de referencia/string para el borrado escalar que limpia el slot propietario.
El operando se baja mediante la pila explícita y el nodo final se reconstruye una sola vez.

La reconstrucción conserva sin reinterpretación:

- `Deallocation`, `ObjectDestruction` y el tipo `builtin:void`;
- `dynamicDestruction`, `valueTypeNoOpDeletion` y `booleanValue`;
- `elementTypeKey`, `objectLifetimeOwnership` y `storageOwnership`;
- la tolerancia genérica de `delete` sobre value types como evaluación con no-op;
- la detección dinámica de referencias directas y punteros a referencias;
- el orden y la cardinalidad del hijo operando.

Los casos incompletos o inválidos permanecen en la ruta histórica. Así se preservan literalmente
`Stage 41 cannot determine delete operand type`, la restricción de array nativo de `delete[]` e
`item 319 cannot determine destroy operand type`.

## Auditoría estática

- Cero reentrada por `DeleteExpression/DestroyExpression → operand` en las rutas válidas.
- El operando se evalúa exactamente una vez y con el mismo address context histórico.
- `delete[]` conserva ownership del binding y limpieza de filas directas.
- `delete` escalar conserva raw pointers, strings, referencias y no-op de value types.
- `destroy` conserva destrucción dinámica y ownership `Owned/Borrowed`.
- Ningún cambio en parser, Sema, MIR, LLVM, CLI, ABI, NSMETA o formatos.
- El test ajeno `ProjectMethodBodyModelTests.cpp` permanece fuera del commit.

## Validación diferida

No se compiló ni se ejecutaron pruebas. El build de `nsc`, el contrafactual profundo, la paridad HIR
y las regresiones focales quedan reservados para la tanda integral solicitada por el usuario.
