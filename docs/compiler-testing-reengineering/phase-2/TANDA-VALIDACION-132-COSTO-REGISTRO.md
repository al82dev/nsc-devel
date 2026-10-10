# Tanda Validacion 132 - costo de construccion del registro

## Alcance

Esta tanda corrige exclusivamente el costo de mantener ordenado el registro de
casos. No cambia el inventario, la identidad, el metadato, el cuerpo ni la
disposicion de ningun caso.

## Evidencia de entrada

Despues de la Validacion 131, `nsc_component_tests --list` materializaba
correctamente `8,054` filas, pero tardaba mas de 60 segundos. El gate
`nsc.compiler-suite.registry-inventory` terminaba por
`subprocess.TimeoutExpired` antes de comparar el inventario.

`TestRegistry::add` ejecutaba esta secuencia para cada alta:

1. busqueda lineal de duplicados;
2. insercion al final;
3. `std::sort` de todo el vector acumulado.

El tercer paso repetia el ordenamiento completo miles de veces.

## Auditoria de consumidores

Las unicas lecturas publicas del registro dentro de la suite son:

- `cases()`, que exige orden estable por ID;
- `find()`, que exige busqueda exacta;
- `all()`, que filtra casos requeridos en orden estable;
- `byComponent()`, que filtra un componente en orden estable;
- `run()`, que conserva el orden de la seleccion recibida.

La prueba `INFRA-REGISTRY-001` comprueba ademas orden, busqueda, duplicados y
filtros. No existe un contrato que requiera reordenar todo el vector tras cada
alta; el contrato exige que el estado observable permanezca ordenado.

## Correccion

`TestRegistry::add` usa `std::lower_bound` sobre el vector ya ordenado para:

1. localizar en tiempo logaritmico la posicion de la nueva identidad;
2. detectar un duplicado en esa misma posicion;
3. insertar una sola vez sin ejecutar `std::sort` global.

La solucion se limita a `TestRegistry.cpp`; no cambia el encabezado ni la API.

## Validacion ejecutada

| Gate | Resultado |
|---|---:|
| Reconstruccion del target | `314/314`, PASS |
| `INFRA-REGISTRY-001` | PASS |
| `nsc_component_tests --list` | salida `0` |
| Filas antes y despues | `8,054` |
| Tiempo medido de `--list` | `9,631 ms` |
| Limite del controlador | `60,000 ms` |
| Gate de inventario | alcanza comparacion en `9.47 s`, sin timeout |

El gate de inventario ya no esta bloqueado por rendimiento. Su comparacion
descubrio un defecto independiente de membresia entre el registro y los
inventarios canónicos: existen IDs faltantes, IDs inesperados y cero filas con
metadato cambiado. Esa diferencia pertenece a la Validacion 133.

## Estado

La construccion del registro queda dentro del limite y conserva sus contratos
observables. La tarea persistente continua activa; no se afirma que el gate de
inventario haya pasado. No se ejecuto baseline, no se desplego y no se escribio
en `C:`.
