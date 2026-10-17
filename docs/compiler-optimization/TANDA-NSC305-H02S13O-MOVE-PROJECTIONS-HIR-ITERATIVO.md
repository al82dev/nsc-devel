# NSC 3.0.5 — H02S13O: proyecciones move HIR iterativas

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada recursiva de proyecciones owned usadas por `move`. |
| Naturaleza | Refactor interno no funcional del lowering AST→HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivo funcional | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Elementos de array, campos owned e `Iterator.Current`. |
| Excluido | Move de local/parámetro, asignaciones, calls, MIR, LLVM, ABI y NSMETA. |
| Contratos | Dirección del slot, tipo, clear-source, prioridad y orden de hijos. |
| Ejecución | Sin build ni pruebas por instrucción expresa; sólo auditoría estática. |

## Cambio

La máquina común incorpora el frame `MoveProjection`. Sólo captura `MoveExpression` no addressable
con binding completo y una de las tres proyecciones que antes reentraban a `lowerExpression`. El
move ordinario de local/parámetro y todos los rechazos semánticos permanecen en la ruta histórica.

El frame baja en address context el slot de un elemento o campo. Para `Iterator.Current` baja la
dirección del iterador receptor, como exigía el contrato previo. La reconstrucción preserva la
prioridad histórica array→field→iterator si un binding inconsistente tuviera más de un flag activo.

## Auditoría estática

- Cero reentrada por las tres proyecciones owned válidas.
- La dirección fuente se evalúa exactamente una vez.
- Se conservan `MoveArrayElement`, `MoveOwnedField` y `MoveIteratorCurrent`.
- `typeKey`, `clearSourceStorage`, child order y cardinalidad permanecen idénticos.
- El move ordinario conserva sus ordinals y diagnóstico Stage 50.8 en la ruta anterior.
- Ningún cambio en parser, Sema, MIR, LLVM, CLI, ABI, NSMETA o formatos.
- El test ajeno `ProjectMethodBodyModelTests.cpp` permanece fuera del commit.

## Validación diferida

No se compiló ni se ejecutaron pruebas. El build de `nsc`, el contrafactual profundo, la paridad HIR
y las regresiones focales quedan reservados para la tanda integral solicitada por el usuario.
