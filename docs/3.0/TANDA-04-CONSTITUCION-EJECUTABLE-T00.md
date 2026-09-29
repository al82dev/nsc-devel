# NativeSharp 3.0 — Tanda 04: Constitución ejecutable T00

Fecha de cierre: 29 de septiembre de 2026

Rama: `development`

Plataforma normativa: Windows x64

Configuración de construcción: Debug/O0

Estado al cerrar el commit: `TARGETED-GREEN`; pendiente del baseline integral que ejecutará Alberto

## 1. Objetivo

La implementación original de T00 describía la constitución del lenguaje, pero varias piezas seguían siendo
auxiliares aislados. Esta tanda conecta esos contratos al compilador productivo para que las ediciones, las etapas,
las categorías de valores, los efectos, los diagnósticos y el harness de conformidad dejen de ser declaraciones y
formen parte de la ejecución real de NSC.

El alcance de cierre fue:

- hacer que el pipeline real registre las siete etapas normativas en orden;
- transportar la edición desde `.nsproj` hasta proyecto, frontend, MIR, cache, NSMETA y LSP;
- impedir que `ProjectReference` o `LibraryReference` mezclen ediciones silenciosamente;
- hacer que las categorías 3.0 gobiernen decisiones reales de lowering HIR;
- usar `EffectContext` en el control productivo de `noheap`;
- estabilizar los códigos `NS3010`, `NS3020` y `NS3030` en las fronteras afectadas;
- ejecutar manifests normativos contra el `nsc.exe` construido, no sólo autochecks de Python;
- conservar el Native ABI cuando únicamente cambia la edición del lenguaje.

No se añadió sintaxis nueva. T00 define y conecta las reglas que las tandas T01–T17 deberán obedecer.

## 2. Decisiones de diseño

### 2.1 Etapas productivas

`ProjectCompilation` es la autoridad de la secuencia:

1. `Parse` se completa después del parse exitoso de todas las unidades.
2. `Binding` se completa después de construir `ProjectNameLookup`.
3. `TypeChecking` se completa después de los cuerpos semánticos.
4. `Comptime` se completa en la frontera semántica posterior al type checking.
5. `Mir` se completa después de construir el MIR OOP.
6. `Monomorphization` se completa cuando ese MIR contiene el catálogo especializado listo para backend.
7. `Codegen` se completa sólo después de producir objetos de máquina.

La transición se hace mediante `CompilationStageTracker`. Un salto, una repetición o un productor ausente genera
`NS3010`. La emisión directa de `.obj`, `.exe`, `.lib` y `.dll` pasa por la misma frontera final; ninguna de esas
rutas puede declarar codegen sin que las seis etapas previas hayan sido registradas.

### 2.2 Edición como identidad semántica

La edición es semántica, no decorativa:

- `Project` y `ProjectSyntaxUnit` ya la conservaban; ahora el `ProjectOopMirModel` también la conserva.
- `ProjectOopMirCacheIdentity` incorporó el fingerprint de edición y elevó su esquema interno a v2.
- cambiar `<Language>2</Language>` por `<Language>3</Language>` invalida el objeto incremental aunque la fuente sea
  byte a byte idéntica.
- NSMETA publica la edición a partir del formato 1.23.
- un módulo precompilado conserva la edición leída de NSMETA al convertirse en `PrecompiledSyntaxModule` y en nodo
  sintético de `ProjectGraph`.
- un consumidor de otra edición es rechazado antes de materializar su API pública mediante `NS3001`.
- los snapshots del LSP publican la edición analizada y su caché de último modelo válido se separa por ruta y
  fingerprint de edición.

La edición no altera automáticamente el ABI nativo. Por eso se excluye deliberadamente de la proyección usada para
calcular el `abiFingerprint`: dos ediciones pueden describir exactamente el mismo layout/ABI y compartir ese
fingerprint, aunque no puedan mezclarse dentro de un mismo grafo de compilación.

### 2.3 Compatibilidad de NSMETA

El formato actual pasa de 1.22 a 1.23 y añade un byte de edición después del target triple.

- 1.23 acepta únicamente los valores 2 y 3.
- los formatos 1.0–1.22 se interpretan como edición 2.
- intentar escribir una edición 3 en un minor histórico se rechaza; no se crea un archivo que mienta sobre su
  contrato.
- el merger exige que fragmentos de una misma biblioteca tengan la misma edición.
- el read-back del productor verifica el nuevo campo como parte del modelo completo.

### 2.4 Categorías de valor en HIR

`HirBuilder` deja de reducir todas las decisiones a `LValue/RValue`:

- `Place` y `BorrowedValue` siguen la ruta de dirección/almacenamiento mediante `isPlaceCategory`.
- `Value` y `Temporary` siguen la extracción de valor para acceso a miembros.
- `MovedFrom` se rechaza en la frontera real de HIR con `NS3020`; no puede convertirse accidentalmente en una carga
  válida.
