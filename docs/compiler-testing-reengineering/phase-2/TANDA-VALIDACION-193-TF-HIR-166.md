# Tanda de validación 193 — TF-HIR-166

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente el fixture de `TF-HIR-166`, perteneciente a
`CF-HIR-021`. No modifica el dumper HIR, el compilador, el plan ni el
manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El fixture construía el valor `a\\\"\n\r\t\0b` mediante la conversión implícita
de un literal C a `std::string`. Esa conversión terminaba en el NUL embebido y
eliminaba el byte NUL y la `b` posterior antes de que el productor recibiera el
valor. El oráculo exigía correctamente que ambos bytes aparecieran escapados.

## Corrección

El fixture construye ahora el `std::string` con longitud explícita de ocho
bytes. El contrato, el plan y el manifiesto ya describían el comportamiento
correcto y permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
