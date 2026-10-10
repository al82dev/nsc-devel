# Tanda 07-09 — SelfHost.Parser — materialización aislada

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente `CSH-PAR-001..016` y `TSH-00452..00578`.
- **Naturaleza:** infraestructura nueva de pruebas; cero cambios funcionales al parser self-host.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostParser*`, gate estructural, registro central,
  entrada host, CMake y este Markdown.
- **Archivos protegidos:** producto, tracking, cursor, manifiesto consolidado, batería histórica, `C:` y
  remotos.
- **Acciones prohibidas:** build, pruebas, gates, baseline, recursión, despliegue, copia del parser y PASS
  sintético.
- **Criterio de este corte:** 127/127 IDs materializados con contrato, capa y perfil exactos; infraestructura
  ausente explícita; cero filas ordinarias y validación pendiente.

## Denominador

| Dimensión | Conteo |
|---|---:|
| Contratos | 16 |
| Casos totales | 127 |
| `PR` / `NIGHTLY` | 107 / 20 |
| Unit / component / integration | 21 / 97 / 9 |
| `InfrastructurePending` | 127 |
| Ejecutables ordinarios | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Cobertura contractual materializada

- `CSH-PAR-001..004`, `TSH-00452..00479`: cursor, registros de nodos, tipos, literales y diagnósticos.
- `CSH-PAR-005..008`, `TSH-00480..00512`: expresiones primarias, postfix, unary y binary con precedencia.
- `CSH-PAR-009..012`, `TSH-00513..00545`: sentencias, control de flujo, secciones, etiquetas y recuperación.
- `CSH-PAR-013..016`, `TSH-00546..00578`: declaraciones, miembros, tipos y unidad de compilación completa.

Cada rango conserva el oráculo publicado por Fase 1 y su capa exacta. Los 20 casos `NIGHTLY` son `00472`,
`00479`, `00486`, `00494`, `00495`, `00502`, `00503`, `00511`, `00512`, `00519`, `00527`, `00535`,
`00536`, `00544`, `00545`, `00553`, `00561`, `00569`, `00577` y `00578`.

## Infraestructura pendiente

Los contratos requieren fuente self-host real, lexer y token stream controlables, arena y ownership del AST,
captura estructurada de diagnósticos, snapshots exactos del cursor, entradas malformadas y comparación de
árboles, rangos, operadores, metadatos y consumo de EOF. El controlador general aún no ofrece un ejecutor
`SelfHost.Parser` dueño de esas fronteras. Reimplementar el parser o sus oráculos dentro del harness C++
probaría un sustituto y no el producto.

Los 127 casos se registran como `InfrastructurePending`; su cuerpo defensivo falla si se invoca directamente.
No existe evidencia ejecutable para afirmar defectos del producto.

## Artefactos

- `SelfHostParserTestSupport.h`: IDs exactos, perfiles, contratos y disposición pendiente.
- `SelfHostParserTypesLiteralsModule.cpp`: 28 casos, cuatro contratos.
- `SelfHostParserExpressionsModule.cpp`: 33 casos, cuatro contratos.
- `SelfHostParserStatementsModule.cpp`: 33 casos, cuatro contratos.
- `SelfHostParserDeclarationsModule.cpp`: 33 casos, cuatro contratos.
- `manifest/cases.jsonl`: conserva cero filas de `SelfHost.Parser` porque todos los casos son pendientes.
- `run_selfhost_parser_gate.py`: comprobación futura del denominador, rangos, capas, perfiles y disposición.

Todos los recorridos son iterativos. Complejidad de registro: tiempo `O(127)` y memoria propia constante aparte
de las entradas almacenadas.

## Estado

Los cuatro registros, módulos y el gate estructural están conectados en `TestRegistry.h`, `main.cpp` y CMake.
El fragmento vacío se retiró y el gate exige cero filas del componente en el manifiesto central. No se tocaron
tracking ni cursor. No se ejecutaron builds, pruebas, gates ni runners. Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 127/127 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.
