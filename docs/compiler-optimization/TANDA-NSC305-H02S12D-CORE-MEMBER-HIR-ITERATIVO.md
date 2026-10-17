# NSC 3.0.5 — H02S12D: core member HIR iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada de receivers para Length e IteratorCurrent. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Incluido | ArrayLength, StringLength e IteratorCurrent con contrato válido. |
| Excluido | FixedArrayLength constante, getters, extensiones, campos e índices. |
| Contratos | Addressability, by-reference item, tipo, element type y orden. |
| Validación | Diferida a la tanda integral por instrucción expresa del usuario. |

## Cambio

La máquina común incorpora `CoreMember`. El receiver se baja primero en contexto de valor y el
nodo core se reconstruye al desenrollar. `IteratorCurrent` conserva la aceptación de address
context sólo cuando su resultado es `ref:` o `refmut:`; los casos inválidos permanecen en la ruta
anterior para emitir el mismo diagnóstico.

`FixedArrayLength` no entra en este corte porque es una constante y no tiene arista recursiva de
receiver.

## Auditoría estática

- El receiver se evalúa una vez.
- ArrayLength/StringLength siguen rechazando address context antes de bajar el receiver.
- IteratorCurrent mantiene su regla de referencia.
- `resultTypeKey` y `elementTypeKey` provienen del mismo binding core.
- El nodo conserva un único receiver y el mismo provenance.
- No se modificaron parser, Sema, MIR, LLVM, ABI, NSMETA ni formatos.

## Validación diferida

No se compiló ni se ejecutaron pruebas. La validación se acumula para la tanda integral solicitada
por el usuario después de completar toda la implementación pendiente.
