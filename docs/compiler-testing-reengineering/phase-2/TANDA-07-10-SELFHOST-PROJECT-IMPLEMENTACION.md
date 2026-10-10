# Tanda 07-10 — SelfHost.Project — materialización aislada

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente `CSH-PRO-001..003` y `TSH-00579..00600`.
- **Naturaleza:** infraestructura nueva de pruebas; cero cambios funcionales al proyecto self-host.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostProject*`, gate estructural, registro central,
  entrada host, CMake y este Markdown.
- **Archivos protegidos:** producto, tracking, cursor, manifiesto consolidado, batería histórica, `C:` y
  remotos.
- **Acciones prohibidas:** build, pruebas, gates, baseline, recursión, despliegue, mutación del proyecto y PASS
  sintético.
- **Criterio de este corte:** 22/22 IDs materializados con contrato, capa y perfil de asignación exactos;
  infraestructura ausente y contradicción documental explícitas; cero filas ordinarias.

## Denominador

| Dimensión | Conteo |
|---|---:|
| Contratos | 3 |
| Casos totales | 22 |
| Perfil operativo `NIGHTLY` | 22 |
| Capa `build-contract` | 22 |
| `InfrastructurePending` | 22 |
| Ejecutables ordinarios | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Cobertura contractual materializada

- `CSH-PRO-001`, `TSH-00579..00586`: identidad del proyecto y globs frente a las 27 fuentes canónicas.
- `CSH-PRO-002`, `TSH-00587..00593`: cierre exacto y ordenado de las cuatro referencias de proyecto.
- `CSH-PRO-003`, `TSH-00594..00600`: cardinalidad, unicidad y ownership del inventario de fuentes.

## Infraestructura pendiente

Los contratos requieren mutar de forma aislada el XML del proyecto, resolver globs contra inventarios
controlados, construir o retirar productores de dependencias, inyectar duplicados y faltantes, observar el
rechazo de admisión y ejercer límites de escala. El controlador aún no posee esa frontera self-host sin
modificar el proyecto canónico ni depender de artefactos externos. Una inspección textual fija no satisface
los casos inválidos, de determinismo, variantes y escala.

Los 22 casos se registran como `InfrastructurePending`; su cuerpo defensivo falla si se invoca directamente.
No existe evidencia ejecutable para afirmar defectos del producto.

## Contradicción conservada

El plan narrativo asigna 19 casos a `PR` y tres a `NIGHTLY`. La matriz ejecutable
`suite-case-allocation.csv` asigna los 22 casos a `NIGHTLY`. La materialización usa la matriz como autoridad
operativa y conserva la discrepancia abierta en `SelfHostProjectPlanContradictions.jsonl`; no reescribe ninguna
fuente para hacerlas coincidir.

## Artefactos

- `SelfHostProjectTestSupport.h`: IDs, metadata y disposición pendiente.
- `SelfHostProjectIdentityModule.cpp`: ocho casos del contrato de identidad y globs.
- `SelfHostProjectDependenciesModule.cpp`: siete casos del contrato de dependencias.
- `SelfHostProjectInventoryModule.cpp`: siete casos del contrato de inventario.
- `SelfHostProjectPlanContradictions.jsonl`: contradicción de perfiles preservada.
- `manifest/cases.jsonl`: conserva cero filas de `SelfHost.Project` porque todos los casos son pendientes.
- `run_selfhost_project_gate.py`: comprobación futura de denominador, rangos, perfiles y disposición.

Todos los recorridos son iterativos. Complejidad de registro: tiempo `O(22)` y memoria propia constante aparte
de las entradas almacenadas.

## Estado

Los tres registros, módulos y el gate estructural están conectados en `TestRegistry.h`, `main.cpp` y CMake. El
fragmento vacío se retiró y el gate exige cero filas del componente en el manifiesto central. No se tocaron
tracking ni cursor. No se ejecutaron builds, pruebas, gates ni runners. Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 22/22 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.
