# Tanda de validación 159 — imports de gates aislados

## Evidencia dirigida

La ejecución aislada de
`test_nscbuilder3_fixture_failures` encontró tres errores con una sola causa:
`selfhost_inventory.py` pudo importar su módulo hermano, pero
`component_gate_support.py` no pudo importar el paquete público `nsc_suite`.

## Corrección

`ExecutionEngine._load_gate()` publica una sola vez las dos raíces que forman
el contrato de importación de los gates:

- `tests/compiler-suite/controller` para `nsc_suite`;
- `tests/compiler-suite/controller/integration` para módulos hermanos.

La prueba del cargador comprueba ambas rutas y conserva la identidad cacheada
del módulo. No se modifica código de producto ni se cambia ningún oráculo.

## Revalidación

Pendiente después del commit; se repetirá únicamente la prueba dirigida del
constructor de fixtures.
