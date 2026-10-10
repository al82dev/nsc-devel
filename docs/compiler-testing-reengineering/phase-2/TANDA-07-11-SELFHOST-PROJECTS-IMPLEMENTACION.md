# Tanda 07-11 — SelfHost.Projects — materialización aislada

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente `CSH-PRJ-001..012` y `TSH-00601..00695`.
- **Naturaleza:** infraestructura nueva de pruebas; cero cambios funcionales al sistema de proyectos self-host.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostProjects*`, gate estructural, registro central,
  entrada host, CMake y este Markdown.
- **Archivos protegidos:** producto, tracking, cursor, manifiesto consolidado, batería histórica, `C:` y
  remotos.
- **Acciones prohibidas:** build, pruebas, gates, baseline, recursión, despliegue, copia del sistema de proyectos
  y PASS sintético.
- **Criterio de este corte:** 95/95 IDs materializados con contrato, capa y perfil exactos; infraestructura
  ausente explícita; cero filas ordinarias y validación pendiente.

## Denominador

| Dimensión | Conteo |
|---|---:|
| Contratos | 12 |
| Casos totales | 95 |
| `PR` / `NIGHTLY` | 82 / 13 |
| Unit / component / integration | 38 / 32 / 25 |
| `InfrastructurePending` | 95 |
| Ejecutables ordinarios | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Cobertura contractual materializada

- `CSH-PRJ-001..003`, `TSH-00601..00622`: modelo, extracción XML, verificación y primera fuente.
- `CSH-PRJ-004..006`, `TSH-00623..00645`: dump determinista, compatibilidad NSMETA e identidad de caché.
- `CSH-PRJ-007..009`, `TSH-00646..00669`: manifiesto de toolchain, plan de productos y unidades del grafo.
- `CSH-PRJ-010..012`, `TSH-00670..00695`: expansión de includes, traversal de dependencias y dump del grafo.

Cada rango conserva el oráculo publicado por Fase 1 y su capa exacta. Los 13 casos `NIGHTLY` son `00614`,
`00621`, `00622`, `00630`, `00638`, `00653`, `00661`, `00669`, `00677`, `00678`, `00686`, `00687` y
`00695`.

## Infraestructura pendiente

Los contratos requieren ejecutar las clases NativeSharp reales con archivos XML y árboles de directorio
aislados, metadata compatible e incompatible, identidad de toolchain, productores de artefactos, grafos de
dependencias, ciclos, duplicados, fallos parciales y límites de escala. El controlador general aún no ofrece
esa frontera self-host ni ownership del filesystem y de los artefactos producidos. Reimplementar el sistema de
proyectos dentro del harness C++ probaría un sustituto.

Los 95 casos se registran como `InfrastructurePending`; su cuerpo defensivo falla si se invoca directamente.
No existe evidencia ejecutable para afirmar defectos del producto.

## Artefactos

- `SelfHostProjectsTestSupport.h`: IDs exactos, perfiles, contratos y disposición pendiente.
- `SelfHostProjectsModelModule.cpp`: 22 casos, tres contratos.
- `SelfHostProjectsIdentityModule.cpp`: 23 casos, tres contratos.
- `SelfHostProjectsArtifactsModule.cpp`: 24 casos, tres contratos.
- `SelfHostProjectsGraphModule.cpp`: 26 casos, tres contratos.
- `manifest/cases.jsonl`: conserva cero filas de `SelfHost.Projects` porque todos los casos son pendientes.
- `run_selfhost_projects_gate.py`: comprobación futura del denominador, rangos, capas, perfiles y disposición.

Todos los recorridos son iterativos. Complejidad de registro: tiempo `O(95)` y memoria propia constante aparte
de las entradas almacenadas.

## Estado

Los cuatro registros, módulos y el gate estructural están conectados en `TestRegistry.h`, `main.cpp` y CMake.
El fragmento vacío se retiró y el gate exige cero filas del componente en el manifiesto central. No se tocaron
tracking ni cursor después del corte persistente anterior. No se ejecutaron builds, pruebas, gates ni runners.
Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 95/95 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.
