# Tanda 05.03 — Materialización e integración de Host.Linker

Fecha: 2026-10-09

## Alcance y SCOPE LOCK aplicado

Esta tanda materializa exclusivamente la suite nueva de `Host.Linker` definida
por la Fase 1. Conserva la trazabilidad
`case_id -> contract_id -> Host.Linker -> oracle` y no modifica producto,
batería histórica, repositorios externos, `C:`, remotos ni los puntos de
integración ajenos a la suite nueva.

La integración central autorizada posteriormente se limitó a:

- `tests/compiler-suite/host/include/nsc_suite/TestRegistry.h`;
- `tests/compiler-suite/host/src/main.cpp`;
- `tests/compiler-suite/CMakeLists.txt`;
- `tests/compiler-suite/manifest/cases.jsonl`;
- `docs/compiler-testing-reengineering/phase-2/TRACKING.md`;
- `docs/compiler-testing-reengineering/phase-2/CURSOR.json`.

No se modificó código de producto. CMake compone las dos fuentes existentes
que exponen los símbolos ejercidos por los casos ejecutables:
`WindowsLibraryBuilder.cpp` y `WindowsRuntimeLibrary.cpp`.

No se ejecutaron compilaciones, pruebas, gates, controladores ni validaciones.

## Inventario canónico

Las fuentes cerradas son:

- `inventory/implementation-waves.csv`;
- `inventory/contracts.csv`;
- `inventory/test-case-design.csv`;
- `inventory/suite-case-allocation.csv`;
- `components/HOST-LINKER.md`;
- `test-plans/HOST-LINKER-TEST-PLAN.md`;
- `audits/TANDA-09-HOST-LINKER-AUDIT.md`.

El conteo exacto conservado es:

| Dimensión | Conteo |
|---|---:|
| archivos de producto inventariados | 16 |
| símbolos físicos inventariados | 132 |
| contratos `CF-LINKER-*` | 40 |
| casos `TF-LINKER-*` | 320 |
| casos PR | 280 |
| casos NIGHTLY | 40 |
| casos omitidos | 0 |
| IDs desconocidos | 0 |

Cada contrato conserva ocho IDs consecutivos. El primer caso es
`TF-LINKER-001`, el último es `TF-LINKER-320` y no existen huecos en el
intervalo.

## Organización materializada

`LinkerTestSupport.h` contiene el catálogo congelado de los cuarenta nombres,
capas y oráculos, el mapeo aritmético de IDs y dos superficies separadas:

1. registro ejecutable ordinario para casos con oráculo local completo;
2. inventario `InfrastructurePending` para casos cuyo productor de fixtures no
   existe aún.

Los módulos consolidados quedan divididos por frontera causal:

- `LinkerFormatsModule.cpp`: COFF archive, PE32+, PDB y verificación ABI del
  archive runtime;
- `LinkerToolProcessModule.cpp`: discovery, response files, NSLink,
  `llvm-lib`, child process e import libraries;
- `LinkerBuildersModule.cpp`: EXE/DLL/LIB, cache, runtime, publicación y las
  partes locales ejecutables de configuración y resolución.

La división no reduce el denominador. Los casos pendientes existen como
registros individuales y conservan ID, contrato, componente, capa, perfil,
oráculo y requisito bloqueante.

## Matriz de materialización

| Contratos | Casos | Cantidad | Estado materializado | Causa |
|---|---|---:|---|---|
| `001..003` | `001..024` | 24 | `InfrastructurePending` | builder COFF independiente |
| `004..013` | `025..104` | 80 | `InfrastructurePending` | NSLink, llvm-lib y proceso controlado |
| `014..023` | `105..184` | 80 | `InfrastructurePending` | builders PDB MSF 7 y PE32+ |
| `024..033` | `185..264` | 80 | `InfrastructurePending` | productos EXE/DLL/LIB, cache y runtime |
| `034` | `265..272` | 8 | 6 ejecutables, 2 pendientes | resultado local frente a producto real |
| `035..036` | `273..288` | 16 | `InfrastructurePending` | import libraries mediante NSLink |
| `037` | `289..296` | 8 | 7 ejecutables, 1 pendiente | resolución local frente a entorno aislado |
| `038` | `297..304` | 8 | `InfrastructurePending` | archive COFF con catálogo ABI completo |
| `039..040` | `305..320` | 16 | `InfrastructurePending` | runtime construido y publicación atómica |

El gate aritmético de esta materialización queda:

```text
inventariados = 320
materializados = 320
ejecutables = 13
InfrastructurePending = 307
KnownProductDefect = 0
omitidos = 0
desconocidos = 0

320 = 13 + 307 + 0
```

Por perfil:

```text
ejecutables PR = 11
ejecutables NIGHTLY = 2
pendientes PR = 269
pendientes NIGHTLY = 38
```

## Casos ejecutables

Los trece casos incluidos en el fragmento de manifiesto son:

