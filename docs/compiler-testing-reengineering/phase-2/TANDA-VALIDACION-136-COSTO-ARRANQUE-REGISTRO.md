# Tanda Validacion 136 - Costo de arranque del registro

Fecha: 2026-10-10  
Estado: VERIFICADA

## Alcance causal

El barrido Debug de la suite nueva alcanzo 83 de 116 gates. Dos productores
pasaron y los controladores restantes expiraron antes de ejecutar sus casos:
construir el registro de 8,054 pruebas consumia entre 9 y 10 segundos, mientras
los casos de clase `tiny` disponen de 5 segundos.

La causa permanecia en `TestRegistry::add`. La Validacion 132 retiro el
ordenamiento completo por alta, pero `vector::insert(lower_bound)` continuaba
desplazando un sufijo del vector en cada insercion. El costo total seguia siendo
cuadratico.

## Correccion minima

- `TestRegistry::add` conserva un indice hash de identidades para rechazar
  duplicados en el punto de alta.
- Cada caso se anexa al vector sin desplazar los casos anteriores.
- El orden lexicografico observable se materializa una sola vez, de forma
  perezosa, antes de `cases`, `find`, `all` o `byComponent`.
- `find` conserva busqueda exacta sobre el vector ya ordenado.

No se modificaron controladores, timeouts, casos, disposiciones, CMake, producto,
bateria historica ni los seis archivos protegidos.

## Evidencia reproducible

| Comprobacion | Resultado |
|---|---:|
| Reconstruccion `nsc_component_tests` Debug | 315/315, salida 0 |
| Filas ordinarias conservadas | 8,054 |
| Arranque `--list` antes | 9,631 ms |
| Arranque `--list` despues | 645.626 ms |
| Mejora observada | 14.92 veces |
| `INFRA-REGISTRY-001` | PASS |
| `host-source-position-controller` | 1/1 PASS, 1.05 s |

La prueba dirigida demuestra el recorrido completo por el controlador y descarta
el timeout de arranque como cierre derivado. El siguiente paso es ejecutar los
116 gates Debug y clasificar solamente los defectos que sobrevivan con el
productor recuperado.

