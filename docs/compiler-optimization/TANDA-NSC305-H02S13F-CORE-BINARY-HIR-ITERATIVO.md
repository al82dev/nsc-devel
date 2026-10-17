# NSC 3.0.5 — H02S13F: binarios core HIR iterativos

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar reentradas de los operadores binarios con binding core especial. |
| Naturaleza | Refactor interno no funcional del lowering AST→HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivo funcional | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Struct equals, string concat/equals y pointer arithmetic. |
| Excluido | Binarios builtin ya iterativos, operadores de usuario, calls, MIR, LLVM y NSMETA. |
| Contratos | Orden left→right, tipos, operación, aggregate leaves, negación y diagnóstico. |
| Ejecución | Sin build ni pruebas por instrucción expresa; sólo auditoría estática. |

## Cambio

La máquina común incorpora el frame `CoreBinary`. Sólo captura un `BinaryExpression` no
addressable con ambos operandos y uno de los cuatro bindings core válidos. Los rechazos por address
context y cualquier binding incompleto permanecen en la ruta histórica con sus diagnósticos.

El frame baja primero el operando izquierdo y después el derecho. La reconstrucción conserva:

- `AggregateEquals`, sus dos argumentos originales y las hojas físicas agregadas después;
- el wrapper `Unary/Bang` para `!=` de structs;
- `StringConcat` y `StringEquals` con el token original;
- `PointerArithmetic`, su `elementTypeKey` y el orden escrito de puntero/entero.

## Auditoría estática

- Cero reentrada por estas operaciones core hacia sus operandos.
- Cada operando se evalúa una vez en orden left→right y en value context.
- `argumentCount` de struct equals permanece fijado antes de anexar aggregate leaves.
- Node kind, result type, element type, operación y orden de hijos permanecen idénticos.
- Operadores de usuario y binarios builtin conservan sus rutas separadas.
- Ningún cambio en parser, Sema, MIR, LLVM, CLI, ABI, NSMETA o formatos.
- El test ajeno `ProjectMethodBodyModelTests.cpp` permanece fuera del commit.

## Validación diferida

No se compiló ni se ejecutaron pruebas. El build de `nsc`, el contrafactual profundo, la paridad HIR
y las regresiones focales quedan reservados para la tanda integral solicitada por el usuario.
