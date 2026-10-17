# NSC 3.0.5 — H02S12A: receivers de campos struct iterativos

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada HIR de campos explícitos cuyo receiver es struct. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Incluido | Dirección natural y materialización `TemporaryAddress` del receiver. |
| Excluido | Getters, core, extensiones, índices, calls, campos implícitos/this/base. |
| Contratos | Orden, address context, tipo, ordinal, volatile, base receiver y provenance. |
| Validación | Diferida a la tanda integral por instrucción expresa del usuario. |

## Cambio

El frame `InstanceField` admite ahora receivers de clase y struct. Para un struct naturalmente
direccionable desciende con contexto de dirección. Para un temporal baja primero el valor y, al
desenrollar, reconstruye el mismo `TemporaryAddress` que producía `lowerReceiver` antes de crear el
campo exterior.

La clasificación conserva la ruta anterior cuando falta binding, receiver, tipo u ordinal, y sigue
excluyendo toda precedencia especial. Por tanto permanecen intactos los diagnósticos previos y el
primer rango de error.

## Auditoría estática

- El receiver se evalúa antes de crear el campo.
- Sólo el campo exterior hereda el `addressContext` solicitado por su consumidor.
- Un receiver struct con almacenamiento natural se baja como dirección.
- Un receiver struct temporal se baja como valor y se materializa exactamente una vez.
- `typeKey`, `memberOrdinal`, `volatileAccess`, `baseReceiver` y provenance usan las mismas fuentes
  que la ruta anterior.
- No se modificaron parser, Sema, MIR, LLVM, ABI, NSMETA ni formatos.

## Validación diferida

No se compiló ni se ejecutaron pruebas en este corte. La validación se acumula para la tanda
integral solicitada por el usuario después de completar toda la implementación pendiente.
