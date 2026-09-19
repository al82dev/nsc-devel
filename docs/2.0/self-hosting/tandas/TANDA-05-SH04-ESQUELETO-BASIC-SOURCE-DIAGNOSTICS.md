# NativeSharp 2.0 self-hosting — tanda 05: esqueleto, Basic, Source y Diagnostics

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-045` a `NS2-SH-054` |
| Gate | `SH04` |
| Rama | `development` |
| Commit de entrada | `d333635d4` |
| Commit de cierre | este commit (`selfhost(tanda-05): bootstrap nsc-next basic services`) |
| Fecha de apertura | 2026-09-19 |
| Fecha de cierre | 2026-09-19 |
| Baseline de cierre | `tanda-05-sh04-final-20260919` |

## 2. Objetivo y alcance

### Objetivo

Entregar el primer `nsc-next.exe` escrito completamente en NativeSharp. Todavía no tokeniza ni
compila: fija la identidad operativa, lee fuentes de manera determinista, modela posiciones y
rangos, controla vida de arenas y reproduce los primeros diagnósticos de `nsc0` con códigos de
salida idénticos. La salida estructurada y las rutas normalizadas quedan listas antes del parser.

### Incluido

- proyecto único `NativeSharp.Compiler.nsproj`, compilable por `nsc0` bajo Bootstrap Profile 2.0;
- módulos físicos Basic, Source, Diagnostics y Driver, sin dependencias C++;
- `FileId`, `SourcePosition`, `SourceRange`, `CompilerResult` y FNV-1a de 64 bits;
- arena por generación: un handle anterior a `Reset()` no vuelve a resolver;
- buffer propietario, finales CRLF/CR/LF canónicos, line map y consulta línea/columna;
- identidad `nsc-next 2.0.0-bootstrap.sh04`, ayuda y códigos de salida 0/1/2/3;
- catálogo `NSC0001`/`NSC0002`, render humano compatible y diagnóstico JSON v1;
- contrato `diagnostics.map.json`, diferencial O0/O2 y verificación CTest acumulativa.

### No incluido

- lexer, tokens, AST o parser; pertenecen a SH05;
- semántica, IR, LLVM o emisión desde `nsc-next`;
- compatibilidad total de la CLI de `nsc0`; cada opción se porta con su rebanada observable;
- instalación o promoción como compilador predeterminado: el gate produce un candidato aislado;
- threading, cache, logging o rendimiento final; siguen las fases del roadmap.

## 3. Diseño

### Arquitectura

Un solo proyecto evita referencias internas que ampliarían artificialmente el allowlist del
Bootstrap Profile. Sus carpetas siguen siendo límites de namespace. Driver depende de Diagnostics,
Diagnostics puede usar Source, Source depende de Basic y Basic sólo usa Core. Ningún módulo llama
al frontend C++ ni carga símbolos de `nsc0`; éste sólo actúa como compilador semilla y oráculo del
harness diferencial.

El flujo observable es: argv → decisión de Driver → validación de archivo → `SourceBuffer` →
normalización LF y line map → salida o diagnóstico. El harness compila dos PE independientes, O0 y
O2, verifica ambos, ejecuta el mismo corpus desde dos directorios y compara los errores básicos
contra el binario `nsc0` usado por la matriz.

### Contratos e invariantes

- `FileId(0)` es inválido; offsets y rangos sólo son válidos dentro del mismo archivo;
- FNV-1a opera sobre los bytes del texto normalizado y desborda módulo 2^64 deliberadamente;
- un `ArenaHandle` contiene dirección, tamaño y generación; sólo su arena puede resolverlo;
- `Reset()` invalida todos los handles existentes antes de reutilizar almacenamiento;
- `SourceBuffer` posee path, texto y line starts; una línea vacía final cuenta;
- CRLF y CR se convierten a LF antes de hash, offsets y número de líneas;
- paths estructurados son absolutos, usan `/` y no dependen del cwd;
- texto humano de los diagnósticos portados es idéntico a `nsc0`;
- JSON declara schema y versión; códigos y exit codes proceden del mapa versionado;
- O0 y O2 deben producir el mismo comportamiento observable.

### Decisiones y alternativas

| Decisión | Alternativa | Razón |
|---|---|---|
| un `.nsproj` con módulos físicos | biblioteca por carpeta | evita referencias bootstrap circulares prematuras |
| IDs como readonly structs | enteros sueltos | impide mezclar dominios al crecer el frontend |
| hash propio FNV-1a | hash del runtime | contrato pequeño, estable y portable entre generaciones |
| handle generacional | publicar puntero de arena | detecta uso después de reset sin impedir almacenamiento estable |
| texto normalizado al cargar | normalizar al render | offsets, hash y line map comparten una sola realidad |
| códigos internos desde el inicio | sólo copiar texto de `nsc0` | habilita JSON y evolución del texto sin perder identidad |
| comparación byte a byte humana | regex aproximada | evita divergencia silenciosa durante el port |

## 4. Inventario de archivos

| Archivo/familia | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/Basic/*.ns` | crear | IDs, resultados, hash y arena generacional |
| `selfhost/nsc/Source/SourceBuffer.ns` | crear | buffer, paths, normalización y line map |
| `selfhost/nsc/Diagnostics/Diagnostics.ns` | crear | catálogo y renders humano/JSON |
| `selfhost/nsc/Driver/Program.ns` | crear | entry point, identidad, ayuda y códigos |
| `selfhost/nsc/NativeSharp.Compiler.nsproj` | crear | unidad compilable bootstrap |
| `selfhost/contracts/diagnostics.map.json` | crear | mapa normativo de diagnósticos portados |
| `selfhost/bootstrap/verify_nsc_next_skeleton.py` | crear | diferencial y reporte SH04 |
| `cmake/VerifyNativeSharpNextSkeleton.cmake` | crear | compilar O0/O2 y orquestar gate |
| `CMakeLists.txt` | modificar | registrar `nsc.selfhost.nsc-next-skeleton` |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | fijar mapa y hash entre Python/NativeSharp |
| roadmap y READMEs | modificar | navegación, estado y límites |
| este expediente y Gate SH04 | crear | diseño y evidencia de tanda |
| `MANIFEST_SHA256.txt` | modificar | integridad de fuentes cubiertas |

