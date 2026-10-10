# Tanda de Validacion 106: claves JSON unicas en salida diagnostics

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

El oraculo JSON de `SelfHost.Diagnostics` analizaba stdout mediante last-wins.
Una salida contradictoria como `{"version":1,"version":2}` podia compararse
como si el fixture hubiera producido unicamente `{"version":2}` y acreditar un
PASS falso.

## Correccion

- `selfhost_inventory.parse_json_value` exige claves unicas en objetos JSON;
- el evaluador Diagnostics clasifica la violacion como salida JSON invalida;
- la comparacion posterior conserva tipos y valores exactos sin recursion;
- compilacion, ejecucion y oraculos declarados permanecen sin cambios.

## Evidencia ejecutada

- pruebas dirigidas del contrato self-host: `6/6` PASS;
- suite Python completa del controlador: `240/240` PASS;
- dos definiciones de `version` ya no satisfacen el valor esperado `2`;
- el inventario de `json.loads` en integracion solo conserva lectores con
  `object_pairs_hook` estricto;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves en la salida JSON evaluada por
`SelfHost.Diagnostics`. No acredita una corrida funcional del fixture ni del
producto. La meta permanece activa hasta cero errores inesperados en la matriz
acordada.
