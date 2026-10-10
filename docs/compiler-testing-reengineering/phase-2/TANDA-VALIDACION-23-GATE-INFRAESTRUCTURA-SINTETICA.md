# Tanda de Validación 23: gate de infraestructura sintética

Fecha: 10 de octubre de 2026
Estado: CERRADA CON EVIDENCIA DIRIGIDA

## Problema confirmado

El gate comprobaba literalmente tres resultados `PASS` y ocho eventos. Ambos
números eran consecuencias del fixture actual y obligaban a editar el gate al
agregar o retirar un caso válido, aunque el protocolo siguiera correcto.

## Cierre

El gate ahora carga `components.csv`, `contracts.csv` y `cases.jsonl` mediante
`InventoryIndex`, `load_manifest` y `select_cases`. A partir de esa selección:

- exige exactamente las mismas identidades en `results.json`;
- deriva las cantidades de estados desde `expected_outcome`;
- exige secuencias contiguas de eventos;
- exige `RUN_STARTED` y `RUN_FINISHED` en las fronteras;
- exige un `CASE_STARTED` y un `CASE_FINISHED` por cada caso vivo;
- coteja `selected`, `completed` y `cancelled` con la corrida materializada.

## Evidencia

- `test_infrastructure_gate.py`: `2/2` PASS.
- La prueba positiva genera resultados y eventos desde el fixture vivo.
- La prueba negativa demuestra rechazo de un resultado o evento faltante.
- `py_compile` del gate y de su prueba: PASS.

No se modificaron fixtures, casos, oráculos, resultados esperados, producto ni
batería histórica. No se configuró, compiló ni ejecutó producto; no hubo
baseline ni despliegue.