- `SemanticDumper` imprime el nombre normativo completo de la categoría, de modo que la evidencia ya no pierde la
  diferencia entre borrowed, temporary y moved-from.

### 2.5 Efectos y `noheap`

El lowering OOP crea un `EffectContext` para el callable actual y consulta `Effect::NoHeap` antes de permitir
allocations y llamadas. Esto sustituye decisiones dispersas basadas solamente en booleanos por el modelo común que
usarán posteriormente `throws`, `async`, `unsafe` y `comptime`.

Los fallos productivos de `noheap`, incluidos override e implementación de interfaces, publican `NS3030`. Los textos
históricos se mantienen después del prefijo para no romper diagnósticos humanos ni pruebas que dependen de su detalle.

### 2.6 LSP consciente de edición

El Analysis ABI del servicio de lenguaje sube de 1 a 2 porque su identidad pública añade:

- edición predeterminada: 2;
- ediciones soportadas: `2,3`;
- edición del proyecto dentro de cada `AnalysisSnapshot`.

El último modelo semántico válido ya no se indexa sólo por ruta. La llave es
`ruta + languageEditionFingerprint`. Si el `.nsproj` cambia de edición mientras una fuente contiene errores, el LSP
no reutiliza un modelo producido bajo reglas diferentes.

### 2.7 Harness que ejecuta NSC real

`tools/ns3_conformance.py` conserva su autocheck y añade:

- placeholder `{compiler}` suministrado mediante `--compiler`;
- validación de que el ejecutable existe;
- expectativas `stdoutContains` y `stderrContains`;
- creación explícita de `--work-root`;
- ejecución de manifests productivos en directorios aislados.

Se añadieron dos contratos normativos:

- `t00.edition3.project-identity`: verifica que un `.nsproj` edición 3 publica `language: 3`.
- `t00.edition3.production-pipeline`: lleva una fuente edición 3 hasta MIR mediante el NSC construido.

CMake registra `nsc.ns3.t00.conformance-compiler` con `$<TARGET_FILE:nsc>`. El harness ya no puede declararse verde
solamente porque Python sea capaz de ejecutar Python.

## 3. Archivos modificados

### Construcción y registro de pruebas

- `CMakeLists.txt`
  - enlaza la prueba constitucional con `nsc_driver` para comprobar NSMETA real;
  - registra el test productivo del harness contra `$<TARGET_FILE:nsc>`.

### Contratos públicos

- `include/nativesharp/Compilation/ProjectGraph.h`
  - añade edición al descriptor de proyecto precompilado.
- `include/nativesharp/Frontend/ProjectCompilation.h`
  - añade edición al módulo precompilado;
  - expone finalización de codegen y etapas completadas;
  - incorpora el tracker productivo.
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
  - incorpora setter, getter y almacenamiento de edición.
- `include/nativesharp/Interop/LibraryMetadata.h`
  - eleva NSMETA a 1.23 y añade `languageEdition`.
- `include/nativesharp/LanguageService/LanguageService.h`
  - eleva Analysis ABI a 2;
  - publica ediciones predeterminada/soportadas y edición de snapshot.

### Pipeline y grafo

- `src/Compilation/ProjectGraph.cpp`
  - conserva la edición en nodos precompilados;
  - valida dueño y prohíbe mezcla con `NS3001`.
- `src/Frontend/ProjectCompilation.cpp`
  - registra Parse, Binding, TypeChecking, Comptime, MIR y Monomorphization;
  - transporta edición a descriptors y MIR;
  - implementa la frontera Codegen.
- `src/Driver/Driver.cpp`
  - convierte el byte NSMETA a edición del módulo;
  - completa Codegen en rutas `.obj`, `.exe`, `.lib` y `.dll`.
- `src/Frontend/ProjectOopMirModel.cpp`
  - implementa persistencia de edición en MIR.
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
  - incluye edición y versión de identidad v2.

### Semántica, HIR y diagnósticos

- `src/HIR/HirBuilder.cpp`
  - usa las categorías normativas y rechaza moved-from con `NS3020`.
- `src/Sema/SemanticDumper.cpp`
  - conserva nombres completos de categorías de valor.
- `src/Frontend/ProjectOopHirModel.cpp`
  - aplica `EffectContext` y `NS3030` en operaciones noheap.
- `src/Frontend/ProjectInterfaceModel.cpp`
  - etiqueta con `NS3030` los conflictos noheap heredados/de interfaz.
- `src/Frontend/ProjectVirtualDispatchModel.cpp`
  - etiqueta con `NS3030` la pérdida de contrato noheap en overrides.

### Metadata y tooling

- `src/Interop/LibraryMetadata.cpp`
  - construye, codifica, decodifica y valida edición 1.23;
  - conserva el ABI fingerprint independiente de edición.
- `src/Interop/LibraryMetadataMerger.cpp`
  - exige edición uniforme entre fragmentos.
- `src/LanguageService/LanguageService.cpp`
  - publica edición e impide fallback semántico cross-edition.