## 5. Tipos y datos

| Tipo | Responsabilidad | Invariantes | Vida/ownership |
|---|---|---|---|
| `FileId` | identidad tipada de fuente | positivo = válido | valor |
| `SourcePosition` | archivo + offset byte | offset no negativo | valor |
| `SourceRange` | intervalo semiabierto | mismo FileId; end >= begin | valor |
| `CompilerResult` | éxito + código de proceso | éxito canónico = código 0 | valor |
| `ArenaHandle` | referencia verificable | generación exacta y tamaño positivo | no posee memoria |
| `GenerationArena` | storage por fase | Reset invalida handles | posee `ArenaAllocator` |
| `SourceBuffer` | fuente canónica | texto LF; line start 0 | posee path/texto/array |
| `Diagnostic` | evento estable | código, severidad, argumento, exit | posee strings |
| mapa JSON | relación nsc0/nsc-next | schema v1, orden y exit codes | versionado |

## 6. Métodos y funciones

| Firma | Responsabilidad | Entradas/precondiciones | Salida/postcondiciones | Errores/efectos |
|---|---|---|---|---|
| `StableHash.Text` | FNV-1a 64 | string válida | hash determinista | ninguno |
| `GenerationArena.Allocate` | reservar bytes | bytes > 0 | handle de generación actual | memoria de arena |
| `GenerationArena.Resolve` | validar handle | handle de esta fase | puntero o null | no extiende vida |
| `GenerationArena.Reset` | iniciar fase | arena viva | handles previos inválidos | reutiliza bloques |
| `DiagnosticPath.Normalize` | path estable | path accesible o textual | absoluto con `/` cuando es resoluble | consulta cwd una vez |
| `SourceBuffer.Load` | cargar y canonizar | caller verificó existencia | owner de contenido/line map | IO síncrono |
| `SourceBuffer.GetLineColumn` | localizar offset | cualquier int | línea/columna 1-based recortada | ninguno |
| `DiagnosticCatalog.*` | crear eventos conocidos | argumento válido | diagnóstico owned | ninguno |
| `DiagnosticRenderer.Human` | compatibilidad CLI | diagnóstico catalogado | texto nsc0 sin newline | asigna string |
| `DiagnosticRenderer.Json` | salida de herramientas | diagnóstico catalogado | JSON v1 en una línea | escapa texto |
| `Program.ReadSource` | rebanada vertical | path | 0 o diagnóstico 1 | lee archivo |
| `verify_nsc_next_skeleton.verify` | diferencial | tres PE/contrato/output | reporte SH04 v1 | crea fixtures aisladas |

