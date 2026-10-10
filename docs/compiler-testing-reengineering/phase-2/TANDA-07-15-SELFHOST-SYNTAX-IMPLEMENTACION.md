# Tanda 07-15 — SelfHost.Syntax — materialización aislada

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente `CSH-SYN-001..005` y `TSH-00945..00980`.
- **Naturaleza:** infraestructura nueva de pruebas; cero cambios funcionales al árbol sintáctico self-host.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostSyntax*`, gate estructural, registro central,
  entrada host, CMake y este Markdown.
- **Archivos protegidos:** producto, integración central, tracking, cursor, manifiesto consolidado, batería
  histórica, `C:` y remotos.
- **Acciones prohibidas:** build, pruebas, gates, baseline, recursión, despliegue, copia del árbol sintáctico y
  PASS sintético.
- **Criterio de este corte:** 36/36 IDs materializados con contrato, capa y perfil exactos; infraestructura
  ausente explícita; cero filas ordinarias y validación pendiente.

## Denominador

| Dimensión | Conteo |
|---|---:|
| Contratos | 5 |
| Casos totales | 36 |
| `PR` / `NIGHTLY` | 33 / 3 |
| Unit / component / integration | 36 / 0 / 0 |
| `InfrastructurePending` | 36 |
| Ejecutables ordinarios | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Cobertura contractual materializada

- `CSH-SYN-001..002`, `TSH-00945..00958`: campos de nodos, setters, slots e hijos ordenados.
- `CSH-SYN-003..004`, `TSH-00959..00973`: freeze, hash estructural, arena, IDs y raíz.
- `CSH-SYN-005`, `TSH-00974..00980`: dump determinista y perturbación de construcción.

Cada rango conserva el oráculo publicado por Fase 1 y su capa `unit`. Los tres casos `NIGHTLY` son
`TSH-00958`, `TSH-00965` y `TSH-00973`.

## Infraestructura pendiente

Los contratos requieren ejecutar las clases NativeSharp self-host reales y observar campos, relaciones,
transiciones de freeze, hashes, arena, raíz, igualdad y dumps exactos. El controlador general no ofrece todavía
esa frontera. Reimplementar `SyntaxNode` o `SyntaxTree` dentro del harness C++ probaría un sustituto.

Los 36 casos se registran como `InfrastructurePending`; su cuerpo defensivo falla si se invoca directamente.
No existe evidencia ejecutable para afirmar defectos del producto.

## Artefactos

- `SelfHostSyntaxTestSupport.h`: IDs exactos, perfiles, contratos y disposición pendiente.
- `SelfHostSyntaxNodesModule.cpp`: 14 casos, dos contratos.
- `SelfHostSyntaxTreeModule.cpp`: 15 casos, dos contratos.
- `SelfHostSyntaxDumpModule.cpp`: 7 casos, un contrato.
- `manifest/cases.jsonl`: conserva cero filas de `SelfHost.Syntax` porque todos los casos son pendientes.
- `run_selfhost_syntax_gate.py`: comprobación futura del denominador, rangos, capas, perfiles y disposición.

Todos los recorridos son iterativos. Complejidad de registro: tiempo `O(36)` y memoria propia constante aparte
de las entradas almacenadas.

## Estado

Los tres registros, módulos y el gate estructural están conectados en `TestRegistry.h`, `main.cpp` y CMake. El
fragmento vacío se retiró y el gate exige cero filas del componente en el manifiesto central. No se tocaron
tracking ni cursor. No se ejecutaron builds, pruebas, gates ni runners. Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 36/36 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.
