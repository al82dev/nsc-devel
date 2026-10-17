# NSC 3.0.5 — H02S12G: array/string/null-conditional index HIR iterativos

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar reentrada del receiver y los índices de accesos core. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Incluido | ArrayElement, RangeArrayElement, StringElement y NullConditionalArrayElement. |
| Excluido | Raw/VLA/flexible arrays, range slices y user indexers. |
| Contratos | Orden receiver→índices, from-end, rank, addressability, tipo y element type. |
| Validación | Diferida a la tanda integral por instrucción expresa del usuario. |

## Cambio

El frame `CoreIndex` conserva el receiver, recorre todos los índices en orden fuente y reconstruye
el acceso sólo cuando están disponibles. Los wrappers `FromEndExpression` se interpretan sin bajar
dos veces su sintaxis y conservan la máscara por dimensión.

El acceso null-conditional conserva exactamente un índice y produce `NullConditionalArrayLoad`.
StringElement en address context y null-conditional address siguen en la ruta anterior para emitir
sus diagnósticos exactos.

## Auditoría estática

- Receiver e índices se evalúan una vez, de izquierda a derecha.
- Array/RangeArray conservan selección Value/Address y rank.
- StringElement permanece sólo como valor.
- La máscara from-end y `booleanValue` conservan dimensiones y bits.
- Result type, element type y provenance vienen del mismo binding core.
- No se modificaron parser, Sema, MIR, LLVM, ABI, NSMETA ni formatos.

## Validación diferida

No se compiló ni se ejecutaron pruebas. La validación se acumula para la tanda integral solicitada
por el usuario después de completar toda la implementación pendiente.