## 7. Superficie observable

- `nsc-next --version`;
- `nsc-next --help`;
- `nsc-next --read-source <file.ns>`;
- `nsc-next --dump-source-info <file.ns>`;
- `nsc-next --diagnostic-format=json <file.ns>`;
- `nsc-next <file.ns>` para la rebanada de lectura;
- `nsc-next --self-test-basic` reservado al gate;
- CTest `nsc.selfhost.nsc-next-skeleton`;
- schemas `nativesharp-source-info`, `nativesharp-diagnostic` y
  `nativesharp-selfhost-sh04-report`, todos v1.

## 8. Implementación

Se creó primero el proyecto único y la cadena Basic → Source → Diagnostics → Driver. La primera
compilación detectó que el `Name` del proyecto no admite guiones; el nombre semántico pasó a
`NativeSharpCompilerNext` sin cambiar el nombre del artefacto `nsc-next.exe`. Después el checker de
ownership rechazó pasar un retorno owned anidado al catálogo: se materializó el path normalizado
como local RAII antes de construir el diagnóstico. Ambas incidencias se resolvieron antes del gate.

El self-test ejercita todos los valores Basic, invalida un handle con Reset, normaliza una mezcla de
finales de línea y consulta el tercer renglón. El harness externo añade los aspectos que no deben
autoafirmarse: compara `nsc0`, cambia cwd, cruza O0/O2, parsea JSON y recalcula FNV-1a en Python.

## 9. Pruebas específicas

| Prueba | Propósito | Resultado previo al baseline |
|---|---|---|
| Bootstrap Profile | impedir construcciones circulares | PASS, 7 fuentes, 0 violaciones |
| compilación O0/O2 | ejecutable semilla y optimizado | PASS × 2 |
| self-test Basic/Source/Diagnostics | invariantes internos | PASS, retorno 42 × 2 |
| PE verifier | artefacto Windows válido | PASS × 2 |
| archivo ausente vs nsc0 | texto/código 1 | PASS × 2 |
| opción desconocida vs nsc0 | texto/código 2 | PASS × 2 |
| mixed endings | LF, line map y hash | PASS, 19 bytes/4 líneas |
| cwd diferencial | paths no dependen de ejecución | PASS × 2 cwd |
| diagnóstico JSON | schema/código/path | PASS × 2 |
| Python selfhost | contratos, mapa y FNV cruzado | PASS, 26/26 |

## 10. Comparación diferencial

La autoridad de SH04 no es todavía la semántica de lenguaje. Se comparan sólo comportamientos que
ambos ejecutables implementan: archivo ausente y opción desconocida. Para éstos, stdout, stderr y
exit code deben ser idénticos. La identidad dice deliberadamente `nsc-next`, no intenta hacerse
pasar por `nsc0`; ayuda y dumps son contratos nuevos versionados.

O0 y O2 se comparan por JSON parseado y efectos, no por bytes PE. Cada matriz del baseline repite
el gate con su linker configurado, por lo que el mismo proyecto queda cubierto por lld-link y
NSLink sin incorporar lógica de linker al frontend.

## 11. Baseline de cierre

