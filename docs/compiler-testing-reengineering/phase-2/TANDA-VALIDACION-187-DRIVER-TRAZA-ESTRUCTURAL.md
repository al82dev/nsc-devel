# Tanda de validación 187 — Traza vacía por estructura

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE BUILD Y PRUEBA DIRIGIDA PENDIENTE.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El productor omite correctamente `timing phase=bad us=x`. El oráculo buscaba
el fragmento textual `"phases": []`, pero el renderer indenta el cierre del
arreglo vacío. El whitespace no forma parte del contrato semántico.

## Corrección causal

El oráculo comprueba que `exit_code` conserva 7, `total_ms` permanece cero y
no existe ningún miembro `name`, sin exigir una representación concreta del
espacio entre corchetes. Se sincronizaron plan, inventario y manifiesto.

No se compiló ni ejecutó por instrucción del usuario; la validación será única
al final de todas las correcciones.