```text
TF-LINKER-265
TF-LINKER-267
TF-LINKER-268
TF-LINKER-269
TF-LINKER-271
TF-LINKER-272
TF-LINKER-289
TF-LINKER-290
TF-LINKER-291
TF-LINKER-292
TF-LINKER-293
TF-LINKER-295
TF-LINKER-296
```

`TF-LINKER-266` y `TF-LINKER-270` requieren productos estáticos/dinámicos y
telemetría posterior a un enlace real. `TF-LINKER-294` requiere controlar el
override global `NSC_WINDOWS_RUNTIME_LIBRARY` en un proceso aislado. Por ello
los tres permanecen fuera del manifiesto ejecutable y dentro del inventario
pendiente.

## Especialización contra capacidad vigente

Los objetivos genéricos de Fase 1 se especializaron sin cambiar IDs ni añadir
features al producto:

- `TF-LINKER-291` verifica que, sin default configurado, la API pública devuelve
  el candidato adyacente; `resolveWindowsRuntimeLibrary` no posee canal de
  rechazo;
- `TF-LINKER-292` verifica que el resolver puro no inventa validación de paths;
  el rechazo de comillas pertenece a la frontera de proceso de `LldLinker`;
- `TF-LINKER-296` aplica escala a la resolución pura. La concurrencia sobre
  environment global queda fuera de este caso local y no se presenta como
  verificada.

Esta especialización sigue la regla de Fase 2: una expectativa que no
corresponde a una capacidad pública vigente se reescribe contra el contrato
real, no obliga a ampliar el producto.

## InfrastructurePending

Los 307 pendientes no son `PASS`, `FAIL`, `SKIP`, `XFAIL` ni defectos de
producto. Requieren uno o más productores ausentes:

- builders binarios con oráculo independiente y mutaciones acotadas para COFF
  archive, PE32+ y PDB MSF 7;
- ejecutables child controlados para argv, cwd, stdin, stdout, stderr, log y
  exit status;
- NSLink y `llvm-lib` controlados, incluyendo response files e import/static
  libraries;
- objetos COFF verificados, runtime Windows, cache e incremental state;
- productos EXE/DLL/LIB y comprobación de limpieza/publicación atómica;
- proceso aislado para variables de entorno globales.

Un productor futuro debe materializar esos fixtures antes de mover cualquier
ID al manifiesto ejecutable. No se permite usar herramientas instaladas,
artefactos antiguos o fallbacks como sustituto.

## KnownProductDefect

No se encontró evidencia ejecutada ni un defecto real previamente documentado
para asignar `KnownProductDefect` en esta tanda. El conteo permanece en cero.
Una falla futura sólo podrá recibir ese estado después de existir fixture,
oráculo contractual y reproducción contra el producto vigente.

## Controlador y manifiesto centrales

`manifest/cases.jsonl` contiene únicamente los trece casos ejecutables de
`Host.Linker`. Los `307` pendientes no entran en la selección ordinaria. El
fragmento temporal se retiró después de confirmar textualmente que los trece
IDs estaban presentes una sola vez en el manifiesto central y en el orden
canónico.

`controller/integration/run_host_linker_gate.py` comprueba el conjunto exacto y
su orden, filtra `Host.Linker` desde el manifiesto central y exige trece
resultados `PASS` cuando se ejecute la validación offline. Su existencia no
afirma que haya sido ejecutado.

`TestRegistry.h`, `main.cpp` y `CMakeLists.txt` registran los tres módulos una
sola vez. CMake registra también el gate controlador y las dos fuentes de
producto estrictamente referenciadas por los oráculos locales. Esta composición
fue confirmada por inspección textual; no se compiló ni enlazó.

## Complejidad, ownership y recursión

- El mapeo `contrato + variante -> ID` es O(1).
- La materialización completa es O(320) en tiempo y espacio persistente.
- La materialización de los 307 pendientes es O(n): cada ID se agrega una sola
  vez desde exactamente un módulo propietario.
- Los fixtures ejecutables poseen sus paths y limpian únicamente el directorio
  temporal creado por el caso.
- No se introdujo recursión directa, indirecta ni mediante callbacks.

## Estado exacto

```text
Inventario Host.Linker: 320/320 MATERIALIZADO
Casos ejecutables declarados: 13
Casos InfrastructurePending: 307
Casos KnownProductDefect: 0
Integración central: MATERIALIZADA; CONFIRMACIÓN TEXTUAL SOLAMENTE
Build Debug/Release: NO EJECUTADO
Pruebas: NO EJECUTADAS
Gate controlador: NO EJECUTADO
Análisis estático automatizado: NO EJECUTADO
Validación dinámica: NO EJECUTADA
```

La tanda queda materializada e integrada en la composición de la suite nueva.
No está compilada, enlazada, ejecutada ni verificada y no se presenta como
cierre validado de `Host.Linker`.
