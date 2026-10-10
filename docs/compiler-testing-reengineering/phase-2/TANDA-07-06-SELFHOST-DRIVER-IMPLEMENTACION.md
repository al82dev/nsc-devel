# Tanda 07-06 — SelfHost.Driver — materialización e integración

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar únicamente `CSH-DRV-001..010` y `TSH-00228..00306`.
- **Naturaleza:** infraestructura nueva de pruebas; cero cambios en Driver o en el compilador.
- **Fuentes normativas:** plan e inventarios canónicos de Fase 1 y el Driver self-host vigente.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostDriver*`, fragmento vacío, gate estructural,
  registro central, entrada host, CMake y este Markdown.
- **Archivos protegidos:** producto, `SelfHost.Diagnostics`, tracking, cursor, manifiesto consolidado, batería
  histórica, `C:` y remotos.
- **Acciones prohibidas:** build, pruebas, gates, baseline, recursión, despliegue, sustitutos del Driver y PASS
  sintético.
- **Criterio de este corte:** 79/79 IDs materializados con contrato, capa y perfil exactos; infraestructura
  ausente visible; cero filas ordinarias y validación pendiente.

## Denominador

| Dimensión | Conteo |
|---|---:|
| Contratos | 10 |
| Casos totales | 79 |
| Capa `process` | 79 |
| `PR` / `NIGHTLY` | 64 / 15 |
| `InfrastructurePending` | 79 |
| Ejecutables ordinarios | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Trazabilidad contractual

| Contrato | Casos | Oráculo |
|---|---|---|
| `CSH-DRV-001` | `TSH-00228..00235` | exit code, streams and semantic fields |
| `CSH-DRV-002` | `TSH-00236..00244` | normalized dump text and exit code |
| `CSH-DRV-003` | `TSH-00245..00252` | selected dump exact structure and exit code |
| `CSH-DRV-004` | `TSH-00253..00259` | check identities and final exit code |
| `CSH-DRV-005` | `TSH-00260..00267` | stage order, artifact bytes and exit code |
| `CSH-DRV-006` | `TSH-00268..00275` | unit order, diagnostics, artifacts and exit code |
| `CSH-DRV-007` | `TSH-00276..00283` | verified stage sequence, artifact and exit result |
| `CSH-DRV-008` | `TSH-00284..00292` | report records, order, identities, counts and exit code |
| `CSH-DRV-009` | `TSH-00293..00299` | artifact identities, reasons and final status |
| `CSH-DRV-010` | `TSH-00300..00306` | handler, arguments, recursion boundary and exit code |

## Infraestructura pendiente

Los diez contratos pertenecen a una frontera de proceso. Requieren un ejecutor del compilador self-host que
posea `argv`, cwd, entorno, streams, fuentes/proyectos temporales, toolchain, artefactos y fallos inyectados sin
publicación parcial. El controlador actual no ofrece esa operación general; copiar handlers o simular sus
salidas probaría un sustituto.

Los 79 casos permanecen `InfrastructurePending`. El cuerpo defensivo de cada caso falla si se invoca fuera de
la selección ordinaria. No hay evidencia ejecutable que autorice clasificarlos como defectos del producto.

## Artefactos

- `SelfHostDriverTestSupport.h`: genera IDs exactos, asigna los 15 perfiles `NIGHTLY` y registra pendientes.
- `SelfHostDriverCliFrontendModule.cpp`: 32 casos de identidad, dumps, IR y self-test.
- `SelfHostDriverEmissionModule.cpp`: 24 casos de emisión simple, proyecto y bootstrap.
- `SelfHostDriverAuditDispatchModule.cpp`: 23 casos de auditoría, dependencias y dispatch.
- `manifest/cases.jsonl`: conserva cero filas de `SelfHost.Driver` porque todos los casos son pendientes.
- `run_selfhost_driver_gate.py`: comprobación futura de denominador, rangos, perfiles y disposición.

La generación de IDs y los recorridos son iterativos; no existe recursión. Complejidad de registro: tiempo
`O(79)` y memoria propia constante aparte de las entradas registradas.

## Estado

Los tres registros, sus módulos y el gate estructural están conectados en `TestRegistry.h`, `main.cpp` y CMake.
El fragmento vacío se retiró y el gate exige que el manifiesto central conserve cero filas del componente.
No se tocaron tracking ni cursor. No se ejecutaron builds, pruebas, gates ni runners. Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 79/79 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.
