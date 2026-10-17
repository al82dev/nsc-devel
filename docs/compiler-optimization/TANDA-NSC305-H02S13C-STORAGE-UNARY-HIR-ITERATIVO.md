# NSC 3.0.5 — H02S13C: operadores de almacenamiento HIR iterativos

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar reentradas de `++`, `--`, postfix, address-of y dereference. |
| Naturaleza | Refactor interno no funcional del lowering AST→HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivo funcional | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Stores compuestos, paso de puntero, address value y pointer load/address. |
| Excluido | Operadores de usuario, calls, asignaciones generales, MIR, LLVM, ABI y NSMETA. |
| Contratos | Address context, tipo, postfix, constante uno, bit fields y orden de hijos. |
| Ejecución | Sin build ni pruebas por instrucción expresa; sólo auditoría estática. |

## Cambio

La máquina común incorpora el frame `StorageUnary`. El clasificador excluye operadores de usuario,
que siguen reservados para H02S13B/H02S14A, y sólo captura rutas semánticamente completas. Los
rechazos por resultado addressable, tipo faltante o address-of de bit field permanecen en la ruta
histórica para conservar exactamente sus diagnósticos.

El descenso del operando usa address context para `++`, `--` y `&`, y value context para `*`. La
reconstrucción conserva:

- `CompoundStore` y la única diferencia prefix/postfix en `booleanValue`;
- paso de puntero de un elemento mediante `elementTypeKey` y constante `builtin:nint`;
- constante flotante `1.0` o entera `1` ya tipada como el destino;
- wrapper `AddressValue` sin mutar el tipo del operando;
- retorno directo del puntero para la dirección de `*p` y `PointerLoad` en value context.

## Auditoría estática

- Cero reentrada por estas cuatro familias hacia su operando en las rutas válidas.
- Cada operando se baja exactamente una vez y en el mismo context histórico.
- Los calls de operadores de usuario conservan íntegra la ruta anterior.
- Node kind, flags, operación, tipos, element type y orden de hijos permanecen idénticos.
- Los bit fields nunca exponen una dirección.
- Ningún cambio en parser, Sema, MIR, LLVM, CLI, ABI, NSMETA o formatos.
- El test ajeno `ProjectMethodBodyModelTests.cpp` permanece fuera del commit.

## Validación diferida

No se compiló ni se ejecutaron pruebas. El build de `nsc`, el contrafactual profundo, la paridad HIR
y las regresiones focales quedan reservados para la tanda integral solicitada por el usuario.
