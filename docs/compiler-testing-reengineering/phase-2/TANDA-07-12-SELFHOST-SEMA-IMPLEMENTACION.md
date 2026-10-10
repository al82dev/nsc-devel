# Tanda 07-12 — SelfHost.Sema — materialización aislada

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente `CSH-SEM-001..020` y `TSH-00696..00848`.
- **Naturaleza:** infraestructura nueva de pruebas; cero cambios funcionales al análisis semántico self-host.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostSema*`, gate estructural, registro central,
  entrada host, CMake y este Markdown.
- **Archivos protegidos:** producto, integración central, tracking, cursor, manifiesto consolidado, batería
  histórica, `C:` y remotos.
- **Acciones prohibidas:** build, pruebas, gates, baseline, recursión, despliegue, copia del analizador semántico
  y PASS sintético.
- **Criterio de este corte:** 153/153 IDs materializados con contrato, capa y perfil exactos; infraestructura
  ausente explícita; cero filas ordinarias y validación pendiente.

## Denominador

| Dimensión | Conteo |
|---|---:|
| Contratos | 20 |
| Casos totales | 153 |
| `PR` / `NIGHTLY` | 130 / 23 |
| Unit / component / integration | 42 / 88 / 23 |
| `InfrastructurePending` | 153 |
| Ejecutables ordinarios | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Cobertura contractual materializada

- `CSH-SEM-001..005`, `TSH-00696..00736`: modelo semántico y grafos de callable, excepciones y genéricos.
- `CSH-SEM-006..010`, `TSH-00737..00777`: índice, unidades, metadata externa, construcción y dumps de módulos.
- `CSH-SEM-011..015`, `TSH-00778..00812`: scopes, tipos, overloads, declaraciones y conversiones numéricas.
- `CSH-SEM-016..020`, `TSH-00813..00848`: expresiones, cuerpos, escapes, modos de análisis y dumps semánticos.

Cada rango conserva el oráculo publicado por Fase 1 y su capa exacta. Los 23 casos `NIGHTLY` son `00709`,
`00710`, `00718`, `00719`, `00727`, `00728`, `00736`, `00744`, `00745`, `00752`, `00753`, `00760`,
`00768`, `00769`, `00777`, `00791`, `00799`, `00806`, `00819`, `00826`, `00833`, `00840` y `00848`.

## Infraestructura pendiente

Los contratos requieren ejecutar las clases NativeSharp self-host reales con árboles sintácticos, tipos,
símbolos, scopes, grafos, metadata externa y diagnósticos bajo ownership del controlador. El controlador general
no ofrece todavía esa frontera ni observación estable de los modelos producidos. Reimplementar el analizador
semántico dentro del harness C++ probaría un sustituto.

Los 153 casos se registran como `InfrastructurePending`; su cuerpo defensivo falla si se invoca directamente.
No existe evidencia ejecutable para afirmar defectos del producto.

## Artefactos

- `SelfHostSemaTestSupport.h`: IDs exactos, perfiles, contratos y disposición pendiente.
- `SelfHostSemaModelModule.cpp`: 41 casos, cinco contratos.
- `SelfHostSemaModuleSetModule.cpp`: 41 casos, cinco contratos.
- `SelfHostSemaResolutionModule.cpp`: 35 casos, cinco contratos.
- `SelfHostSemaAnalysisModule.cpp`: 36 casos, cinco contratos.
- `manifest/cases.jsonl`: conserva cero filas de `SelfHost.Sema` porque todos los casos son pendientes.
- `run_selfhost_sema_gate.py`: comprobación futura del denominador, rangos, capas, perfiles y disposición.

Todos los recorridos son iterativos. Complejidad de registro: tiempo `O(153)` y memoria propia constante aparte
de las entradas almacenadas.

## Estado

Los cuatro registros, módulos y el gate estructural están conectados en `TestRegistry.h`, `main.cpp` y CMake.
El fragmento vacío se retiró y el gate exige cero filas del componente en el manifiesto central. No se
ejecutaron builds, pruebas, gates ni runners. Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 153/153 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.
