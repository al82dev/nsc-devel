# NSC 3.0.5 — H02S12F: range-slice HIR iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada de receiver, start y end del protocolo range-slice. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Incluido | Receiver explícito, endpoints presentes/omitidos y conversiones seleccionadas. |
| Excluido | Binding inválido, receiver implícito/base, índices ordinarios y user indexers. |
| Contratos | Orden receiver→start→end, flags, ordinals, reference result y tipos. |
| Validación | Diferida a la tanda integral por instrucción expresa del usuario. |

## Cambio

La máquina común incorpora un frame `RangeSlice` de tres etapas. Conserva el receiver y el start
mientras baja el siguiente operando; los endpoints omitidos se materializan como el mismo cero
`builtin:int`, y las conversiones semánticas se aplican inmediatamente después de bajar cada
endpoint.

El receiver struct conserva dirección natural o `TemporaryAddress`. Los bindings inválidos,
receiver implícito/base y address context continúan por la ruta anterior para mantener diagnósticos.

## Auditoría estática

- Orden exacto: receiver, start, end y después construcción del nodo.
- Cada sintaxis se evalúa una sola vez.
- Flags has/from-end/bounds conservan los mismos bits.
- Ordinals Length/Slice y `referenceResult` conservan las mismas fuentes.
- `argumentCount`, parameter ordinals, tipos y provenance no cambian.
- No se modificaron parser, Sema, MIR, LLVM, ABI, NSMETA ni formatos.

## Validación diferida

No se compiló ni se ejecutaron pruebas. La validación se acumula para la tanda integral solicitada
por el usuario después de completar toda la implementación pendiente.
