# Tanda 07-14 — SelfHost.Symbols — materialización aislada

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente `CSH-SYM-001..005` y `TSH-00910..00944`.
- **Naturaleza:** infraestructura nueva de pruebas; cero cambios funcionales al modelo de símbolos self-host.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostSymbols*`, gate estructural, registro central,
  entrada host, CMake y este Markdown.
- **Archivos protegidos:** producto, integración central, tracking, cursor, manifiesto consolidado, batería
  histórica, `C:` y remotos.
- **Acciones prohibidas:** build, pruebas, gates, baseline, recursión, despliegue, copia del modelo de símbolos y
  PASS sintético.
- **Criterio de este corte:** 35/35 IDs materializados con contrato, capa y perfil exactos; infraestructura
  ausente explícita; cero filas ordinarias y validación pendiente.

## Denominador

| Dimensión | Conteo |
|---|---:|
| Contratos | 5 |
| Casos totales | 35 |
| `PR` / `NIGHTLY` | 34 / 1 |
| Unit / component / integration | 35 / 0 / 0 |
| `InfrastructurePending` | 35 |
| Ejecutables ordinarios | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Cobertura contractual materializada

- `CSH-SYM-001..002`, `TSH-00910..00923`: vocabulario, identidad y metadata de declaración.
- `CSH-SYM-003..004`, `TSH-00924..00937`: miembros, layout, constantes y reachability.
- `CSH-SYM-005`, `TSH-00938..00944`: declaración, enumeración y lookup de scopes léxicos.

Cada rango conserva el oráculo publicado por Fase 1 y su capa `unit`. El único caso `NIGHTLY` es
`TSH-00937`.

## Infraestructura pendiente

Los contratos requieren ejecutar las clases NativeSharp self-host reales y observar enums, campos mutables,
tablas de miembros, layout, constantes, flags de reachability, scopes anidados, orden y lookup. El controlador
general no ofrece todavía esa frontera. Reimplementar `Symbol` o `Scope` dentro del harness C++ probaría un
sustituto.

Los 35 casos se registran como `InfrastructurePending`; su cuerpo defensivo falla si se invoca directamente.
No existe evidencia ejecutable para afirmar defectos del producto.

## Artefactos

- `SelfHostSymbolsTestSupport.h`: IDs exactos, perfiles, contratos y disposición pendiente.
- `SelfHostSymbolsIdentityModule.cpp`: 14 casos, dos contratos.
- `SelfHostSymbolsMetadataModule.cpp`: 14 casos, dos contratos.
- `SelfHostSymbolsScopeModule.cpp`: 7 casos, un contrato.
- `manifest/cases.jsonl`: conserva cero filas de `SelfHost.Symbols` porque todos los casos son pendientes.
- `run_selfhost_symbols_gate.py`: comprobación futura del denominador, rangos, capas, perfiles y disposición.

Todos los recorridos son iterativos. Complejidad de registro: tiempo `O(35)` y memoria propia constante aparte
de las entradas almacenadas.

## Estado

Los tres registros, módulos y el gate estructural están conectados en `TestRegistry.h`, `main.cpp` y CMake. El
fragmento vacío se retiró y el gate exige cero filas del componente en el manifiesto central. No se tocaron
tracking ni cursor. No se ejecutaron builds, pruebas, gates ni runners. Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 35/35 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.