- `tools/ns3_conformance.py`
  - ejecuta un compilador explícito y soporta contains checks.

### Pruebas existentes reforzadas

- `tests/unit/NativeSharp3T00ConstitutionTests.cpp`
  - prueba etapas productivas, NSMETA 1.23 y metadata precompilada cross-edition.
- `tests/unit/LanguageServiceIdentityTests.cpp`
  - prueba Analysis ABI 2 y ediciones publicadas.
- `tests/unit/LanguageServiceSnapshotTests.cpp`
  - prueba snapshot edición 3 y aislamiento del fallback al cambiar a edición 2.
- `tests/unit/Item321NoHeapContextTests.cpp`
  - exige `NS3030` en allocations prohibidas.
- `tests/unit/Item322NoHeapPropagationTests.cpp`
  - exige `NS3030` en calls, constructors, overrides e interfaces.

## 4. Archivos nuevos y razón

- `tests/conformance/t00/Edition3.nsproj`
  - proyecto mínimo y explícito de edición 3 para el harness productivo.
- `tests/conformance/t00/Main.ns`
  - unidad fuente mínima sin dependencias externas.
- `tests/conformance/t00/edition3-project.json`
  - contrato auditable de identidad de proyecto/edición.
- `tests/conformance/t00/edition3-mir.json`
  - contrato auditable de tránsito edición 3 hasta MIR.
- `docs/3.0/TANDA-04-CONSTITUCION-EJECUTABLE-T00.md`
  - expediente completo de este commit, sus razones y evidencia.

No se creó ningún directorio Codex ni worktree.

## 5. Mejora obtenida

Antes de esta tanda, un cambio de edición podía perderse al importar NSMETA, compartir una entrada de cache o permitir
que el LSP reciclara semántica anterior. El stage tracker, `EffectContext` y categorías extendidas tenían pruebas
propias, pero no eran autoridades del pipeline.

Después de esta tanda:

- edición 2/3 es una identidad verificable de extremo a extremo;
- la cache incremental no confunde MIR de ediciones distintas;
- las bibliotecas declaran su edición y los consumidores incompatibles fallan temprano;
- el LSP no presenta semántica obsoleta de otra edición;
- todas las salidas de máquina registran el mismo orden normativo de etapas;
- moved-from y noheap tienen códigos estables en sus fronteras reales;
- el harness ejecuta el compilador que se pretende certificar.

Esto no hace que el resto de NativeSharp 3.0 esté terminado. Sí convierte T00 en una base ejecutable sobre la que se
pueden implementar las siguientes vertical slices sin duplicar versión, efectos, categorías, cache o conformidad.

## 6. Construcción y pruebas dirigidas

### Construcción

- `nsc` Debug/O0: correcta.
- construcción final después de los últimos diagnósticos: correcta.
- no se construyó O1, O2 ni O3.

### Ejecutables unitarios directos

| Prueba | Resultado |
|---|---:|
| `nsc_ns3_t00_constitution_tests` | 31/31 |
| `nsc_language_service_identity_tests` | 9/9 |
| `nsc_language_service_snapshot_tests` | 19/19 |
| `nsc_project_graph_tests` | 25/25 |
| `nsc_hir_tests` | 34/34 |
| `nsc_item321_noheap_context_tests` | 16/16 |
| `nsc_item322_noheap_propagation_tests` | 25/25 |
| **Total C++ dirigido** | **159/159** |

### Harness y CLI

- `tools/ns3_conformance.py --self-test`: 5/5.
- manifest `t00.edition3.project-identity`: passed.
- manifest `t00.edition3.production-pipeline`: passed.
- smoke CLI `--emit-project-oop-obj -O0`: passed; produjo COFF AMD64 válido.

La primera ejecución de Item322 desde `build-lsp-debug` descubrió un caché CMake antiguo que apuntaba a
`C:\Program Files (x86)\NativeSharp`. No fue un fallo del cambio y no se escribió nada en esa ruta. El caché local del
árbol se corrigió a `C:\native-sharp`, que es la instalación indicada por Alberto. La validación final de Item322 se
ejecutó completa y terminó 25/25.

## 7. Exclusiones y limpieza

- No se ejecutó baseline.
- No se ejecutó CTest completo.
- No se instalaron binarios.
- No se tocó ninguna remota.
- No se modificó `native-sharp-3.0`.
- No se escribió código fuente ni evidencia en C:.
- Se eliminaron `_tmp`, `error.log`, objetos, metadata y directorios de evidencia creados por los smoke tests.
- Los outputs normales dentro de los árboles CMake de A: permanecen como artefactos reconstruibles del build.

## 8. Riesgo pendiente

El baseline integral sigue siendo la única certificación acumulada. Alberto lo ejecutará al final de las tandas. Hasta
entonces este commit es `TARGETED-GREEN`, no `BASELINE-CERTIFIED` ni `LANDED`.
