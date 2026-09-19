# NativeSharp 2.0 self-hosting — tanda 03: biblioteca anfitriona

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-020` a `NS2-SH-029` |
| Gate | `SH02` |
| Rama | `development` |
| Milestone | `nativesharp-2.0-selfhost-milestone` (`6a9b94850`) |
| Commit de entrada | `b7d84a4cc` |
| Commit de cierre | este commit (`selfhost(tanda-03): certify compiler host workload`) |
| Fecha de apertura | 2026-09-19 |
| Fecha de cierre | 2026-09-19 |
| Baseline de cierre | `tanda-03-sh02-final-20260919` |

## 2. Objetivo y alcance

### Objetivo

Demostrar con un programa NativeSharp compilable por `nsc0` que la biblioteca permitida por el
Bootstrap Profile puede sostener una carga representativa del compilador: fuentes UTF-8, vistas,
tokens sintéticos, tablas, orden estable, serialización, diagnósticos, procesos y arenas medibles.

### Incluido

- corpus de tres fuentes con Unicode, comentarios, literales y una ruta deliberadamente larga;
- carga NativeSharp que usa `StringView`, `Span`, `ReadOnlySpan`, `Optional`, `List`, `Dictionary`,
  `HashSet`, hashing, IO binario, `Stopwatch`, filesystem y procesos;
- orden estable sin asignaciones para tablas pequeñas;
- contadores explícitos de vida de asignaciones en `ArenaAllocator`;
- compilación O0 y dos ejecuciones idénticas con `lld-link` y NSLink;
- reporte de asignaciones, pico de arena y tiempo; Gate SH02 y baseline acumulativo.

### No incluido

- pools especializados: la carga no mostró todavía una necesidad distinta de la arena existente;
- lexer o parser reales de `nsc-next`;
- bindings LLVM/Win32, que pertenecen a SH03;
- optimización de rendimiento general del compilador o ejecución paralela;
- comparar hashes de EXE, porque SH02 certifica comportamiento y determinismo observable.

## 3. Diseño

### Arquitectura

CTest invoca un script CMake que compila `CompilerHostWorkload.nsproj` con el `nsc0` construido por
esa misma matriz. El ejecutable enumera el corpus, ordena sus rutas, representa cada byte de texto
como token sintético, llena tres familias de colección, reserva almacenamiento en una arena,
serializa un encabezado binario, verifica truncación e invoca el linker activo mediante `Process`.

La carga se ejecuta dos veces. stdout y stderr deben coincidir byte a byte; el tiempo se escribe en
un archivo separado porque medirlo es obligatorio pero incluirlo en la salida comparable volvería
no determinista la prueba. CMake valida y materializa `workload-report.json` en el árbol de build.

Flujo: perfil SH01 → compilación O0 → corpus ordenado → vistas/tokens/tablas/arena → round-trip
binario → proceso linker → `Reset()` → diagnóstico/resumen → repetición → comparación/reporte.

### Contratos e invariantes

- el corpus contiene exactamente tres archivos no vacíos y se procesa en orden ordinal;
- `StringView` presta del texto durante su vida local y nunca escapa;
- existe exactamente una asignación de arena por token; ninguna queda activa después de `Reset()`;
- `TotalAllocationCount()` es acumulativo; `ActiveAllocationCount()` representa obligaciones vivas;
- empates de `StableSort` conservan el orden de entrada y la operación no asigna memoria;
- el formato binario se consume por completo y una lectura posterior devuelve `Optional` vacío;
- el linker devuelve 0 y produce salida; la carga devuelve 42;
- dos ejecuciones de la misma carga producen stdout/stderr idénticos;
- el tiempo medido no excede 30,000 ms; el umbral detecta bloqueo, no es presupuesto de producto.

### Decisiones y alternativas

| Decisión | Alternativas consideradas | Razón |
|---|---|---|
| insertion sort estable | quicksort existente, merge sort con buffer | tablas por scope son pequeñas; conserva empates y no asigna |
| contadores en la arena existente | pool nuevo, tracker externo | la evidencia pidió vida de arena, no otra abstracción |
| tiempo fuera de stdout/stderr | normalizar texto, omitir tiempo | mide sin invalidar comparación byte a byte |
| corpus leído, no compilado | hacer cada archivo sintético válido | estresa el camino anfitrión y permite contenido hostil deliberado |
| ejecutar el linker activo | simular proceso | certifica stdout, exit code, quoting y dependencia externa real |
| excluir `selfhost/evidence` del digest de fuentes | incluir gates/reportes | evita que un gate que contiene el digest sea autorreferente |

## 4. Inventario de archivos

| Archivo/familia | Acción | Responsabilidad | Razón |
|---|---|---|---|
| `CMakeLists.txt` | modificar | registrar CTest SH02 serial | incluir la carga en todo baseline |
| `cmake/VerifyNativeSharpCompilerHostWorkload.cmake` | crear | compilar, ejecutar dos veces, comparar y reportar | orquestación común para ambos linkers |
| `cmake/VerifyNativeSharpBootstrapProfile.cmake` | modificar | filtrar también `selfhost/tests/host` | impedir que la prueba use capacidades prohibidas |
| `lib/NativeSharp.Core/ArenaAllocator.ns` | modificar | contadores total/activo/reset | ownership y fugas observables |
| `lib/NativeSharp.Core.Collections.Generic/Algorithm.ns` | modificar | `StableSort<T>` | algoritmo requerido por tablas del compilador |
| `selfhost/tests/host/CompilerHostWorkload/*` | crear | carga NativeSharp representativa | evidencia integrada de stdlib |
| `selfhost/tests/host/corpus/*` | crear | entradas UTF-8/ruta larga | estrés reproducible y pequeño |
| `nscbuilder.py` | modificar | excluir evidencia del digest de entrada | eliminar autorreferencia del gate |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | fijar exclusión de runs y gates | regresión del contrato de identidad |
| `docs/2.0/SELF_HOSTING_ROADMAP.md` | modificar | enlazar expediente/carga/gate | navegación normativa |
| `docs/2.0/self-hosting/README.md` | modificar | descubrir Tanda 03 | índice humano |
| `selfhost/tests/README.md` | modificar | documentar semántica de la carga | operación y ownership |
| este expediente | crear | diseño, firmas, resultados y cierre | metodología por tanda |
| `selfhost/evidence/gates/SH02/*` | crear | métricas y gate versionado | evidencia acumulativa |
| `MANIFEST_SHA256.txt` | modificar | hashes de archivos cubiertos | integridad del repositorio |

## 5. Tipos y datos

| Tipo | Clase | Responsabilidad | Invariantes | Ownership/vida |
|---|---|---|---|---|
| `ArenaAllocator` | class | arena de bloques y contadores | activo ≤ total; reset deja activo 0 | `raii`, posee bloques |
| `StringView` | borrowed struct | recorrer fuente sin copiar | no escapa del `string` leído | préstamo local |
| `Span<string>` | borrowed struct | vista mutable para sorting | longitud de arreglo estable | préstamo del arreglo |
| `ReadOnlySpan<byte>` | borrowed struct | entrada del lector binario | no modifica `byte[]` | préstamo del buffer |
| `Optional<T>` | struct | representar lectura disponible/truncada | vacío después de EOF | valor |
| `List<int>` | class | secuencia de tokens sintéticos | count = bytes del corpus | `raii` |
| `Dictionary<int,int>` | class | frecuencias por clase | cuatro clases conocidas | `raii` |
| `HashSet<int>` | class | clases distintas | cuatro elementos | `raii` |
| reporte SH02 | JSON v1 | métricas de carga | status passed, dos runs, exit 42 | artefacto de build/evidencia |

## 6. Métodos y funciones

| Firma | Responsabilidad | Entradas y precondiciones | Salida y postcondiciones | Errores/diagnósticos | Efectos, ownership y concurrencia |
|---|---|---|---|---|---|
| `Algorithm.StableSort<T>(Span<T>, Comparison<T>)` | ordenar estable | span válido y comparador total | orden ascendente; empates preservados | propaga fallo del comparador | in-place, sin asignación, single-thread |
| `ArenaAllocator.TotalAllocationCount()` | contar reservas exitosas históricas | arena viva | contador monotónico | ninguno | sólo lectura |
| `ArenaAllocator.ActiveAllocationCount()` | contar obligaciones vivas | arena viva | 0 después de reset | ninguno | sólo lectura |
| `ArenaAllocator.ResetCount()` | contar resets | arena viva | incrementa una vez por reset | ninguno | sólo lectura |
| `ArenaAllocator.HasOutstandingAllocations()` | consultar fuga lógica | arena viva | equivale a activo != 0 | ninguno | sólo lectura |
| `Program.CompareText(left,right)` | orden ordinal de rutas | strings no nulos | relación ordinal | ninguno | pura |
| `Program.CompareFirstCharacter(left,right)` | forzar empates observables | strings no vacíos | compara primer byte | ninguno | pura |
| `Program.Kind(value)` | clasificar token sintético | byte de fuente | clase 1..4 | ninguno | pura |
| `Program.CountToken(...)` | actualizar arena/lista/map/set | contenedores vivos | una reserva y un token | false si arena falla | muta estructuras, borrows locales |
| `Program.Main()` | ejecutar Gate SH02 | corpus/linker/destinos válidos | stdout/resumen, stderr/NSH0001, exit 42 | exits 1..22 localizan invariante | single-thread; limpia owners `raii` |

## 7. Superficie observable

- CTest nuevo: `nsc.selfhost.compiler-host-workload`;
- reporte `nativesharp-selfhost-host-workload` v1;
- resumen estable: `SH02 files=3 tokens=N allocations=N arena_peak_bytes=N`;
- diagnóstico estable: `SH02 diagnostic NSH0001: synthetic corpus verified`;
- retorno exitoso de la carga: 42; retorno exitoso del linker invocado: 0;
- `NSC_LLD_LINK` selecciona NSLink en la segunda matriz; de lo contrario se usa `CMAKE_LINKER`;
- no se añade una opción pública nueva a `nsc` ni se cambia ABI del runtime.

## 8. Implementación

Primero se creó la carga integrada y se comprobó manualmente con ambos linkers. Después se hicieron
observables los contadores de la arena y el orden estable. La primera versión validaba el límite de
tiempo, pero no persistía la medición; se separó `elapsed_ms` de stdout/stderr y CMake pasó a generar
un reporte. Una primera ejecución de esa revisión encontró CRLF en el archivo de métricas; el parser
se corrigió con `string(STRIP)` y se repitieron ambas pruebas específicas.

La carga no justificó un contenedor ni pool adicional: las colecciones actuales cubrieron la tabla
representativa. Ése es el resultado explícito de `NS2-SH-028`, no una omisión. También se corrigió
el digest de fuentes para que la evidencia derivada no pueda describirse a sí misma.

## 9. Pruebas

| Prueba | Tipo | Propósito | Resultado | Evidencia |
|---|---|---|---|---|
| Bootstrap Profile sobre host | regresión | impedir capacidades fuera de SH01 | PASS | `bootstrap-profile-positive` |
| carga con lld-link | positiva | certificar stdlib/proceso/determinismo | PASS, retorno 42 × 2 | reporte SH02 |
| carga con NSLink | diferencial | repetir contrato con linker NativeSharp | PASS, retorno 42 × 2 | reporte SH02 |
| empate `a/b` intercalado | regresión | demostrar estabilidad real | PASS | exit distinto de 22 |
| lectura posterior a EOF | negativa | demostrar truncación segura | PASS, Optional vacío | exit distinto de 15 |
| reset de arena | regresión | cero obligaciones y bytes usados | PASS | exits distintos de 18..20 |
| contrato de digest | unit | excluir output/evidencia, incluir fuentes | PASS | unit tests Python |

## 10. Comparación diferencial

La misma fuente NativeSharp se compila con `nsc0` en las dos matrices y se enlaza alternativamente
con `lld-link` y NSLink. Ambas variantes procesan 3 archivos, 282 tokens, 282 asignaciones y un pico
de arena de 7,424 bytes; ambas retornan 42 y repiten exactamente stdout/stderr. El tiempo se compara
contra el mismo máximo de seguridad, no entre linkers, porque no es una salida determinista.

## 11. Baseline de cierre

| Campo | Valor |
|---|---|
| Comando | `python -B nscbuilder.py --baseline --config selfhost/bootstrap/bootstrap.config.example.json --run-id tanda-03-sh02-final-20260919 --jobs 16` |
| Identificador | `tanda-03-sh02-final-20260919` |
| Revisión probada | árbol de cierre de esta tanda sobre `b7d84a4cc` |
| Fecha/entorno | 2026-09-19; Windows x64; LLVM `C:\clang-llvm`; prefijo `A:\native-sharp` |
| Reporte Markdown | `selfhost/evidence/runs/tanda-03-sh02-final-20260919/report.md`, SHA-256 `35cb4f0be39a4325eb1d0fbca4bcbfc51073fa132b26197b24c238c892b670c7` |
| Reporte JSON | `selfhost/evidence/runs/tanda-03-sh02-final-20260919/report.json`, SHA-256 `42273f608792b651d08bd6e002410862ac592d287ded55a94f359c6eeb4999c7` |
| Reporte de workload | `selfhost/evidence/gates/SH02/workload-report.json`, SHA-256 `a7a5783ebec433a7542c9f533defa9ea685562e2a28e506a4a79678386db269c` |
| Estado | `PASSED`, 0 fallos, 452.888 s; fuente `8cf4c876f3bc0c808f70ebb78d18a1eebda767326c8e32e23e4e3cdd2e2d5e31`; `nsc0` construido = instalado `556850a81de30e4a440e4c4ccead6cc27c8b5310e2abb595507e6bf500bc7859` |

### Matriz y resultados esperados para el cierre

| Matriz | Total | Aprobadas | Fallidas | Omitidas |
|---|---:|---:|---:|---:|
| CTest/lld-link | 439 | 439 | 0 | 0 |
| CTest/NSLink | 439 | 439 | 0 | 0 |
| Fixtures (148 por linker) | 296 | 296 | 0 | 0 |
| Bug triage (18 × 2 linkers) | 36 | 36 | 0 | 0 |
| Self-hosting SH00/SH01/SH02 (4 × 2 linkers) | 8 | 8 | 0 | 0 |

### Corridas anteriores no promovidas

Las pruebas específicas iniciales pasaron, pero no se promovieron como baseline. Después de añadir
la persistencia de tiempo, una ejecución específica por linker falló sólo en el lector del archivo
de métricas: `WriteAllText` produjo CRLF y la expresión exigía LF. Se normalizó con
`string(STRIP)`; ambas pruebas se repitieron y pasaron. No fue un fallo del programa NativeSharp.

## 12. Riesgos, limitaciones y deuda

- `StableSort` es O(n²) y está destinado a tablas pequeñas; tablas grandes deberán medir merge o
  sorting de índices antes de cambiarlo;
- el pico reportado corresponde a capacidad reservada por la arena, no al working set del proceso;
- el corpus es pequeño y sintético; SH05 en adelante añadirá fuentes reales al lexer/parser;
- `Directory` y `File` usan hoy la plataforma disponible; SH03 encapsulará la frontera Win32;
- 30 segundos es watchdog, no promesa de rendimiento;
- el workload es single-thread por contrato SH01.

## 13. Contenido del commit

### Archivos incluidos

Los archivos incluidos son exactamente los de la sección 4 más los artefactos de cierre del
baseline. El staging se audita por nombre antes del commit y excluye cambios locales preexistentes.

### Higiene

- [x] No contiene `.obj`, `.exe`, `.dll`, `.lib`, PDB, temporales ni cachés no normativos.
- [x] No contiene cambios locales ajenos a la tanda.
- [x] El manifiesto de fuentes está actualizado cuando corresponde.
- [x] La documentación describe las firmas y archivos realmente incluidos.
- [x] El baseline se ejecutó después del último cambio funcional.

### Commit de cierre

```text
selfhost(tanda-03): certify compiler host workload
```

## 14. Criterio de cierre

- [x] `NS2-SH-020` a `NS2-SH-029` terminados.
- [x] Pruebas específicas verdes con ambos linkers y retorno 42.
- [x] Baseline completo verde con ambos linkers.
- [x] Gate SH02 `LANDED`, enlazado a SH01 y sin problemas.
- [x] Expediente, implementación y commit coinciden.
