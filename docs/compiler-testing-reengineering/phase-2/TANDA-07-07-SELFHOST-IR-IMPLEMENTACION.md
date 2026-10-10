# Tanda 07-07 — SelfHost.IR — materialización e integración

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente `CSH-IR-001..012` y `TSH-00307..00398`.
- **Naturaleza:** infraestructura nueva de pruebas; cero cambios funcionales al IR self-host.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostIr*`, fragmento vacío, gate estructural, registro
  central, entrada host, CMake y este Markdown.
- **Archivos protegidos:** producto, tracking, cursor, manifiesto consolidado, batería histórica, `C:` y remotos.
- **Acciones prohibidas:** build, pruebas, gates, baseline, recursión, despliegue, copia de algoritmos del IR y
  PASS sintético.
- **Criterio de este corte:** 92/92 IDs materializados con contrato, capa y perfil exactos; infraestructura
  ausente explícita; cero filas ordinarias y validación pendiente.

## Denominador

| Dimensión | Conteo |
|---|---:|
| Contratos | 12 |
| Casos totales | 92 |
| `PR` / `NIGHTLY` | 79 / 13 |
| Unit / component / integration | 44 / 41 / 7 |
| `InfrastructurePending` | 92 |
| Ejecutables ordinarios | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Cobertura contractual materializada

- `CSH-IR-001..004`, `TSH-00307..00337`: registros HIR, construcción, verificación y modelo de flujo.
- `CSH-IR-005..008`, `TSH-00338..00368`: lowering MIR, CFG, cleanups de ownership y verificación.
- `CSH-IR-009..012`, `TSH-00369..00398`: dominancia, dumps estructurales, pipeline y admisión IR.

Cada rango conserva el oráculo publicado por Fase 1 y su capa exacta. Los 13 casos `NIGHTLY` son
`00314`, `00322`, `00323`, `00330`, `00337`, `00345`, `00346`, `00353`, `00361`, `00368`, `00376`,
`00391` y `00398`.

## Infraestructura pendiente

Los contratos requieren construir modelos semánticos reales, controlar ownership/lifetime, inyectar fallos,
comparar recorridos independientes de HIR/MIR/CFG/dominancia y observar dumps, pases y estados de admisión. El
controlador general aún no ofrece una operación self-host IR con esas fronteras. Duplicar los verificadores o
reconstruir sus algoritmos dentro de C++ probaría un sustituto.

Los 92 casos se registran como `InfrastructurePending`; su cuerpo defensivo falla si se invoca directamente.
No existe evidencia ejecutable para afirmar defectos del producto.

## Artefactos

- `SelfHostIrTestSupport.h`: IDs exactos, perfiles y disposición.
- `SelfHostIrHirFlowModule.cpp`: 31 casos, cuatro contratos.
- `SelfHostIrMirVerificationModule.cpp`: 31 casos, cuatro contratos.
- `SelfHostIrDominanceLifecycleModule.cpp`: 30 casos, cuatro contratos.
- `manifest/cases.jsonl`: conserva cero filas de `SelfHost.IR` porque todos los casos son pendientes.
- `run_selfhost_ir_gate.py`: comprobación futura del denominador, rangos, capas, perfiles y disposición.

Todos los recorridos son iterativos. Complejidad de registro: tiempo `O(92)` y memoria propia constante aparte
de las entradas almacenadas.

## Estado

Los tres registros, módulos y el gate estructural están conectados en `TestRegistry.h`, `main.cpp` y CMake.
El fragmento vacío se retiró y el gate exige cero filas del componente en el manifiesto central. No se tocaron
tracking ni cursor. No se ejecutaron builds, pruebas, gates ni runners. Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 92/92 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.
