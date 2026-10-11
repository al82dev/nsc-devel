# Tanda de validación 195 — TF-HIR-207 y TF-HIR-208

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente los fixtures `TF-HIR-207` y `TF-HIR-208`,
pertenecientes a `CF-HIR-026`. No modifica el verificador HIR, el compilador,
el plan ni el manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

Los dos casos declaraban escenarios válidos de determinismo y escala, pero el
fixture común construía un `Store` cuyo destino era un `Constant` de tipo
entero. El productor rechazaba correctamente ese destino porque `Store`
requiere una dirección de tipo puntero.

## Corrección

`TF-HIR-207` usa ahora una dirección válida y verifica dos veces el mismo grafo
para conservar el contrato de repetición determinista. `TF-HIR-208` construye
un grafo ancho de 256 stores válidos y verifica su terminación y aceptación.
El plan, el inventario y el manifiesto ya describían determinismo y escala, por
lo que permanecen sin cambios.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