Comando canónico ejecutado después del último cambio funcional:

```text
python -B nscbuilder.py --baseline --config selfhost/bootstrap/bootstrap.config.example.json --run-id tanda-05-sh04-final-20260919 --jobs 16
```

Resultado global: `PASSED`, cero fallos, 476.424 segundos.

| Matriz/prueba | Resultado |
|---|---:|
| CTest con `lld-link` | 441/441 PASS |
| CTest con NSLink | 441/441 PASS |
| fixtures diferenciales | 296/296 PASS (148 por linker) |
| triage diferencial | 36/36 PASS (18 por linker) |
| gates self-hosting acumulativos | 12/12 PASS (6 por linker) |
| Python selfhost | 26/26 PASS |
| Gate SH04 | PASS O0/O2 en ambas matrices |

| Artefacto | SHA-256 |
|---|---|
| `report.md` | `63fe1569d35eb381b4f6ef536b6807deeb7448dc08aad894d9a69090cd0578e4` |
| `report.json` | `d2686299e4bc449033077a1534a72a98247f6b4cea01e4bc0ee497e59b4235bd` |
| digest de fuentes, 1385 archivos | `ed4341255a241203e8b7f8d1b1f206f5f90bdc4d4055fdbba409ec407fda8806` |
| `nsc0` construido/instalado | `d2eeb7abcadb802a1bf2802a0fef148d62f666bfa6e53ee70ec02b5c288f6561` |
| Gate G00 | `cd74a680fb6cad7e89f4e562c25cdeab0beac802580cdcfb527dc57d21762836` |
| reporte SH04 | `5aebb4e930614ed2d1648f8440a6934120f86b6c203a76e640391dc0e443bdd6` |
| `nsc-next` O0, matriz lld-link | `6f3e3c87076758461f104bfc9de8088dd3d5e173f795f29a0d2e9e2c25a5f137` |
| `nsc-next` O2, matriz lld-link | `74c5766f6622cc11d93e7fe6c3e4cf6669397e30cb070850bdaa452cde0682aa` |

Evidencia normativa: `selfhost/evidence/runs/tanda-05-sh04-final-20260919/` y
`selfhost/evidence/gates/SH04/`. No hubo corridas finales fallidas; las dos correcciones de
compilación descritas en la sección 8 ocurrieron antes del baseline de cierre.

## 12. Riesgos, limitaciones y deuda

- `SourceBuffer.Load` presupone que Driver verificó existencia; la API de IO actual no transporta
  un error rico;
- el hash usa bytes de `char` NativeSharp; Unicode válido se conserva, pero clasificación Unicode
  corresponde al lexer SH05;
- el catálogo contiene dos diagnósticos: cada nueva rebanada debe ampliar mapa, renderer y prueba;
- el path humano conserva exactamente el spelling de argv para igualar `nsc0`; sólo JSON normaliza;
- no hay timeout/cancelación ni compilación real; el ejecutable es deliberadamente un esqueleto;
- `--self-test-basic` es superficie de gate y podrá retirarse cuando existan unit tests compilados
  directamente por `nsc-next`.

## 13. Contenido del commit

### Higiene

- [x] No contiene `.obj`, `.exe`, `.dll`, `.lib`, PDB, temporales ni cachés no normativos.
- [x] No contiene cambios locales ajenos a la tanda.
- [x] El manifiesto de fuentes está actualizado cuando corresponde.
- [x] Métodos, funciones, archivos y decisiones están documentados.
- [x] El baseline se ejecutó después del último cambio funcional.

### Commit de cierre

```text
selfhost(tanda-05): bootstrap nsc-next basic services
```

## 14. Criterio de cierre

- [x] `NS2-SH-045` a `NS2-SH-054` implementados.
- [x] Gate específico verde O0/O2 y diferencial contra `nsc0`.
- [x] Baseline completo verde con lld-link y NSLink.
- [x] Gate SH04 `LANDED`, enlazado a SH03 y sin problemas.
- [x] Expediente, manifiesto, evidencia y commit coinciden.
