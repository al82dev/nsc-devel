# Validacion 134 - inventario pendiente observable

## Alcance

Esta tanda hace observable el metadato de casos `InfrastructurePending` que ya
existía fuera de `TestRegistry`. No registra funciones ejecutables, no cambia
disposiciones y no presenta ningún caso pendiente como PASS, FAIL o SKIP.

## Causa

El gate global mezclaba el registro ejecutable con cuatro colecciones
deliberadamente no ejecutables. Antes de corregir el gate era necesario poder
cotejar ambas superficies sin inferir rangos privados ni eliminar casos.

## Implementacion

- `TestInventoryRow` expresa solamente ID, contrato y componente.
- `infrastructurePendingInventory()` agrega las colecciones vivas de AST,
  Interop, Linker y Runtime, valida metadato, ordena por ID y rechaza
  duplicados.
- `--list-pending` publica esa colección con el mismo formato tabulado de
  `--list`.
- `--list`, `--all`, `--case` y `--component` conservan su conducta.

## Evidencia

La reconstrucción del target terminó con salida cero. Las dos superficies
vivas producen:

| Superficie | Casos |
|---|---:|
| Registro ordinario | 8,054 |
| InfrastructurePending externo al registro | 841 |
| Intersección | 0 |

Desglose pendiente:

| Componente | Casos |
|---|---:|
| Host.AST | 6 |
| Host.Interop | 224 |
| Host.Linker | 307 |
| Host.Runtime | 304 |
| **Total** | **841** |

Las 841 filas existen en el inventario canónico y conservan exactamente su
contrato y componente: `changed=0`, `duplicates=0`, `overlap=0`. La primera es
`TA-AST-014`; la última, `TF-RUNTIME-384`.

El gate global todavía no se presenta como cerrado: la siguiente tanda debe
validar la unión del registro ordinario, estos pendientes y los productores
externos del controlador. No se ejecutó baseline, no se desplegó y no se tocó
`C:`.
