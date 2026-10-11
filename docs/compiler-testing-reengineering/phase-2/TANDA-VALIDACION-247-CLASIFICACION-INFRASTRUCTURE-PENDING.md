# Validacion 247: clasificacion exhaustiva de `InfrastructurePending`

## Estado

`AUDITORIA DOCUMENTAL COMPLETA; SIN CONFIGURACION, BUILD NI EJECUCION`

Esta validacion clasifica los `3,438` casos pendientes de infraestructura que
pertenecen al compilador. No modifica manifiestos, casos, producto, CMake,
Builder 3 ni la infraestructura de ejecucion.

## Universo auditado

La reconstruccion aplica la misma precedencia documental usada por Builder 3:

1. disposicion explicita en los catalogos JSONL;
2. presencia en `tests/compiler-suite/manifest/cases.jsonl` como `Executable`;
3. ausencia de ambas como `InfrastructurePending`.

El inventario vigente contiene `4,044` casos `InfrastructurePending`. De ellos,
`606` pertenecen a componentes externos `Library.*` o `SDK` y se excluyen del
alcance del compilador por su componente canonico:

| Componente externo | Casos |
|---|---:|
| `Library.NativeSharp.Core` | 264 |
| `Library.NativeSharp.Core.Collections.Generic` | 156 |
| `Library.NativeSharp.Core.Threading` | 66 |
| `Library.NativeSharp.Core.Diagnostics` | 30 |
| `Library.NativeSharp.Core.IO` | 30 |
| `Library.NativeSharp.LLVM.C` | 24 |
| `Library.NativeSharp.Win32.Compiler` | 24 |
| `Library.Provenance` | 6 |
| `SDK` | 6 |
| **Total excluido** | **606** |

La identidad queda cerrada por `4,044 - 606 = 3,438`.

## Fuentes canonicas cruzadas por caso

Cada fila del CSV y JSON cruza, por identificador y no por semejanza nominal:

- `inventory/test-case-design.csv`: `case_id`, componente, contrato, objetivo,
  preparacion, entrada, resultado y documento de origen;
- `inventory/suite-case-allocation.csv`: capa, ejecutor planeado, familia y
  razon de asignacion;
- `inventory/contracts.csv`: nombre del contrato, precondiciones, salidas,
  oraculo observable, dependencias y documento de autoridad;
- manifiesto ejecutable y catalogos de disposicion: origen del estado
  `InfrastructurePending`.

Huellas SHA-256 de las cuatro fuentes centrales:

| Fuente | SHA-256 |
|---|---|
| `test-case-design.csv` | `EB40C84F4425570C9FF112AB9A4E140BA43461AAB6770187E73D1B5974ECA04E` |
| `suite-case-allocation.csv` | `D0802E8E2748375F8E99AD9506C35F4CC5F751BC9E8AD8F648A9815490FEFC02` |
| `contracts.csv` | `26BE6B07A97E399482A576079DED6671987060381047D1362CC8CE5F6AF0A48F` |
| `manifest/cases.jsonl` | `797EF8729EC91803EA0AB8FD622069C3D0B5ADB36070420CA3399D606B6BE6A3` |

## Regla de clasificacion

La segunda auditoria no usa el ejecutor planeado como decision. Para cada uno
de los `489` contratos representados se revisaron entradas, salidas, oraculo,
dependencias, capa y las variantes de diseño de sus casos. La pregunta rectora
fue si el contrato puede observarse por la interfaz publica de `nsc.exe` sin
perder informacion.

- **Clase 1**: una sola unidad `.ns` puede provocar el contrato y
  `nsc.exe` expone aceptación/rechazo, diagnostico, dump, artefacto o
  ejecucion suficiente para el oraculo.
- **Clase 2**: el oraculo tambien es publico, pero requiere identidad de
  proyecto o biblioteca, referencias, metadata o varias unidades; por ello
  necesita `.nsproj` o multiples `.ns`.
- **Clase 3**: el contrato exige estado que una entrada publica no puede
  construir o distinguir: identidad de punteros, vida util, grafos sinteticos,
  parsers binarios, modelos internos o transiciones privadas.
- **Clase 4**: el contrato exige orquestacion de procesos, herramientas,
  cache, instalacion, filesystem transaccional, entorno, self-host, packaging
  o una frontera real de toolchain.
- **Clase 5**: se reserva para evidencia insuficiente. Se usa cuando una misma
  fila mezcla alternativas "segun aplicabilidad" —por ejemplo MIR grande,
  cache concurrente o particiones— sin fijar cual debe materializarse.

Esta regla reclasifica casos aunque su asignacion actual diga
`nsc_component_tests`: si el comportamiento es observable por el producto, se
prefiere el fixture publico. Tambien evita el movimiento inverso: un dump
interno de un grafo sintetico no se declara publico solo porque produzca texto.

## Resultado exacto

| Clase | Casos |
|---|---:|
| `1_SINGLE_NS_FIXTURE` | 313 |
| `2_NSPROJ_OR_MULTI_NS` | 329 |
| `3_INTERNAL_CPP_UNIT` | 554 |
| `4_INTEGRATION_ORCHESTRATION_SELFHOST_TOOLCHAIN` | 2,171 |
| `5_NO_DETERMINABLE` | 71 |
| **Total** | **3,438** |

## Ejemplos representativos

- **Clase 1 — `TF-LLVM-409`**: `Statements loops switch and control flow` se
  provoca con una unidad fuente y su estructura IR, artefacto o ejecucion es
  observable por `nsc.exe`.
