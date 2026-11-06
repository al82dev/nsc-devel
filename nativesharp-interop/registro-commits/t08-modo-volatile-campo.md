# T08-015: modo volatile en el símbolo importado

El proveedor conserva el modo de la observación seleccionada solo cuando
todas las observaciones del grupo coinciden. El dato llega al campo importado
y al símbolo de NSC; si discrepan o falta información permanece desconocido.
Esto aún no modifica HIR, MIR ni las operaciones emitidas; falta validación.