- **Clase 2 — `TF-DRIVER-233`**: `Project OOP object generation transaction
  and debug contract` requiere identidad y salida de proyecto; corresponde a
  `.nsproj` o multiples fuentes.
- **Clase 3 — `TA-AST-014`**: `Arena ownership and failure boundaries` exige
  fallo simulado despues de `new`, conteo de destrucciones e identidad interna;
  una fuente `.ns` no conserva ese oraculo.
- **Clase 4 — `TF-NSC-121`**: `Explicit and automatic compilation logs`
  controla proceso, streams, rutas, publicacion atomica y codigo de salida;
  necesita orquestacion.
- **Clase 5 — `TF-LLVM-416`**: la fila combina MIR grande, layouts profundos,
  cache concurrente y particiones "segun aplicabilidad"; no existe evidencia
  suficiente para elegir una sola topologia sin diseñar primero el caso.

## Distribucion por componente

| Componente | Clase 1 | Clase 2 | Clase 3 | Clase 4 | Clase 5 | Total |
|---|---:|---:|---:|---:|---:|---:|
| Host.AST | 0 | 0 | 6 | 0 | 0 | 6 |
| Host.CFG | 0 | 0 | 4 | 0 | 0 | 4 |
| Host.Dominance | 0 | 0 | 7 | 0 | 0 | 7 |
| Host.Driver | 21 | 98 | 8 | 225 | 0 | 352 |
| Host.Interop | 0 | 147 | 40 | 37 | 0 | 224 |
| Host.LanguageService | 0 | 0 | 80 | 0 | 0 | 80 |
| Host.Linker | 12 | 54 | 155 | 86 | 0 | 307 |
| Host.LLVM | 144 | 25 | 147 | 88 | 36 | 440 |
| Host.nsc | 0 | 0 | 0 | 160 | 0 | 160 |
| Host.Parser | 0 | 0 | 3 | 0 | 0 | 3 |
| Host.Passes | 0 | 0 | 6 | 0 | 0 | 6 |
| Host.Runtime | 136 | 5 | 86 | 42 | 35 | 304 |
| Host.Sema | 0 | 0 | 8 | 0 | 0 | 8 |
| Host.Source | 0 | 0 | 4 | 0 | 0 | 4 |
| Packaging | 0 | 0 | 0 | 6 | 0 | 6 |
| SelfHost.Bootstrap | 0 | 0 | 0 | 23 | 0 | 23 |
| SelfHost.Bootstrap.Infrastructure | 0 | 0 | 0 | 438 | 0 | 438 |
| SelfHost.CodeGen | 0 | 0 | 0 | 136 | 0 | 136 |
| SelfHost.Contracts | 0 | 0 | 0 | 288 | 0 | 288 |
| SelfHost.Driver | 0 | 0 | 0 | 79 | 0 | 79 |
| SelfHost.IR | 0 | 0 | 0 | 92 | 0 | 92 |
| SelfHost.Parser | 0 | 0 | 0 | 127 | 0 | 127 |
| SelfHost.Project | 0 | 0 | 0 | 22 | 0 | 22 |
| SelfHost.Projects | 0 | 0 | 0 | 95 | 0 | 95 |
| SelfHost.Sema | 0 | 0 | 0 | 153 | 0 | 153 |
| SelfHost.Source | 0 | 0 | 0 | 2 | 0 | 2 |
| SelfHost.Symbols | 0 | 0 | 0 | 35 | 0 | 35 |
| SelfHost.Syntax | 0 | 0 | 0 | 36 | 0 | 36 |
| SelfHost.Types | 0 | 0 | 0 | 1 | 0 | 1 |
| **Total** | **313** | **329** | **554** | **2,171** | **71** | **3,438** |

## Artefactos auditables

- `INFRASTRUCTURE-PENDING-3438-CLASSIFICATION.csv`: una fila por `case_id`,
  adecuada para filtros y conciliacion tabular.
- `INFRASTRUCTURE-PENDING-3438-CLASSIFICATION.json`: la misma proyeccion,
  adecuada para consumo mecanico.

Ambos contienen los campos `case_id`, `component`, `contract_id`, `contract`,
`class`, `evidence`, `source` y `reason`. La comprobacion estatica confirma:

- CSV: `3,438` filas;
- JSON: `3,438` objetos;
- identificadores unicos: `3,438`;
- filas con campos requeridos vacios: `0`;
- suma de clases: `3,438`.

Huellas de salida:

| Artefacto | SHA-256 |
|---|---|
| CSV | `DF24A257BC4A74F819A72DC392DE0A5723041D15A6E939A8C50F02265E0B3CF9` |
| JSON | `779017CB423A68D6CC4BA0578F7FFE12E840E09457D1ECEECE249FA4C3052992` |

## Limite de la evidencia

Esta validacion determina la topologia necesaria para materializar cada caso;
no implementa los casos pendientes y no demuestra que sus contratos pasen. No
se configuro, compilo ni ejecuto ninguna prueba.

## Nota de transición posterior

Los conteos, tablas y huellas anteriores son el snapshot histórico producido
por la validación 247 y no se reescriben. La validación 254 reconcilia el estado
vivo después de integrar `14` de los `18` casos re-auditados: los catálogos con
nombre de procedencia `3438` contienen ahora `3,424` filas. Cuatro casos
permanecen como trabajo futuro fuera del pipeline: `TA-AST-017`, `TS-SEMA-077`,
`TS-SEMA-094` y `TS-SRC-049`.
