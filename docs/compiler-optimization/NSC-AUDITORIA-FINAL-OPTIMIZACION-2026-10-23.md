# NSC — Auditoría final de optimización, organización y seguimiento de los 670 renglones

**Fecha:** 2026-10-23
**Rama y revisión auditadas:** `development-claude-incremental-compilation` en `b822a92b331a0393cfd7c64aaad151cb6b28aec3`
**Documento contrastado:** `3.0.6-performance/NSC-PENDIENTES-Y-TERMINADOS-RENDIMIENTO-SOBRECALCULADO.md`
(auditado originalmente sobre `development` en `5b6582648661fdf78c32d267dca900edca1fffa5`)
**Documentos previos de esta rama:** `NSC-COMPILACION-INCREMENTAL-AUDITORIA-2026-10-22.md`,
`3.0.6-performance/NSC-PENDIENTES-Y-TERMINADOS-RENDIMIENTO.md` (T1–T7, 48/48),
`3.0.6-performance/NSC-PLAN-IMPLEMENTACION-COMPILACION-FRIA-O0.md` (59/60)
**Tipo de trabajo:** sólo auditoría. No se cambió código.

## 0. Resumen ejecutivo

1. De los 670 renglones del tracking sobrecalculado, **197 ya están resueltos** en esta rama y **28 están superados**
   por otro diseño. Además, **1 no aplica**, **51 están resueltos en parte** y **46 tienen la implementación hecha
   pero desconectada**. **249 siguen aplicando**, 29 sólo esperan cobertura focal, 40 son validación del usuario
   y 29 no se pudieron verificar con certeza. Detalle en las secciones 2 y 7.
2. Hay **dos cachés implementadas y apagadas**. HECHO:
   - Caché MIR (C04): `setMirCacheRoot` no tiene ningún llamador.
   - Caché de plantillas (C01): `ProjectGenericModel::setTemplateCacheRoot` no tiene ningún llamador.

   El código consumidor de las dos existe y está cableado, pero nunca se habilita. La llave MIR además incluye
   el volcado completo del HIR del programa, así que, aun reconectada, cualquier cambio invalidaría todas las
   funciones (N-01, N-02).
3. **Hallazgo de corrección con impacto en rendimiento.** Todo programa compilado con `debug=full` incrusta una
   traza de runtime encendida (`flags = 7`) que escribe en la ruta fija `A:\temp\nsc-runtime-debug-trace.log`
   (N-04). En otra máquina esa ruta no existe o no es del usuario.
4. **El runtime del ejecutable se reconstruye y se optimiza con LLVM en cada enlace**, incluso cuando el enlace
   incremental va a dar hit: la llave se calcula después de construirlo (N-03). La caché de `runtime.obj` existe
   sólo en el constructor de bibliotecas.
5. **Organización.** Tres puntos principales:
   - `TemplateParseCache` se usa como almacén genérico (la caché MIR es un `TemplateParseCache`) y 20
     encabezados `ProjectOopMir*.h` lo incluyen sin usarlo.
   - Las capas `Compilation`/`Frontend` dependen de clases del namespace `driver`.
   - Hay 151 marcadores de código de instrumentación, casi todos `#if 0`, y archivos de 5,000 a 7,400 líneas.

   Ver sección 4.
6. Lo que más rendiría, en orden, está en la sección 6.

## 1. Alcance, método y etiquetas

- Todo se verificó contra el disco de `A:\nsc-devel` en la revisión indicada, con búsquedas `git grep` y lectura
  de rangos de líneas mediante scripts de solo lectura en `A:\temp\nsc-audit`, fuera del repositorio.
- **No se compiló ni se ejecutó ninguna prueba para esta auditoría.** Las cifras de tiempo citadas vienen de la
  auditoría incremental del 2026-10-22 y se marcan así.
- Etiquetas: **HECHO** (visto en el código o medido), **INFERENCIA** (deducido del código, sin ejecutar),
  **NO VERIFICADO**.
- Estados usados en la sección 7:

| Estado | Significado |
|---|---|
| RESUELTO | El código vigente ya elimina el costo descrito; se cita archivo y línea. |
| APLICA | El costo descrito sigue presente. |
| PARCIAL | Una parte quedó resuelta y otra no; la evidencia dice cuál. |
| DESCONECTADO | La implementación existe pero nunca se activa en el compilador. |
| SUPERADO | El problema desapareció por otro diseño (p. ej. productos publicados en NSMETA). |
| NO APLICA | El supuesto del renglón no se cumple en el código. |
| COBERTURA PENDIENTE | La implementación está; falta la prueba o el target focal pedido. No se encontró target específico. |
| VALIDACIÓN DEL USUARIO | Medición o validación que corresponde al usuario. |
| NO VERIFICADO | No se pudo confirmar con la evidencia reunida. |

## 2. Conteos

| Estado | Renglones |
|---|---:|
| RESUELTO | 197 |
| SUPERADO | 28 |
| NO APLICA | 1 |
| PARCIAL | 51 |
| DESCONECTADO | 46 |
| APLICA | 249 |
| COBERTURA PENDIENTE | 29 |
| VALIDACIÓN DEL USUARIO | 40 |
| NO VERIFICADO | 29 |
| **Total** | **670** |

Por tanda del tracking (abreviaturas: Res resuelto, Sup superado, N/A no aplica, Par parcial, Desc desconectado,
Apl aplica, Cob cobertura pendiente, Usr validación del usuario, N/V no verificado):

| Tanda | Res | Sup | N/A | Par | Desc | Apl | Cob | Usr | N/V | Total |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0 | 0 | 0 | 0 | 0 | 8 | 0 | 14 | 0 | 22 |
| 1 | 10 | 0 | 0 | 0 | 0 | 3 | 7 | 0 | 0 | 20 |
| 2 | 62 | 0 | 0 | 0 | 0 | 16 | 6 | 0 | 0 | 84 |
| 3 | 23 | 0 | 0 | 2 | 0 | 2 | 2 | 0 | 1 | 30 |
| 4 | 0 | 0 | 0 | 0 | 0 | 9 | 0 | 0 | 0 | 9 |
| 5 | 14 | 0 | 0 | 1 | 0 | 9 | 2 | 0 | 2 | 28 |
| 6 | 55 | 0 | 0 | 12 | 0 | 28 | 5 | 0 | 0 | 100 |
| 7 | 13 | 0 | 0 | 0 | 0 | 18 | 0 | 0 | 0 | 31 |
| 8 | 7 | 0 | 0 | 0 | 0 | 15 | 1 | 1 | 0 | 24 |
| 9 | 0 | 0 | 0 | 29 | 0 | 0 | 0 | 1 | 0 | 30 |
| 10 | 0 | 28 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 30 |
| 11 | 0 | 0 | 0 | 0 | 23 | 0 | 0 | 2 | 0 | 25 |
| 12 | 0 | 0 | 0 | 0 | 23 | 0 | 0 | 2 | 0 | 25 |
| 13 | 7 | 0 | 0 | 0 | 0 | 30 | 2 | 1 | 2 | 42 |
| 14 | 6 | 0 | 1 | 7 | 0 | 109 | 4 | 1 | 24 | 152 |
| 15 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 16 | 0 | 18 |

Nota: el tracking original marcaba 0 terminados. Esta auditoría no determina en qué commit se resolvió cada
renglón; sólo constata el estado en esta rama. Buena parte de los resueltos corresponde a T1–T7 y al plan de
compilación fría O0.

## 3. Hallazgos nuevos de rendimiento

Los hallazgos de esta sección no están en el tracking de 670 renglones o lo amplían.

### N-01. Caché MIR desconectada y con llave de programa completo — HECHO

- `ProjectOopMirModel::setMirCacheRoot` (`src/Frontend/MIR/MirCache.cpp:1460`) no tiene llamadores en `src/`.
  `mirCache_.enabled()` es falso siempre.
- Por eso `calculateMirCacheKeys` sale de inmediato (`:1471`) y `lookupMirCachePayloads`,
  `installRestoredMirFunctions` y `storeMirFunctions` no hacen nada útil. Sus llamadas sí existen:
  - `MirBuildCoordinator.cpp:713-714,762`;
  - `MirFunctionLowerer.cpp:879`.
- La llave global mete `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`). Eso implica
  dos cosas:
  - el costo de volcar todo el HIR en cada build, si se reconecta;
  - un cambio en cualquier función invalida todas las demás.
- El renglón PERF-0305..0309 (llave calculable antes de construir MIR, por función) sigue siendo el diseño correcto.
- Acción: no reconectar tal cual. Primero llave por función, con la huella del HIR de esa función más las
  identidades de los modelos que realmente consulta.

### N-02. Caché de plantillas (C01) desconectada — HECHO

- `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores.
  `ProjectCompilation::setTemplateCacheRoot` (`ProjectCompilation.cpp:49`), invocado desde `DriverExecution.cpp:48`,
  no la propaga al modelo genérico.
- Los consumidores de `TemplateCacheBridge.cpp` y `SpecializationExecutor.cpp:264-469` quedan inertes.
- Corresponde a la desconexión registrada en cf0a94725; los renglones PERF-0279..0303 quedan en DESCONECTADO.

### N-03. Runtime del ejecutable reconstruido en cada enlace — HECHO

- `WindowsExecutableBuilder::buildObjects` llama `buildExecutableRuntime` y `optimizeExecutableRuntime`
  (`WindowsExecutableBuilder.cpp:587-590`) **antes** de `prepareIncrementalExecutable` (`:593-597`).
- Consecuencias:
  - con `--incremental-link` y hit, el runtime se construyó y optimizó para nada;
  - sin hit, se emite `runtime.obj` nuevo (`:545-566`) porque el constructor de ejecutables no tiene caché de
    `runtime.obj`; el de bibliotecas sí (`WindowsLibraryBuilder.cpp:547-585`).
- Acción: llave de runtime calculable sin construir el módulo (versión del generador, símbolo de entrada, perfil
  de pánico, nivel de optimización, debug), y reutilizar la caché de `WindowsLibraryBuilder`.

### N-04. Traza de runtime encendida en todo programa con `debug=full` — HECHO (corrección y rendimiento)

- Con `debug=full`, `ensureRuntimeDebugTrace` (`OopLlvmRuntimeDebugTrace.cpp:11-59`) hace dos cosas:
  - declara la función de traza y el global de banderas con valor 7 (entradas, asignación/liberación y stores
    de campo);
  - incrusta la ruta `A:\temp\nsc-runtime-debug-trace.log`.
- INFERENCIA: un programa del usuario compilado en debug completo escribe a esa ruta en cada llamada, asignación y
  store instrumentados. En otra máquina la ruta no existe o apunta a un disco ajeno.
- Es la decisión pendiente de PERF-0212..0218, que en el tracking figura como "requiere autorización".
- Acción sugerida: dejar `flags = 0` por defecto y exponer la ruta y las banderas como opción explícita del
  compilador.

### N-05. Caché pre-LLVM apagada con `debug=full` y con ThinLTO — HECHO

- `usePreLlvmCache` exige `debugInformation != Full` y LTO distinto de Thin (`ProjectOopObjectGenerator.cpp:1011-1013`).
- Para el flujo típico de desarrollo (debug completo) toda unidad se baja, optimiza y emite aunque no haya cambiado.
- Es el grupo PERF-0473..0479. La auditoría incremental del 2026-10-22 ya lo había señalado.

### N-06. SHA-256 de cada fuente repetido por unidad de código — HECHO

- `createDebugFile` (`Support/DebugInfoEmission.cpp:111-148`) calcula `sha256` del archivo fuente completo y lo
  guarda en `debugFiles`. Ese mapa es por unidad LLVM.
- Con N unidades que tocan el mismo archivo, ese archivo se hashea N veces. Es PERF-0160/0161.
- Acción: precalcular el checksum por `fileId` una vez en el generador y pasarlo a las unidades.

### N-07. Canonicalización de rutas repetida en la carga — HECHO

| Sitio | Costo |
|---|---|
| `ProjectSourceSet::add` (`ProjectSourceSet.cpp:22-29,56-63`) | Canonicaliza el directorio del proyecto por fuente; duplicado O(n²) recalculando `comparisonKey`. |
| `ProjectSourceSet::finalize` (`:86-98`) | Recalcula dos claves de texto por comparación. |
| `normalizeTransitiveSourceOwnership` (`ProjectGraphLoader.cpp:146-165`) | Tres canonicalizaciones por fuente. |
| `preferExactSourceModuleArtifacts` (`:207-227`) | Canonicaliza dentro de `any_of` y en dos comparadores. |
| `precompiledProjectOrdinal` (`ProjectCompilation.cpp:1341-1358`) | Dos `weakly_canonical` por proyecto por módulo precompilado (P×M). |
| `buildVisibility` (`TypeSpellingAndVisibility.cpp:398-423`) | `weakly_canonical` + minúsculas por referencia por visita, con recursión en `std::function`. |
| `ProjectGraph` (`ProjectGraph.cpp:461-463`) | `canonicalWindowsModuleIdentity` dentro de un comparador. |

INFERENCIA: en Windows cada `weakly_canonical` toca el sistema de archivos. Con decenas de proyectos y cientos de
fuentes esto se nota en el arranque. Acción: una sola tabla `ruta → clave canónica` por carga de grafo.

### N-08. Copias evitables en el cierre binario — HECHO

`expandBinaryClosure` copia el vector `binaryDependencies` completo de cada nodo para que `loaded` pueda crecer sin
invalidar referencias (`PrecompiledProjectPreparation.cpp:633-634`). Reservar y trabajar con ordinales lo evita
(PERF-0466/0467).

### N-09. `nsc build` no acepta `--time` ni `--time-trace` ni prefix map — HECHO

- `parseBuildOptions` rechaza toda opción desconocida (`ProjectCommands.cpp:1025-1028`).
- `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`).
- El lanzador `src/nsc/main.cpp` convierte `--time-trace=` en `--time` (`:267,302`). INFERENCIA, no ejecutado:
  `nsc build --time-trace=x` termina con "unknown build option".
- Es PERF-0195..0204 y PERF-0452. Sin esto no se puede medir por fases un build de proyecto desde el IDE.

### N-10. Poda de la caché en cada invocación — HECHO

- Con caché por defecto, `pruneInvocationArtifactCache` (`CompilerSession.cpp:78-96`) llama `prune`, que inspecciona
  todas las entradas (`CompilationArtifactCache.cpp:522-575`).
- No hay sello de última poda. Es PERF-0355..0362.

### N-11. Hallazgos medidos de la auditoría incremental que siguen abiertos

Fuente: `NSC-COMPILACION-INCREMENTAL-AUDITORIA-2026-10-22.md`. Las cifras son de esa medición, no de esta auditoría.

| Id | Estado | Resumen |
|---|---|---|
| H4 | Abierto | El número de unidades de código depende de los hilos en O1–O3, así que cambiar `-j` invalida objetos. |
| H8 | Abierto | Un hit de bundle reimprime las líneas por unidad. |
| H9 | Abierto | Copia y verificación COFF por hit, unos 14 ms por unidad (medido). |
| H10 | Abierto | Sin reutilización de frontend: pico de 4.6 a 5.2 GB (medido). |
| H11 | Abierto | `oop-hir` de Math.Core cerca de 1 s (medido). |
| H12 | Abierto | Regresión `void*[]`; NativeForms falla. |
| `other` | Abierto | Parte del tiempo de pared queda sin instrumentar. |
| L0b, L4–L6 | Abiertos | Capas de reutilización propuestas en esa auditoría. |

### N-12. Linealidades vigentes de prioridad media

Siguen en el código y están en el tracking (sección 7):

- HIR `isStructValueType`/`isClassReferenceType`/`isReferenceType` y `isEnumBinaryTypeKey`;
- MIR `lowerGroup21`, ABI por span y frames async O(F²);
- LLVM `isExternCDelegateType`/`isVectorCallDelegateType`;
- `resolveDeclaredTypeIdentity`;
- verificación NSMETA (`findMetadataDispatch*`, cierres de plantillas sin memo, diagnósticos construidos antes de
  fallar);
- ownership (`conflictingMutableReference`, `hasActiveBorrowOfOwnerLocal`);
- metaprogramación (`findAttributeTarget`, `findType`);
- `resolveNameOfTemplateGenericMethod`, `findTraitImplementationOwner`, `resolveMemberPhysicalIdentity`.

Todas siguen el mismo remedio ya probado en T5: índice compartido en `ProjectCompilationIndexes` con fallback lineal
para modelos armados a mano.

## 4. Organización y mantenibilidad

### O-01. `TemplateParseCache` como almacén genérico — HECHO

- La caché MIR es un `TemplateParseCache mirCache_` (`ProjectOopMirModel.h:492`), igual que `PrecompiledProductCache`
  y `precompiledSyntaxCache_`.
- 20 encabezados `include/nativesharp/Frontend/ProjectOopMir*.h` incluyen `TemplateParseCache.h` sin usarlo.
  Cualquier cambio en ese encabezado recompila todo MIR.
- Acción:
  - renombrar a un almacén neutro (p. ej. `PersistentPayloadCache`);
  - dejar el include sólo donde se usa.

### O-02. Inversión de capas `Compilation`/`Frontend` → `driver` — HECHO

- `ProjectGraphLoader.cpp:8` incluye `nativesharp/Driver/CompilationWorkerPool.h`.
- `ProjectCompilationLifetime.h:4-5` incluye `CompilationGraphScheduler.h` y `CompilationWorkerPool.h`.
- El arreglo H13 (89a0550bb) movió `CompilationGraphScheduler.cpp` a `nsc_compilation` para cerrar el enlace, pero el
  namespace y la ruta siguen diciendo `driver`.
- Acción: mover pool y scheduler a una capa de concurrencia bajo `basic` o a una biblioteca propia, con su
  namespace.

### O-03. Hash transparente triplicado — HECHO

`ProjectCompilationIndexes.h:157`, `ProjectMemberModel.h:163` y `Parser/KnownTypeNameSet.h:12` definen el mismo hash.
Un único `basic::TransparentStringHash` basta (PERF-0017/0018).

### O-04. Rutas fijas a `A:\temp` en el código — HECHO

| Sitio | Estado |
|---|---|
| `OopLlvmRuntimeDebugTrace.cpp:39` | **Activa** (N-04). |
| `ColdLlvmTrace.cpp:27` | Apagada por `constexpr`. |
| `OopLlvmImplementation.cpp:19` | Apagada por `constexpr`. |
| `MethodBodyInternals.h:15` | Apagada por `constexpr`. |
| `CompilerSession.cpp:9` | Dentro de `#if 0`. |
| `DriverHelp.cpp:255` | Dentro de `#if 0`. |

Acción: un solo directorio de trazas configurable.

### O-05. Instrumentación aparcada — HECHO

- Hay 151 líneas con `INSTRUMENTATION CODE` en `src/` e `include/`, casi todas bloques `#if 0`.
- `LlvmInstrumentationSwitches.h` muestra el patrón bueno: interruptores `constexpr` centralizados.
- Acción: migrar los `#if 0` a interruptores o borrarlos (requiere autorización, PERF-0206..0211 ya cumplidos por
  los interruptores).

### O-06. Archivos y encabezados enormes — HECHO

| Archivo | Tamaño | Líneas |
|---|---:|---:|
| `src/Runtime/WindowsRuntimeBuilder.cpp` | 389 KB | 7,400 |
| `src/Frontend/Bodies/StatementAnalyzer.cpp` | 282 KB | 6,219 |
| `src/Frontend/Bodies/CallAnalyzer.cpp` | 279 KB | 5,798 |
| `src/LLVM/Lowering/OopLlvmExpressionMachine.cpp` | 220 KB | 4,746 |
| `include/nativesharp/Frontend/ProjectMethodBodyModel.h` | 237 KB | 5,216 |
| `include/nativesharp/Frontend/ProjectOopHirModel.h` | 206 KB | 4,346 |
| `src/LLVM/Internal/OopLlvmImplementation.h` | 161 KB | 3,326 |
| `tests/unit/ProjectOopLoweringTests.cpp` | 1.08 MB | 21,854 |

INFERENCIA: los dos encabezados de modelo se incluyen en decenas de unidades y dominan el tiempo de compilación del
propio compilador. Partirlos por responsabilidad (consultas de tipo, ownership, llamadas, etc.) y separar
`ProjectOopLoweringTests.cpp` por fase reduce recompilación y tiempo de enlace de pruebas.

### O-07. Dos `TypeIdentity.cpp` — HECHO

`src/Compilation/TypeIdentity.cpp` y `src/Frontend/MemberModel/TypeIdentity.cpp` (`CMakeLists.txt:289,507`). El
nombre repetido confunde búsquedas y perfiles; conviene renombrar el segundo según lo que hace.

### O-08. Volumen de documentación — HECHO

- `docs/` tiene 2,084 archivos y 37 MB.
- Incluye inventarios generados de varios MB (`symbols.csv` 6.9 MB, `commits.jsonl` 4.4 MB,
  `test-case-design.csv` 3.3 MB).
- Sugerencia: mover inventarios generados fuera del árbol o regenerarlos bajo demanda; dejar en `docs/` sólo
  documentos de decisión.

## 5. Proceso y pruebas

| Id | Etiqueta | Hallazgo |
|---|---|---|
| T-01 | HECHO | Dos fuentes de prueba no compilan: `NativeSharp3T03SumTypeTests.cpp` (`MatchArmCoverage` ganó `listLengths`) y `NativeSharp3T10AwaitProtocolTests.cpp` (inicializador desactualizado). |
| T-02 | HECHO, medido el 2026-10-22 | La suite completa da 387 aprobadas, 297 fallidas y 2 sin correr, igual en base y rama. Las 297 son de entorno: SDK no instalado en `C:/Program Files (x86)/NativeSharp` y bibliotecas esperadas en `A:\nsc-devel\lib`. |
| T-03 | HECHO, causa NO VERIFICADA | Algunas pruebas corridas en su lugar desde `A:\nsc-devel\build-incremental` terminan al inicio con `0xC0000409`. |
| T-04 | HECHO | No hay targets focales de rendimiento para identidades, SHA-256, índices de layouts, miembros, superficie pública o NSMETA. Los renglones "registro del target focal" quedan en COBERTURA PENDIENTE. |
| T-05 | HECHO | No existe el oráculo R0 (revisión, comandos, máquina, manifiestos) que el tracking pide en la tanda 0. Sin él, ningún renglón de la tanda 15 puede cerrarse. |
| T-06 | HECHO | La regla de `nsexec` exige ruta completa del ejecutable y los LINEAMIENTOS piden usar PATH. Ya se documentó en la auditoría incremental. |

## 6. Prioridades recomendadas

| # | Acción | Renglones | Por qué primero |
|---:|---|---|---|
| 1 | Apagar por defecto la traza de runtime de `debug=full` y quitar la ruta fija | PERF-0212..0218, N-04 | Afecta programas del usuario, no sólo al compilador. |
| 2 | Permitir `--time`, `--time-trace` y `-fdebug-prefix-map` en `nsc build` | PERF-0195..0204, 0452 | Sin medición por fases no se puede priorizar lo demás. |
| 3 | Oráculo R0 mínimo: revisión, tres comandos, cinco muestras | PERF-0175..0194 | Es el gate de toda la tanda 15. |
| 4 | Caché de runtime para ejecutables y llave antes de construir | PERF-0219..0233, 0480..0486, N-03 | Ahorra la optimización del runtime en cada enlace. |
| 5 | Caché pre-LLVM con `debug=full` | PERF-0473..0479 | Es el modo diario de desarrollo. |
| 6 | Checksum de fuentes por archivo, no por unidad | PERF-0160/0161 | Cambio local, sin riesgo de formato. |
| 7 | Rediseñar la llave MIR por función y reconectar; reconectar C01 | PERF-0279..0328 | Hoy son código muerto con costo de mantenimiento. |
| 8 | Tabla única de rutas canónicas en la carga | PERF-0083..0086, 0103, N-07 | Arranque más rápido en grafos grandes. |
| 9 | Índices restantes (HIR, LLVM delegados, MIR frames, NSMETA) | Sección 7, APLICA | Mismo patrón que T5. |
| 10 | Reorganización: O-01, O-02, O-03 y partir los encabezados de modelo | Sección 4 | Baja el costo de compilar el compilador. |

## 7. Seguimiento de los 670 renglones

La columna "Estado original" copia el tracking. La evidencia cita la revisión auditada.

| Número | Qué resuelve | Estado original | Estado en esta rama | Evidencia |
|---|---|---|---|---|
| PERF-0001 | Reconstrucción repetida del nombre completo de los tipos | PENDIENTE | RESUELTO | `TypeIdentity::key()`/`fullyQualifiedName()` son accesores de 4 líneas sobre valores ya calculados (`src/Compilation/TypeIdentity.cpp:127-135`). |
| PERF-0002 | Reconstrucción repetida de la clave de los tipos | PENDIENTE | RESUELTO | `TypeIdentity::key()`/`fullyQualifiedName()` son accesores de 4 líneas sobre valores ya calculados (`src/Compilation/TypeIdentity.cpp:127-135`). |
| PERF-0003-A | Copia innecesaria al consultar el nombre completo de un tipo | PENDIENTE | RESUELTO | `TypeIdentity::key()`/`fullyQualifiedName()` son accesores de 4 líneas sobre valores ya calculados (`src/Compilation/TypeIdentity.cpp:127-135`). |
| PERF-0003-B | Copia innecesaria al consultar la clave de un tipo | PENDIENTE | RESUELTO | `TypeIdentity::key()`/`fullyQualifiedName()` son accesores de 4 líneas sobre valores ya calculados (`src/Compilation/TypeIdentity.cpp:127-135`). |
| PERF-0004-A | Cobertura de equivalencia de identidades cacheadas | PENDIENTE | COBERTURA PENDIENTE | No existe un target focal de identidades; `TypeIdentity.cpp` sólo aparece como fuente de biblioteca en `CMakeLists.txt:289`. |
| PERF-0004-B | Registro del target focal de identidades cacheadas | PENDIENTE | COBERTURA PENDIENTE | No existe un target focal de identidades; `TypeIdentity.cpp` sólo aparece como fuente de biblioteca en `CMakeLists.txt:289`. |
| PERF-0005 | Duplicación del procesamiento interno de bloques SHA-256 | PENDIENTE | RESUELTO | `sha256` (7 líneas) y `sha256File` (37 líneas, lectura por bloques) en `src/Basic/Sha256.cpp:127-171`. |
| PERF-0006 | Copia y relleno completos de entradas SHA-256 en memoria | PENDIENTE | RESUELTO | `sha256` (7 líneas) y `sha256File` (37 líneas, lectura por bloques) en `src/Basic/Sha256.cpp:127-171`. |
| PERF-0007 | Lectura byte a byte durante SHA-256 de archivos | PENDIENTE | RESUELTO | `sha256` (7 líneas) y `sha256File` (37 líneas, lectura por bloques) en `src/Basic/Sha256.cpp:127-171`. |
| PERF-0008 | Equivalencia del SHA-256 optimizado en memoria | PENDIENTE | COBERTURA PENDIENTE | No hay target focal de SHA-256 registrado en `CMakeLists.txt` (sólo la fuente en la línea 315). |
| PERF-0009-A | Equivalencia del SHA-256 optimizado sobre archivos | PENDIENTE | COBERTURA PENDIENTE | No hay target focal de SHA-256 registrado en `CMakeLists.txt` (sólo la fuente en la línea 315). |
| PERF-0009-B | Registro del target focal de SHA-256 | PENDIENTE | COBERTURA PENDIENTE | No hay target focal de SHA-256 registrado en `CMakeLists.txt` (sólo la fuente en la línea 315). |
| PERF-0010 | Lectura fragmentada de ejecutables PE | PENDIENTE | RESUELTO | `readPeBytes` lee por bloques (`src/Linker/PeExecutableVerifier.cpp:111`). |
| PERF-0011 | Comprobación duplicada al resolver tipos LLVM | PENDIENTE | RESUELTO | `mapLlvmType` memoiza (T6.1/T6.2, `src/LLVM/Lowering/Support/LlvmTypeMapping.cpp:179`). |
| PERF-0012 | Cálculo innecesario de procedencia de dependencias | PENDIENTE | APLICA | `executeBuild` siempre llama `dependencyProvenance` (`src/Driver/ProjectCommands.cpp:1579-1581`); sólo la impresión depende de `--dependency-report`. INFERENCIA: hay consumidores (`updateRootDependencyProvenance`, `executeConfiguredBuild`), así que el gate debe confirmar que no se publica en otro lado. |
| PERF-0013 | Canonicalización repetida de raíces de paquetes | PENDIENTE | APLICA | `appendArtifact` canonicaliza `package.sourceRoot` por cada artefacto (`ProjectCommands.cpp:345`). |
| PERF-0014 | Copia innecesaria del plan de restauración | PENDIENTE | APLICA | `executeRestore` copia el plan completo en `*completedPlan = plan` (`ProjectCommands.cpp:696`). |
| PERF-0015 | Relectura de metadatos NSMETA repetidos | PENDIENTE | RESUELTO | INFERENCIA: `LibraryMetadataRepository` memoiza entradas por ruta (`entries_`, `LibraryMetadataRepository.h:49`) y tiene caché verificada persistente; la lectura repetida ya no decodifica dos veces. |
| PERF-0016-A | Cobertura de deduplicación de metadatos precompilados | PENDIENTE | COBERTURA PENDIENTE | Sin target focal de deduplicación NSMETA. |
| PERF-0016-B | Registro del target focal de deduplicación NSMETA | PENDIENTE | COBERTURA PENDIENTE | Sin target focal de deduplicación NSMETA. |
| PERF-0017 | Implementaciones dispersas del hash transparente | PENDIENTE | APLICA | Siguen tres hashes transparentes: `ProjectCompilationIndexes.h:157`, `ProjectMemberModel.h:163`, `Parser/KnownTypeNameSet.h:12`. |
| PERF-0018-A | Hash heterogéneo duplicado en el índice de nombres de miembros | PENDIENTE | APLICA | `TransparentStringHash` duplicado en `ProjectMemberModel.h:163-170` y `ProjectCompilationIndexes.h:157-163`. |
| PERF-0018-B | Hash heterogéneo duplicado en el índice de ordinales de tipos | PENDIENTE | APLICA | `TransparentStringHash` duplicado en `ProjectMemberModel.h:163-170` y `ProjectCompilationIndexes.h:157-163`. |
| PERF-0019-A | Ausencia de consulta indexada por clave de identidad | PENDIENTE | RESUELTO | Índices compartidos de T5 (`ProjectCompilationIndexes`) consultados primero, con fallback lineal para modelos armados a mano (`ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`). |
| PERF-0019-B | Ausencia de consulta indexada por clave con prefijo `type:` | PENDIENTE | RESUELTO | Índices compartidos de T5 (`ProjectCompilationIndexes`) consultados primero, con fallback lineal para modelos armados a mano (`ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`). |
| PERF-0020-A | Ausencia del mapa de ordinales por identidad | PENDIENTE | RESUELTO | Índices compartidos de T5 (`ProjectCompilationIndexes`) consultados primero, con fallback lineal para modelos armados a mano (`ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`). |
| PERF-0020-B | Ausencia de una bandera propia de disponibilidad del índice | PENDIENTE | RESUELTO | Índices compartidos de T5 (`ProjectCompilationIndexes`) consultados primero, con fallback lineal para modelos armados a mano (`ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`). |
| PERF-0021 | Construcción repetida de búsquedas de ordinales de tipo | PENDIENTE | RESUELTO | Índices compartidos de T5 (`ProjectCompilationIndexes`) consultados primero, con fallback lineal para modelos armados a mano (`ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`). |
| PERF-0022 | Compatibilidad de modelos construidos sin índice | PENDIENTE | RESUELTO | Índices compartidos de T5 (`ProjectCompilationIndexes`) consultados primero, con fallback lineal para modelos armados a mano (`ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`). |
| PERF-0023 | Integración de la implementación del índice de identidades | PENDIENTE | RESUELTO | Índices compartidos de T5 (`ProjectCompilationIndexes`) consultados primero, con fallback lineal para modelos armados a mano (`ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`). |
| PERF-0024-A | Equivalencia entre búsqueda indexada y recorrido lineal | PENDIENTE | COBERTURA PENDIENTE | Fallback presente en código; no se encontró prueba diferencial focal. |
| PERF-0024-B | Fallback de búsqueda en modelos armados sin `build()` | PENDIENTE | COBERTURA PENDIENTE | Fallback presente en código; no se encontró prueba diferencial focal. |
| PERF-0025 | Búsqueda lineal de tipos en herencia | PENDIENTE | RESUELTO | Índice primero + fallback: `ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`, `ProjectGenericConstraintModel.cpp:63`, `ProjectObjectLayoutModel.cpp:222,658,689,1105`. |
| PERF-0026 | Búsqueda lineal de tipos en interfaces | PENDIENTE | RESUELTO | Índice primero + fallback: `ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`, `ProjectGenericConstraintModel.cpp:63`, `ProjectObjectLayoutModel.cpp:222,658,689,1105`. |
| PERF-0027 | Búsqueda lineal de tipos en restricciones genéricas | PENDIENTE | RESUELTO | Índice primero + fallback: `ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`, `ProjectGenericConstraintModel.cpp:63`, `ProjectObjectLayoutModel.cpp:222,658,689,1105`. |
| PERF-0028 | Búsqueda lineal de layouts de valores | PENDIENTE | RESUELTO | Índice primero + fallback: `ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`, `ProjectGenericConstraintModel.cpp:63`, `ProjectObjectLayoutModel.cpp:222,658,689,1105`. |
| PERF-0029 | Búsqueda lineal de lifetime trivial de structs | PENDIENTE | RESUELTO | Índice primero + fallback: `ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`, `ProjectGenericConstraintModel.cpp:63`, `ProjectObjectLayoutModel.cpp:222,658,689,1105`. |
| PERF-0030 | Búsqueda lineal del ancho de bit-fields | PENDIENTE | RESUELTO | Índice primero + fallback: `ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`, `ProjectGenericConstraintModel.cpp:63`, `ProjectObjectLayoutModel.cpp:222,658,689,1105`. |
| PERF-0031 | Búsqueda lineal de bases de bit-fields de clase | PENDIENTE | RESUELTO | Índice primero + fallback: `ProjectInheritanceModel.cpp:272`, `ProjectInterfaceModel.cpp:344`, `ProjectGenericConstraintModel.cpp:63`, `ProjectObjectLayoutModel.cpp:222,658,689,1105`. |
| PERF-0032-A | Búsqueda lineal en `isStructValueType` de HIR | PENDIENTE | APLICA | Siguen lineales: `HirBuildCoordinator.cpp:2200` (`isStructValueType`), `:2212` (`isClassReferenceType`), `:2238` (`isReferenceType`), recorren `namedTypes()` comparando `identity.key()`. |
| PERF-0032-B | Búsqueda lineal en `isClassReferenceType` de HIR | PENDIENTE | APLICA | Siguen lineales: `HirBuildCoordinator.cpp:2200` (`isStructValueType`), `:2212` (`isClassReferenceType`), `:2238` (`isReferenceType`), recorren `namedTypes()` comparando `identity.key()`. |
| PERF-0032-C | Búsqueda lineal en `isReferenceType` de HIR | PENDIENTE | APLICA | Siguen lineales: `HirBuildCoordinator.cpp:2200` (`isStructValueType`), `:2212` (`isClassReferenceType`), `:2238` (`isReferenceType`), recorren `namedTypes()` comparando `identity.key()`. |
| PERF-0033 | Búsqueda lineal de enums durante HIR | PENDIENTE | APLICA | `isEnumBinaryTypeKey` hace `any_of` sobre `namedTypes()` (`HirExpressionMachine.cpp:476`). |
| PERF-0034 | Búsqueda lineal de tipos durante llamadas HIR | PENDIENTE | RESUELTO | Consultan índice (`HirCallLowering.cpp:1592`, `HirCoreCallFrameLowering.cpp:561`). |
| PERF-0035 | Búsqueda lineal de tipos en call frames de Core | PENDIENTE | RESUELTO | Consultan índice (`HirCallLowering.cpp:1592`, `HirCoreCallFrameLowering.cpp:561`). |
| PERF-0036 | Búsqueda lineal de tipos durante MIR | PENDIENTE | APLICA | `MirNodeMachine::lowerGroup21` recorre `namedTypes()` (`MirNodeMachine.cpp:1953-2011`). |
| PERF-0037-A | Búsqueda lineal en `typeKeyHasDefinedDefaultStorage` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0037-B | Búsqueda lineal en `typeKeyIsStruct` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0038-A | Búsqueda lineal en `isEnumTypeKey` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0038-B | Búsqueda lineal en `elementTypeHasTrivialLifetime` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0038-C | Búsqueda lineal en `isExternCDelegateTypeKey` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0039-A | Búsqueda lineal en `aggregateIsUnion` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0039-B | Búsqueda lineal en `aggregateRequiresActiveMember` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0039-C | Búsqueda lineal en `aggregateAllowsSynthesizedMember` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0039-D | Búsqueda lineal en `aggregateIsBorrowedView` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0039-E | Búsqueda lineal en `aggregateElementTypeKeys` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0039-F | Búsqueda lineal en `aggregateElementNames` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0040 | Búsqueda lineal de tipos al enlazar lambdas | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0041-A | Búsqueda lineal en `classifyCVariadicPromotion` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0041-B | Búsqueda lineal en `isAnyDelegateTypeKey` | PENDIENTE | RESUELTO | Predicados de Bodies con índice: `MethodBodyTypeQueries.cpp:32,56`, `TypeKeyPredicates.cpp:66,84,134`, `ConstructionAnalyzer.cpp:41-377`, `LambdaDelegateBinder.cpp:59`, `ConversionAndPassKindChecks.cpp:119,280`. |
| PERF-0042 | Búsqueda lineal de tipos nombrados en miembros | PENDIENTE | RESUELTO | `namedTypeForKey` (`MemberModel/BuildAndDeclaration.cpp:97`), `findEnumUnderlyingInfo` (`LlvmTypeLayout.cpp:17`), `LlvmTypeClassification.cpp:13,27`, `debugNamedType` (`OopLlvmDebugInfo.cpp:341`) usan índice. |
| PERF-0043 | Búsqueda lineal del tipo subyacente de enums en LLVM | PENDIENTE | RESUELTO | `namedTypeForKey` (`MemberModel/BuildAndDeclaration.cpp:97`), `findEnumUnderlyingInfo` (`LlvmTypeLayout.cpp:17`), `LlvmTypeClassification.cpp:13,27`, `debugNamedType` (`OopLlvmDebugInfo.cpp:341`) usan índice. |
| PERF-0044-A | Búsqueda lineal en `isDelegateReferenceType` de LLVM | PENDIENTE | RESUELTO | `namedTypeForKey` (`MemberModel/BuildAndDeclaration.cpp:97`), `findEnumUnderlyingInfo` (`LlvmTypeLayout.cpp:17`), `LlvmTypeClassification.cpp:13,27`, `debugNamedType` (`OopLlvmDebugInfo.cpp:341`) usan índice. |
| PERF-0044-B | Búsqueda lineal en `isClassReferenceType` de LLVM | PENDIENTE | RESUELTO | `namedTypeForKey` (`MemberModel/BuildAndDeclaration.cpp:97`), `findEnumUnderlyingInfo` (`LlvmTypeLayout.cpp:17`), `LlvmTypeClassification.cpp:13,27`, `debugNamedType` (`OopLlvmDebugInfo.cpp:341`) usan índice. |
| PERF-0045 | Búsqueda lineal de tipos para información de depuración | PENDIENTE | RESUELTO | `namedTypeForKey` (`MemberModel/BuildAndDeclaration.cpp:97`), `findEnumUnderlyingInfo` (`LlvmTypeLayout.cpp:17`), `LlvmTypeClassification.cpp:13,27`, `debugNamedType` (`OopLlvmDebugInfo.cpp:341`) usan índice. |
| PERF-0046-A | Búsqueda lineal en `isExternCDelegateType` | PENDIENTE | APLICA | `isExternCDelegateType` y `isVectorCallDelegateType` recorren `namedTypes()` (`OopLlvmThunksAndWrappers.cpp:567-598`). |
| PERF-0046-B | Búsqueda lineal en `isVectorCallDelegateType` | PENDIENTE | APLICA | `isExternCDelegateType` y `isVectorCallDelegateType` recorren `namedTypes()` (`OopLlvmThunksAndWrappers.cpp:567-598`). |
| PERF-0047-A | Búsqueda lineal en `isFloat32x4TypeKey` | PENDIENTE | RESUELTO | Clasificadores SIMD con ordinales cacheados (`OopLlvmTypesAbiAttributes.cpp:306-347`). |
| PERF-0047-B | Búsqueda lineal en `compactFloatVectorLaneCount` | PENDIENTE | RESUELTO | Clasificadores SIMD con ordinales cacheados (`OopLlvmTypesAbiAttributes.cpp:306-347`). |
| PERF-0047-C | Búsqueda lineal en `integerSimd128LaneBits` | PENDIENTE | RESUELTO | Clasificadores SIMD con ordinales cacheados (`OopLlvmTypesAbiAttributes.cpp:306-347`). |
| PERF-0047-D | Búsqueda lineal en `floatingSimd256LaneBits` | PENDIENTE | RESUELTO | Clasificadores SIMD con ordinales cacheados (`OopLlvmTypesAbiAttributes.cpp:306-347`). |
| PERF-0047-E | Búsqueda lineal en `integerSimd256LaneBits` | PENDIENTE | RESUELTO | Clasificadores SIMD con ordinales cacheados (`OopLlvmTypesAbiAttributes.cpp:306-347`). |
| PERF-0048-A | Búsqueda lineal en `namedTypeForCoreKey` | PENDIENTE | APLICA | Prioridad baja: `namedTypeForCoreKey` y `collectReachableHostTypes` siguen lineales (`Interop/CHeaderGenerator.cpp:130,253`); sólo corren al emitir cabeceras C. |
| PERF-0048-B | Búsqueda lineal en `collectReachableHostTypes` | PENDIENTE | APLICA | Prioridad baja: `namedTypeForCoreKey` y `collectReachableHostTypes` siguen lineales (`Interop/CHeaderGenerator.cpp:130,253`); sólo corren al emitir cabeceras C. |
| PERF-0049-A | Ausencia del almacenamiento del índice de layouts por miembro | PENDIENTE | RESUELTO | `hasLayoutIndexes()` + fallback (`ProjectObjectLayoutModel.cpp:161-214`). |
| PERF-0049-B | Ausencia de una bandera propia de disponibilidad del índice | PENDIENTE | RESUELTO | `hasLayoutIndexes()` + fallback (`ProjectObjectLayoutModel.cpp:161-214`). |
| PERF-0050 | Precedencia determinista del índice de layouts | PENDIENTE | RESUELTO | `hasLayoutIndexes()` + fallback (`ProjectObjectLayoutModel.cpp:161-214`). |
| PERF-0051 | Recorridos lineales al consultar layouts de campos | PENDIENTE | RESUELTO | `hasLayoutIndexes()` + fallback (`ProjectObjectLayoutModel.cpp:161-214`). |
| PERF-0052-A | Cobertura diferencial del índice de layouts de campos | PENDIENTE | COBERTURA PENDIENTE | Sin prueba diferencial focal de layouts de campos. |
| PERF-0052-B | Registro del target focal de layouts de campos | PENDIENTE | COBERTURA PENDIENTE | Sin prueba diferencial focal de layouts de campos. |
| PERF-0053-A | Ausencia del mapa de layouts de structs por clave | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0053-B | Ausencia del acceso indexado a layouts de structs | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0053-C | Ausencia del mapa de layouts de clases por clave | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0053-D | Ausencia del acceso indexado a layouts de clases | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0054-A | Construcción tardía o repetida del índice de structs | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0054-B | Construcción tardía o repetida del índice de clases | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0055-A | Recorrido LLVM en `findStructLayout` | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0055-B | Recorrido LLVM en `findClassLayout` | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0055-C | Fallback lineal de layouts de structs para modelos sin índice | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0055-D | Fallback lineal de layouts de clases para modelos sin índice | PENDIENTE | RESUELTO | `structLayoutIndexByType_`/`classLayoutIndexByType_` (`ProjectObjectLayoutModel.h:200-203`); `findStructLayout`/`findClassLayout` con índice (`LlvmTypeLayout.cpp:5-15`). |
| PERF-0056-A | Cobertura diferencial de índices de layouts MIR | PENDIENTE | COBERTURA PENDIENTE | Sin prueba diferencial focal de índices de layouts MIR. |
| PERF-0056-B | Registro del target focal de índices de layouts MIR | PENDIENTE | COBERTURA PENDIENTE | Sin prueba diferencial focal de índices de layouts MIR. |
| PERF-0057 | Resolución ABI lineal cuando existe índice de layouts | PENDIENTE | APLICA | `abiPassingForType`, `configureFunctionAbi` y `sumAwaitedFrameSizeNodes` recorren el span de layouts (`MirFunctionLowerer.cpp:256-313`); `lowerOneFunction` sigue pasando el span (`:797`). |
| PERF-0058-A | Falta de propagación del índice a `configureFunctionAbi` | PENDIENTE | APLICA | `abiPassingForType`, `configureFunctionAbi` y `sumAwaitedFrameSizeNodes` recorren el span de layouts (`MirFunctionLowerer.cpp:256-313`); `lowerOneFunction` sigue pasando el span (`:797`). |
| PERF-0058-B | Falta de propagación del índice a `lowerOneFunction` | PENDIENTE | APLICA | `abiPassingForType`, `configureFunctionAbi` y `sumAwaitedFrameSizeNodes` recorren el span de layouts (`MirFunctionLowerer.cpp:256-313`); `lowerOneFunction` sigue pasando el span (`:797`). |
| PERF-0059 | Recorridos lineales al sumar frames awaited | PENDIENTE | APLICA | `abiPassingForType`, `configureFunctionAbi` y `sumAwaitedFrameSizeNodes` recorren el span de layouts (`MirFunctionLowerer.cpp:256-313`); `lowerOneFunction` sigue pasando el span (`:797`). |
| PERF-0060-A | Ausencia de referencia compartida a layouts de structs | PENDIENTE | RESUELTO | `configureFunctionLowerer` ya no copia layouts y comparte `indexes_` (`MirFunctionLowerer.cpp:827`); `lowerGroup16`/`lowerAwaitFuture` consultan índice (`MirNodeMachine.cpp:1552,1631`). |
| PERF-0060-B | Ausencia de selección entre layouts propias y compartidas | PENDIENTE | RESUELTO | `configureFunctionLowerer` ya no copia layouts y comparte `indexes_` (`MirFunctionLowerer.cpp:827`); `lowerGroup16`/`lowerAwaitFuture` consultan índice (`MirNodeMachine.cpp:1552,1631`). |
| PERF-0061 | Falta de compartición de layouts entre lowerers | PENDIENTE | RESUELTO | `configureFunctionLowerer` ya no copia layouts y comparte `indexes_` (`MirFunctionLowerer.cpp:827`); `lowerGroup16`/`lowerAwaitFuture` consultan índice (`MirNodeMachine.cpp:1552,1631`). |
| PERF-0062-A | Lectura directa del vector copiado en `FutureFieldAddress` | PENDIENTE | RESUELTO | `configureFunctionLowerer` ya no copia layouts y comparte `indexes_` (`MirFunctionLowerer.cpp:827`); `lowerGroup16`/`lowerAwaitFuture` consultan índice (`MirNodeMachine.cpp:1552,1631`). |
| PERF-0062-B | Lectura directa del vector copiado en `lowerAwaitFuture` | PENDIENTE | RESUELTO | `configureFunctionLowerer` ya no copia layouts y comparte `indexes_` (`MirFunctionLowerer.cpp:827`); `lowerGroup16`/`lowerAwaitFuture` consultan índice (`MirNodeMachine.cpp:1552,1631`). |
| PERF-0063 | Ausencia de caché de tipos LLVM | PENDIENTE | RESUELTO | Caché de tipos LLVM (T6.1/T6.2, `OopLlvmTypesAbiAttributes.cpp:11`). |
| PERF-0064-A | Consulta repetida antes de reutilizar un tipo LLVM válido | PENDIENTE | RESUELTO | Caché de tipos LLVM (T6.1/T6.2, `OopLlvmTypesAbiAttributes.cpp:11`). |
| PERF-0064-B | Falta de almacenamiento de resultados LLVM no nulos | PENDIENTE | RESUELTO | Caché de tipos LLVM (T6.1/T6.2, `OopLlvmTypesAbiAttributes.cpp:11`). |
| PERF-0065 | Construcción completa de workers para funciones precompiladas | PENDIENTE | PARCIAL | Los miembros sólo-declaración ya no van al pool: se construyen en línea (`HirBuildCoordinator.cpp:789-797`), pero todavía instancian un `ProjectOopHirModel` completo vía `buildImpl`. |
| PERF-0066 | Construcción completa de workers para funciones C importadas | PENDIENTE | PARCIAL | Los miembros sólo-declaración ya no van al pool: se construyen en línea (`HirBuildCoordinator.cpp:789-797`), pero todavía instancian un `ProjectOopHirModel` completo vía `buildImpl`. |
| PERF-0067-A | Falta de una ruta ligera para funciones precompiladas sin cuerpo | PENDIENTE | APLICA | No existe ruta ligera para precompiladas/importadas C; usan el mismo `buildMember`. |
| PERF-0067-B | Falta de una ruta ligera para funciones C importadas | PENDIENTE | APLICA | No existe ruta ligera para precompiladas/importadas C; usan el mismo `buildMember`. |
| PERF-0068-A | Creación de workers para miembros que deben omitirse | PENDIENTE | RESUELTO | Los omitidos no crean worker (`HirBuildCoordinator.cpp:784-788`); el resultado se escribe directo en `results[memberOrdinal]`. |
| PERF-0068-B | Creación de workers para funciones precompiladas | PENDIENTE | RESUELTO | Los omitidos no crean worker (`HirBuildCoordinator.cpp:784-788`); el resultado se escribe directo en `results[memberOrdinal]`. |
| PERF-0068-C | Creación de workers para funciones C importadas | PENDIENTE | RESUELTO | Los omitidos no crean worker (`HirBuildCoordinator.cpp:784-788`); el resultado se escribe directo en `results[memberOrdinal]`. |
| PERF-0069-A | Escritura directa de resultado HIR precompilado | PENDIENTE | RESUELTO | Los omitidos no crean worker (`HirBuildCoordinator.cpp:784-788`); el resultado se escribe directo en `results[memberOrdinal]`. |
| PERF-0069-B | Escritura directa de resultado HIR importado desde C | PENDIENTE | RESUELTO | Los omitidos no crean worker (`HirBuildCoordinator.cpp:784-788`); el resultado se escribe directo en `results[memberOrdinal]`. |
| PERF-0069-C | Escritura directa de resultado HIR regular | PENDIENTE | RESUELTO | Los omitidos no crean worker (`HirBuildCoordinator.cpp:784-788`); el resultado se escribe directo en `results[memberOrdinal]`. |
| PERF-0070-A | Redimensionamiento del vector HIR por miembro en cada worker | PENDIENTE | RESUELTO | Con `memberFilter` el worker no redimensiona `functionByMember_` ni `functionByNamespaceFunction_` (`HirBuildCoordinator.cpp:2157-2162`). |
| PERF-0070-B | Redimensionamiento del vector HIR de namespace en cada worker | PENDIENTE | RESUELTO | Con `memberFilter` el worker no redimensiona `functionByMember_` ni `functionByNamespaceFunction_` (`HirBuildCoordinator.cpp:2157-2162`). |
| PERF-0071 | Sobrecosto del camino secuencial filtrado de HIR | PENDIENTE | NO VERIFICADO | `buildRegularFunctionsSequentially` (`HirBuildCoordinator.cpp:1596`) no se midió. |
| PERF-0072 | Búsqueda repetida de locales por callable | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0073 | Búsqueda repetida de locales por lambda | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0074 | Búsqueda repetida de cleanups de parámetros | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0075-A | Reconstrucción del índice de locales por callable | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0075-B | Reconstrucción del índice de locales por lambda | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0075-C | Reconstrucción del índice de cleanups por callable | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0075-D | Ausencia del acceso a locales indexados por callable | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0075-E | Ausencia del acceso a locales indexados por lambda | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0075-F | Ausencia del acceso a cleanups indexados por callable | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0076 | Recorridos de locales al construir funciones regulares | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0077 | Recorridos de locales al construir funciones de namespace | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0078 | Recorridos de locales al construir funciones lambda | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0079 | Recorridos de cleanups en funciones regulares | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0080 | Recorridos de cleanups en funciones de namespace | PENDIENTE | RESUELTO | Locales y cleanups por índice compartido + fallback (`hirLocalOrdinalsForMember/ForLambda`, `HirBuildCoordinator.cpp:989-1906`). |
| PERF-0081-A | Equivalencia del orden HIR entre ejecución secuencial y paralela | PENDIENTE | COBERTURA PENDIENTE | Sin target focal de orden HIR secuencial/paralelo. |
| PERF-0081-B | Registro del target focal de orden HIR | PENDIENTE | COBERTURA PENDIENTE | Sin target focal de orden HIR secuencial/paralelo. |
| PERF-0082 | Búsqueda repetida de funciones lambda al marcar closures | PENDIENTE | RESUELTO | `markLambdaClosureCells` usa `functionByLambda_`/`functionByMember_` (`HirBuildCoordinator.cpp:1920-1971`). |
| PERF-0083-A | Canonicalización repetida del directorio del proyecto | PENDIENTE | APLICA | `ProjectSourceSet::add` canonicaliza y consulta `is_directory` del proyecto en cada fuente (`ProjectSourceSet.cpp:22-29`); además el duplicado es O(n²) y recalcula `comparisonKey` por elemento (`:56-63`). |
| PERF-0083-B | Consulta repetida de existencia del directorio del proyecto | PENDIENTE | APLICA | `ProjectSourceSet::add` canonicaliza y consulta `is_directory` del proyecto en cada fuente (`ProjectSourceSet.cpp:22-29`); además el duplicado es O(n²) y recalcula `comparisonKey` por elemento (`:56-63`). |
| PERF-0084-A | Reconstrucción de `comparisonKey` al ordenar fuentes | PENDIENTE | APLICA | El comparador de `finalize` reconstruye `comparisonKey` y `generic_string` en cada comparación (`ProjectSourceSet.cpp:86-98`). |
| PERF-0084-B | Reconstrucción de `generic_string` al ordenar fuentes | PENDIENTE | APLICA | El comparador de `finalize` reconstruye `comparisonKey` y `generic_string` en cada comparación (`ProjectSourceSet.cpp:86-98`). |
| PERF-0084-C | Comparación directa repetida durante el ordenamiento de fuentes | PENDIENTE | APLICA | El comparador de `finalize` reconstruye `comparisonKey` y `generic_string` en cada comparación (`ProjectSourceSet.cpp:86-98`). |
| PERF-0085-A | Canonicalización repetida en el primer recorrido de propiedad transitiva | PENDIENTE | APLICA | `normalizeTransitiveSourceOwnership` canonicaliza cada fuente dos veces y `ownSources.add` la vuelve a canonicalizar (`ProjectGraphLoader.cpp:146-165`). |
| PERF-0085-B | Canonicalización repetida en el segundo recorrido de propiedad transitiva | PENDIENTE | APLICA | `normalizeTransitiveSourceOwnership` canonicaliza cada fuente dos veces y `ownSources.add` la vuelve a canonicalizar (`ProjectGraphLoader.cpp:146-165`). |
| PERF-0086-A | Canonicalización repetida del proyecto al elegir artefactos exactos | PENDIENTE | APLICA | `preferExactSourceModuleArtifacts` canonicaliza dentro de `any_of` y de los dos `sort` (`ProjectGraphLoader.cpp:207-227`). |
| PERF-0086-B | Canonicalización repetida dentro del ordenamiento de artefactos | PENDIENTE | APLICA | `preferExactSourceModuleArtifacts` canonicaliza dentro de `any_of` y de los dos `sort` (`ProjectGraphLoader.cpp:207-227`). |
| PERF-0087 | Ausencia de índice de tipos declarados por nombre fuente | PENDIENTE | APLICA | `declaredTypeIdentities_` es un vector recorrido linealmente (`TypeSpellingAndVisibility.cpp:233,295`). |
| PERF-0088 | Recolección no indexada de tipos declarados | PENDIENTE | APLICA | `declaredTypeIdentities_` es un vector recorrido linealmente (`TypeSpellingAndVisibility.cpp:233,295`). |
| PERF-0089 | Búsqueda lineal de identidades calificadas por módulo | PENDIENTE | APLICA | `declaredTypeIdentities_` es un vector recorrido linealmente (`TypeSpellingAndVisibility.cpp:233,295`). |
| PERF-0090 | Búsqueda lineal de identidades de tipos declarados | PENDIENTE | APLICA | `declaredTypeIdentities_` es un vector recorrido linealmente (`TypeSpellingAndVisibility.cpp:233,295`). |
| PERF-0091-A | Cobertura diferencial del índice de tipos declarados | PENDIENTE | COBERTURA PENDIENTE | Sin índice que cubrir todavía. |
| PERF-0091-B | Registro del target focal de tipos declarados | PENDIENTE | COBERTURA PENDIENTE | Sin índice que cubrir todavía. |
| PERF-0092 | Copia anticipada del scope genérico copy-on-write | PENDIENTE | RESUELTO | Scopes copy-on-write con `read()/write()` (`GenericNodeProcessor.cpp:107-117`); `outScope = scope` sólo comparte. INFERENCIA: la copia profunda ocurre sólo al escribir. |
| PERF-0093 | Materialización innecesaria de scopes genéricos de solo lectura | PENDIENTE | RESUELTO | Scopes copy-on-write con `read()/write()` (`GenericNodeProcessor.cpp:107-117`); `outScope = scope` sólo comparte. INFERENCIA: la copia profunda ocurre sólo al escribir. |
| PERF-0094 | Copia innecesaria de hijos en el modelo genérico | PENDIENTE | RESUELTO | INFERENCIA: `AstNode::children()` devuelve `const std::vector<AstNode*>&` (`AST/AstNode.h:97`) y no hay copias locales `children = ...` en `src/Frontend/Generics`. |
| PERF-0095 | Copia innecesaria de hijos al expandir aliases | PENDIENTE | RESUELTO | INFERENCIA: `AstNode::children()` devuelve `const std::vector<AstNode*>&` (`AST/AstNode.h:97`) y no hay copias locales `children = ...` en `src/Frontend/Generics`. |
| PERF-0096 | Copia innecesaria de hijos al procesar nodos genéricos | PENDIENTE | RESUELTO | INFERENCIA: `AstNode::children()` devuelve `const std::vector<AstNode*>&` (`AST/AstNode.h:97`) y no hay copias locales `children = ...` en `src/Frontend/Generics`. |
| PERF-0097 | Primera copia innecesaria en dependencias de templates | PENDIENTE | RESUELTO | INFERENCIA: `AstNode::children()` devuelve `const std::vector<AstNode*>&` (`AST/AstNode.h:97`) y no hay copias locales `children = ...` en `src/Frontend/Generics`. |
| PERF-0098 | Segunda copia innecesaria en dependencias de templates | PENDIENTE | RESUELTO | INFERENCIA: `AstNode::children()` devuelve `const std::vector<AstNode*>&` (`AST/AstNode.h:97`) y no hay copias locales `children = ...` en `src/Frontend/Generics`. |
| PERF-0099 | Copia innecesaria de hijos al recolectar templates | PENDIENTE | RESUELTO | INFERENCIA: `AstNode::children()` devuelve `const std::vector<AstNode*>&` (`AST/AstNode.h:97`) y no hay copias locales `children = ...` en `src/Frontend/Generics`. |
| PERF-0100 | Copia innecesaria del namespace de origen | PENDIENTE | NO VERIFICADO | No se localizó la copia del namespace de origen. |
| PERF-0101-A | Construcción de string para buscar templates por nombre terminal | PENDIENTE | APLICA | `resolveTypeTemplate` ya agrupa por nombre terminal (NSC305-G02), pero construye `std::string(terminalName)`, `qualifiedName(...)` y un `candidate` por estado (`TemplateResolver.cpp:133-152`). |
| PERF-0101-B | Reconstrucción del nombre calificado de cada template | PENDIENTE | APLICA | `resolveTypeTemplate` ya agrupa por nombre terminal (NSC305-G02), pero construye `std::string(terminalName)`, `qualifiedName(...)` y un `candidate` por estado (`TemplateResolver.cpp:133-152`). |
| PERF-0101-C | Comparación contra nombres calificados reconstruidos | PENDIENTE | APLICA | `resolveTypeTemplate` ya agrupa por nombre terminal (NSC305-G02), pero construye `std::string(terminalName)`, `qualifiedName(...)` y un `candidate` por estado (`TemplateResolver.cpp:133-152`). |
| PERF-0102 | Resolución duplicada de sintaxis de tipos demandados | PENDIENTE | NO VERIFICADO | `processTypeSyntax` (`TypeDemandScheduler.cpp:548`) no se revisó a fondo. |
| PERF-0103 | Canonicalización repetida de rutas de visibilidad | PENDIENTE | APLICA | `buildVisibility` hace `weakly_canonical` + minúsculas por cada referencia en cada visita (`TypeSpellingAndVisibility.cpp:398-423`). |
| PERF-0104-A | Tres tablas separadas para clasificar keywords | PENDIENTE | RESUELTO | Una sola tabla ordenada con `lower_bound` (`Lexer.cpp:164-171`); `lexAll` reserva `size/4+1` (`:192`). |
| PERF-0104-B | Hasta tres consultas hash por identificador | PENDIENTE | RESUELTO | Una sola tabla ordenada con `lower_bound` (`Lexer.cpp:164-171`); `lexAll` reserva `size/4+1` (`:192`). |
| PERF-0105 | Realocaciones evitables durante lexing | PENDIENTE | RESUELTO | Una sola tabla ordenada con `lower_bound` (`Lexer.cpp:164-171`); `lexAll` reserva `size/4+1` (`:192`). |
| PERF-0106 | Asignaciones temporales al consultar nombres de tipos | PENDIENTE | RESUELTO | INFERENCIA: `KnownTypeNameSet` con hash transparente y `std::equal_to<>` (`Parser/KnownTypeNameSet.h:12-24`) permite consultar sin string temporal. |
| PERF-0107 | Crecimiento repetido del buffer de consulta del parser | PENDIENTE | RESUELTO | INFERENCIA: `KnownTypeNameSet` con hash transparente y `std::equal_to<>` (`Parser/KnownTypeNameSet.h:12-24`) permite consultar sin string temporal. |
| PERF-0108 | Reconstrucción del buffer de búsqueda de tipos | PENDIENTE | RESUELTO | INFERENCIA: `KnownTypeNameSet` con hash transparente y `std::equal_to<>` (`Parser/KnownTypeNameSet.h:12-24`) permite consultar sin string temporal. |
| PERF-0109-A | Realocaciones al reunir nombres conocidos por el parser | PENDIENTE | APLICA | `collectParserKnownTypeNames` usa `std::set` y un doble bucle alias × nombres (`ProjectCompilation.cpp:588-621`); se llama una vez por compilación (`:1858`). |
| PERF-0109-B | Reconstrucción repetida de nombres conocidos por compilación | PENDIENTE | PARCIAL | Se calcula una vez por compilación (`ProjectCompilation.cpp:1858`); se repite sólo entre compilaciones (IDE). |
| PERF-0110-A | Búsqueda lineal en metadata decodificada y ordenada | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0110-B | Búsqueda lineal en metadata construida en memoria | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0111 | Recorridos de metadata al publicar miembros | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0112 | Recorridos de metadata al publicar contratos | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0113-A | Equivalencia de búsquedas en metadata ordenada | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0113-B | Equivalencia del fallback sobre metadata en memoria | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0113-C | Registro del target focal de superficie pública | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0114 | Recalculo de prioridad al ordenar métodos publicados | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0115-A | Llamadas repetidas al ranking dentro del comparador | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0115-B | Ausencia de índice de dispatch por clave de tipo | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0115-C | Ausencia de índice de firmas de interfaces | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0115-D | Reconstrucción del orden después de clasificar los métodos | PENDIENTE | PARCIAL | Siguen lineales (`PublicSurfaceBuilder.cpp:1144-1357`), pero tras T4 corren sólo en el productor o como fallback. |
| PERF-0116-A | Ausencia de memo de resultados de tipos de contrato | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0116-B | Ausencia de memo de alcanzabilidad pública | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0117-A | Rematerialización al validar declaraciones precompiladas | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0117-B | Rematerialización al validar métodos nativos | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0117-C | Rematerialización al validar constructores | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0117-D | Envoltorios públicos sin reutilización de caché | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0117-E | Caché aislada durante generación de superficie pública | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0117-F | Caché aislada durante publicación de símbolos nativos | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0118 | Cachés aisladas durante materialización recursiva | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0119-A | Cobertura de equivalencia de la caché de contratos | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0119-B | Registro del target focal de materialización de contratos | PENDIENTE | APLICA | `ContractMaterialization.cpp` no tiene memo ni caché compartida (`:22-561`). |
| PERF-0120-A | Nueva visita de un template genérico ya verificado | PENDIENTE | APLICA | `verifyGenericTemplateClosure` borra `visiting` al salir y no registra raíces verificadas; cada raíz pública vuelve a recorrer el cierre (`LibraryMetadataVerification.cpp:829-909`). |
| PERF-0120-B | Falta de registro de templates verificados con éxito | PENDIENTE | APLICA | `verifyGenericTemplateClosure` borra `visiting` al salir y no registra raíces verificadas; cada raíz pública vuelve a recorrer el cierre (`LibraryMetadataVerification.cpp:829-909`). |
| PERF-0121 | Memos aislados entre raíces de verificación | PENDIENTE | APLICA | `verifyGenericTemplateClosure` borra `visiting` al salir y no registra raíces verificadas; cada raíz pública vuelve a recorrer el cierre (`LibraryMetadataVerification.cpp:829-909`). |
| PERF-0122-A | Búsqueda lineal de tablas de dispatch por tipo | PENDIENTE | APLICA | `findMetadataDispatch` y `findMetadataDispatchSlot` son lineales (`LibraryMetadataVerification.cpp:595-620`). |
| PERF-0122-B | Búsqueda lineal de slots por firma | PENDIENTE | APLICA | `findMetadataDispatch` y `findMetadataDispatchSlot` son lineales (`LibraryMetadataVerification.cpp:595-620`). |
| PERF-0123 | Búsqueda lineal de miembros virtuales | PENDIENTE | APLICA | `findMetadataDispatch` y `findMetadataDispatchSlot` son lineales (`LibraryMetadataVerification.cpp:595-620`). |
| PERF-0124-A | Recorrido repetido en `findMetadataDispatch` | PENDIENTE | APLICA | `findMetadataDispatch` y `findMetadataDispatchSlot` son lineales (`LibraryMetadataVerification.cpp:595-620`). |
| PERF-0124-B | Recorrido repetido en `findMetadataDispatchSlot` | PENDIENTE | APLICA | `findMetadataDispatch` y `findMetadataDispatchSlot` son lineales (`LibraryMetadataVerification.cpp:595-620`). |
| PERF-0124-C | Falta de propagación de índices a `verifyVirtualMemberDispatch` | PENDIENTE | APLICA | `findMetadataDispatch` y `findMetadataDispatchSlot` son lineales (`LibraryMetadataVerification.cpp:595-620`). |
| PERF-0125 | Construcción anticipada del primer diagnóstico de metadata | PENDIENTE | APLICA | Los textos de diagnóstico se construyen antes de saber si hay error, p. ej. `"member '" + ...` por miembro (`LibraryMetadataVerification.cpp:1490-1509`). |
| PERF-0126 | Construcción anticipada del segundo diagnóstico de metadata | PENDIENTE | APLICA | Los textos de diagnóstico se construyen antes de saber si hay error, p. ej. `"member '" + ...` por miembro (`LibraryMetadataVerification.cpp:1490-1509`). |
| PERF-0127 | Construcción anticipada del tercer diagnóstico de metadata | PENDIENTE | APLICA | Los textos de diagnóstico se construyen antes de saber si hay error, p. ej. `"member '" + ...` por miembro (`LibraryMetadataVerification.cpp:1490-1509`). |
| PERF-0128 | Construcción anticipada del cuarto diagnóstico de metadata | PENDIENTE | APLICA | Los textos de diagnóstico se construyen antes de saber si hay error, p. ej. `"member '" + ...` por miembro (`LibraryMetadataVerification.cpp:1490-1509`). |
| PERF-0129-A | Ausencia del almacenamiento de miembros declarados por tipo | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0129-B | Ausencia de una bandera propia de disponibilidad del índice de miembros | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0129-C | Ausencia del acceso indexado a miembros declarados por tipo | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0129-D | Falta de construcción del índice al cerrar el modelo de miembros | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0129-E | Falta de fallback para modelos de miembros armados manualmente | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0130 | Filtrado lineal de miembros en predicados de tipos | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0131-A | Filtrado lineal de miembros en `bindAtomicOperation` | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0131-B | Filtrado lineal de miembros en operaciones Mutex/Monitor | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0131-C | Filtrado lineal de miembros en `callTypeIsSend` | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0131-D | Filtrado lineal de miembros en `callTypeIsSync` | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0132 | Filtrado lineal de miembros durante construcción | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0133 | Bucle interno lineal del ciclo de vida de callables | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0134 | Filtrado lineal de miembros durante coordinación HIR | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0135 | Filtrado lineal de miembros en call frames de Core | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0136 | Filtrado lineal de miembros para debug info LLVM | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0137 | Filtrado lineal de miembros en restricciones genéricas | PENDIENTE | RESUELTO | `membersDeclaredByType` + `hasMembersDeclaredByTypeIndex()` con fallback; `findMembers` en `bindAtomicOperation` (`CallAnalyzer.cpp:2214`), Send/Sync (`:4874-4937`), `HirBuildCoordinator.cpp:1371`, `OopLlvmDebugInfo.cpp:859`, `ProjectGenericConstraintModel.cpp:853`. |
| PERF-0138-A | Cobertura diferencial del índice de miembros por tipo | PENDIENTE | COBERTURA PENDIENTE | No hay prueba diferencial focal del índice de miembros. |
| PERF-0138-B | Registro del target focal de miembros por tipo | PENDIENTE | COBERTURA PENDIENTE | No hay prueba diferencial focal del índice de miembros. |
| PERF-0139-A | Ausencia del almacenamiento de planes de interfaz por clase | PENDIENTE | RESUELTO | `tablePlanOrdinalsForClass` + `hasTablePlanOrdinalsByClassIndex()` con fallback (`AssignmentAnalyzer.cpp:2574-2584`). |
| PERF-0139-B | Falta de construcción del índice de planes de interfaz | PENDIENTE | RESUELTO | `tablePlanOrdinalsForClass` + `hasTablePlanOrdinalsByClassIndex()` con fallback (`AssignmentAnalyzer.cpp:2574-2584`). |
| PERF-0139-C | Ausencia del acceso indexado a planes de interfaz | PENDIENTE | RESUELTO | `tablePlanOrdinalsForClass` + `hasTablePlanOrdinalsByClassIndex()` con fallback (`AssignmentAnalyzer.cpp:2574-2584`). |
| PERF-0139-D | Falta de fallback lineal para modelos sin índice de planes | PENDIENTE | RESUELTO | `tablePlanOrdinalsForClass` + `hasTablePlanOrdinalsByClassIndex()` con fallback (`AssignmentAnalyzer.cpp:2574-2584`). |
| PERF-0140 | Recorrido lineal de planes de interfaz en asignaciones | PENDIENTE | RESUELTO | `tablePlanOrdinalsForClass` + `hasTablePlanOrdinalsByClassIndex()` con fallback (`AssignmentAnalyzer.cpp:2574-2584`). |
| PERF-0141-A | Ausencia de almacenamiento del ordinal de `Exception` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0141-B | Ausencia de almacenamiento del ordinal de `IDisposable` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0141-C | Ausencia de almacenamiento del ordinal de `ThreadStart` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0141-D | Ausencia de almacenamiento del ordinal de `StringView` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0141-E | Ausencia de almacenamiento del ordinal de la fachada nativa | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0142-A | Resolución repetida del ordinal de `Exception` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0142-B | Resolución repetida del ordinal de `IDisposable` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0142-C | Resolución repetida del ordinal de `ThreadStart` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0142-D | Resolución repetida del ordinal de `StringView` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0142-E | Resolución repetida del ordinal de la fachada nativa | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0143 | Falta de propagación de ordinales de Core a los shards | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0144 | Búsqueda repetida de la raíz de excepciones | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0145 | Búsqueda repetida de `IDisposable` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0146 | Búsqueda repetida de `ThreadStart` | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0147 | Búsqueda repetida del tipo usado por patrones de string | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0148 | Búsqueda repetida de la fachada nativa | PENDIENTE | RESUELTO | Ordinales y claves de Core resueltos una vez (`MethodBodyBuildCoordinator.cpp:169-193`) y propagados a los shards (`MethodBodyCallableLifecycle.cpp:261-267`). |
| PERF-0149-A | Ausencia del almacenamiento de tipos por nombre simple | PENDIENTE | RESUELTO | `resolveTypeIdentifier` usa `typeOrdinalsForName` (`AssignmentAnalyzer.cpp:2649`). |
| PERF-0149-B | Falta de construcción del índice por nombre simple | PENDIENTE | RESUELTO | `resolveTypeIdentifier` usa `typeOrdinalsForName` (`AssignmentAnalyzer.cpp:2649`). |
| PERF-0149-C | Ausencia del acceso ordenado a candidatos por nombre simple | PENDIENTE | RESUELTO | `resolveTypeIdentifier` usa `typeOrdinalsForName` (`AssignmentAnalyzer.cpp:2649`). |
| PERF-0150 | Búsqueda lineal de identificadores de tipo en asignaciones | PENDIENTE | RESUELTO | `resolveTypeIdentifier` usa `typeOrdinalsForName` (`AssignmentAnalyzer.cpp:2649`). |
| PERF-0151 | Cobertura de nombres simples repetidos | PENDIENTE | COBERTURA PENDIENTE | Sin cobertura focal de nombres simples repetidos. |
| PERF-0152-A | Ausencia del almacenamiento de extensiones por nombre | PENDIENTE | RESUELTO | `extensionFunctionsNamed` + `hasExtensionFunctionIndex()` con fallback (`MemberAccessAnalyzer.cpp:1250-1263`). |
| PERF-0152-B | Falta de construcción del índice de extensiones | PENDIENTE | RESUELTO | `extensionFunctionsNamed` + `hasExtensionFunctionIndex()` con fallback (`MemberAccessAnalyzer.cpp:1250-1263`). |
| PERF-0152-C | Ausencia del acceso indexado a extensiones por nombre | PENDIENTE | RESUELTO | `extensionFunctionsNamed` + `hasExtensionFunctionIndex()` con fallback (`MemberAccessAnalyzer.cpp:1250-1263`). |
| PERF-0152-D | Falta de fallback lineal para modelos sin índice de extensiones | PENDIENTE | RESUELTO | `extensionFunctionsNamed` + `hasExtensionFunctionIndex()` con fallback (`MemberAccessAnalyzer.cpp:1250-1263`). |
| PERF-0153 | Recorrido lineal de extensiones candidatas | PENDIENTE | RESUELTO | `extensionFunctionsNamed` + `hasExtensionFunctionIndex()` con fallback (`MemberAccessAnalyzer.cpp:1250-1263`). |
| PERF-0154-A | Ausencia del almacenamiento de templates por clave canónica | PENDIENTE | RESUELTO | `templateForCanonicalKey` y `genericTemplateOrdinalsForName` (`CarrierResolver.cpp:167`, `ProjectCompilationIndexes.cpp:582`). |
| PERF-0154-B | Falta de construcción del índice de templates | PENDIENTE | RESUELTO | `templateForCanonicalKey` y `genericTemplateOrdinalsForName` (`CarrierResolver.cpp:167`, `ProjectCompilationIndexes.cpp:582`). |
| PERF-0154-C | Ausencia del acceso indexado a templates por clave | PENDIENTE | RESUELTO | `templateForCanonicalKey` y `genericTemplateOrdinalsForName` (`CarrierResolver.cpp:167`, `ProjectCompilationIndexes.cpp:582`). |
| PERF-0154-D | Falta de fallback lineal para modelos sin índice de templates | PENDIENTE | RESUELTO | `templateForCanonicalKey` y `genericTemplateOrdinalsForName` (`CarrierResolver.cpp:167`, `ProjectCompilationIndexes.cpp:582`). |
| PERF-0155 | Recorrido lineal de templates al resolver carriers | PENDIENTE | RESUELTO | `templateForCanonicalKey` y `genericTemplateOrdinalsForName` (`CarrierResolver.cpp:167`, `ProjectCompilationIndexes.cpp:582`). |
| PERF-0156-A | Alias inútiles recorridos por cada identidad física | PENDIENTE | APLICA | `bindPrecompiledPhysicalNames` reaplica todos los alias por miembro y recanonicaliza la clave del tipo declarante en cada miembro (`MemberModel/BuildSupport.cpp:333-369`). |
| PERF-0156-B | Recanonicalización de la clave del tipo declarante por miembro | PENDIENTE | APLICA | `bindPrecompiledPhysicalNames` reaplica todos los alias por miembro y recanonicaliza la clave del tipo declarante en cada miembro (`MemberModel/BuildSupport.cpp:333-369`). |
| PERF-0156-C | Canonicalización innecesaria cuando no existen aliases efectivos | PENDIENTE | APLICA | `bindPrecompiledPhysicalNames` reaplica todos los alias por miembro y recanonicaliza la clave del tipo declarante en cada miembro (`MemberModel/BuildSupport.cpp:333-369`). |
| PERF-0157-A | Ausencia del almacenamiento del índice de funciones precompiladas | PENDIENTE | RESUELTO | `PrecompiledMemberIdentityIndex` busca por tipo, clave del declarante, nombre y firma (`BuildSupport.cpp:352-371`). |
| PERF-0157-B | Falta de construcción del índice de funciones precompiladas | PENDIENTE | RESUELTO | `PrecompiledMemberIdentityIndex` busca por tipo, clave del declarante, nombre y firma (`BuildSupport.cpp:352-371`). |
| PERF-0157-C | Ausencia de búsqueda por los cuatro campos de identidad | PENDIENTE | RESUELTO | `PrecompiledMemberIdentityIndex` busca por tipo, clave del declarante, nombre y firma (`BuildSupport.cpp:352-371`). |
| PERF-0157-D | Falta de preservación del primer ordinal entre identidades iguales | PENDIENTE | RESUELTO | `PrecompiledMemberIdentityIndex` busca por tipo, clave del declarante, nombre y firma (`BuildSupport.cpp:352-371`). |
| PERF-0158 | Recorrido lineal al enlazar resúmenes de escape | PENDIENTE | APLICA | `bindPrecompiledEscapeSummaries` hace `find_if` por función (`ProjectNamespaceFunctionModel.cpp:1629-1638`). |
| PERF-0159-A | Cobertura diferencial de identidades precompiladas | PENDIENTE | COBERTURA PENDIENTE | Sin cobertura diferencial focal. |
| PERF-0159-B | Registro del target focal de identidades precompiladas | PENDIENTE | COBERTURA PENDIENTE | Sin cobertura diferencial focal. |
| PERF-0160-A | Recalculo del checksum de un archivo en cada unidad LLVM | PENDIENTE | APLICA | `createDebugFile` calcula SHA-256 del fuente completo y el mapa `debugFiles` vive por unidad, así que cada unidad vuelve a hashear cada archivo (`DebugInfoEmission.cpp:111-148`). |
| PERF-0160-B | Falta de sincronización por entrada de checksum compartido | PENDIENTE | APLICA | `createDebugFile` calcula SHA-256 del fuente completo y el mapa `debugFiles` vive por unidad, así que cada unidad vuelve a hashear cada archivo (`DebugInfoEmission.cpp:111-148`). |
| PERF-0161-A | Falta de propiedad de la caché en `ProjectOopObjectGenerator` | PENDIENTE | APLICA | `createDebugFile` calcula SHA-256 del fuente completo y el mapa `debugFiles` vive por unidad, así que cada unidad vuelve a hashear cada archivo (`DebugInfoEmission.cpp:111-148`). |
| PERF-0161-B | Falta de propagación de la caché a cada emisor | PENDIENTE | APLICA | `createDebugFile` calcula SHA-256 del fuente completo y el mapa `debugFiles` vive por unidad, así que cada unidad vuelve a hashear cada archivo (`DebugInfoEmission.cpp:111-148`). |
| PERF-0161-C | Falta de consumo de la caché en `createDebugFile` | PENDIENTE | APLICA | `createDebugFile` calcula SHA-256 del fuente completo y el mapa `debugFiles` vive por unidad, así que cada unidad vuelve a hashear cada archivo (`DebugInfoEmission.cpp:111-148`). |
| PERF-0162-A | Ausencia del índice de bindings por par de tipos | PENDIENTE | RESUELTO | `interfaceBindingForTypes`/`interfaceSlotForMember` con `indexes_` + `interfaceIndexesBuilt_` + fallback (`MirBuildCoordinator.cpp:878-933`). |
| PERF-0162-B | Falta de construcción del índice de bindings de interfaz | PENDIENTE | RESUELTO | `interfaceBindingForTypes`/`interfaceSlotForMember` con `indexes_` + `interfaceIndexesBuilt_` + fallback (`MirBuildCoordinator.cpp:878-933`). |
| PERF-0162-C | Falta de fallback lineal para bindings de interfaz | PENDIENTE | RESUELTO | `interfaceBindingForTypes`/`interfaceSlotForMember` con `indexes_` + `interfaceIndexesBuilt_` + fallback (`MirBuildCoordinator.cpp:878-933`). |
| PERF-0163-A | Ausencia del índice de slots por miembro | PENDIENTE | RESUELTO | `interfaceBindingForTypes`/`interfaceSlotForMember` con `indexes_` + `interfaceIndexesBuilt_` + fallback (`MirBuildCoordinator.cpp:878-933`). |
| PERF-0163-B | Falta de construcción del índice de slots de interfaz | PENDIENTE | RESUELTO | `interfaceBindingForTypes`/`interfaceSlotForMember` con `indexes_` + `interfaceIndexesBuilt_` + fallback (`MirBuildCoordinator.cpp:878-933`). |
| PERF-0163-C | Falta de fallback lineal para slots de interfaz | PENDIENTE | RESUELTO | `interfaceBindingForTypes`/`interfaceSlotForMember` con `indexes_` + `interfaceIndexesBuilt_` + fallback (`MirBuildCoordinator.cpp:878-933`). |
| PERF-0164-A | Plegado repetido de nombres de paquetes | PENDIENTE | APLICA | `typeIsVisibleTo` pliega nombres con una lambda en cada llamada (`NameLookup/TypeVisibility.cpp:37-95`). |
| PERF-0164-B | Plegado repetido de nombres de módulos | PENDIENTE | APLICA | `typeIsVisibleTo` pliega nombres con una lambda en cada llamada (`NameLookup/TypeVisibility.cpp:37-95`). |
| PERF-0164-C | Plegado repetido de nombres de módulos amigos | PENDIENTE | APLICA | `typeIsVisibleTo` pliega nombres con una lambda en cada llamada (`NameLookup/TypeVisibility.cpp:37-95`). |
| PERF-0165-A | Plegado repetido al comparar visibilidad de paquete | PENDIENTE | APLICA | `typeIsVisibleTo` pliega nombres con una lambda en cada llamada (`NameLookup/TypeVisibility.cpp:37-95`). |
| PERF-0165-B | Plegado repetido al comparar visibilidad de módulo | PENDIENTE | APLICA | `typeIsVisibleTo` pliega nombres con una lambda en cada llamada (`NameLookup/TypeVisibility.cpp:37-95`). |
| PERF-0165-C | Plegado repetido al comprobar módulos amigos | PENDIENTE | APLICA | `typeIsVisibleTo` pliega nombres con una lambda en cada llamada (`NameLookup/TypeVisibility.cpp:37-95`). |
| PERF-0165-D | Compatibilidad de las APIs públicas de visibilidad usadas por el IDE | PENDIENTE | APLICA | `typeIsVisibleTo` pliega nombres con una lambda en cada llamada (`NameLookup/TypeVisibility.cpp:37-95`). |
| PERF-0166 | Asignaciones de string al consultar especializaciones | PENDIENTE | RESUELTO | `specializationForTypeKey` usa `string_view` + índice (`ProjectGenericModel.cpp:650-662`); `closedTypeKeyForTemplate` usa `genericTemplateOrdinalsForName` (`:672`). |
| PERF-0167 | Búsqueda lineal de templates cerrados por nombre | PENDIENTE | RESUELTO | `specializationForTypeKey` usa `string_view` + índice (`ProjectGenericModel.cpp:650-662`); `closedTypeKeyForTemplate` usa `genericTemplateOrdinalsForName` (`:672`). |
| PERF-0168 | Búsqueda repetida de inicializadores de campos estáticos | PENDIENTE | RESUELTO | `hirFieldInitializerByMember_` + fallback (`MirBuildCoordinator.cpp:467-480`). |
| PERF-0169 | Recorrido lineal al materializar constantes estáticas | PENDIENTE | RESUELTO | `hirFieldInitializerByMember_` + fallback (`MirBuildCoordinator.cpp:467-480`). |
| PERF-0170 | Búsqueda lineal de módulos runtime por DLL | PENDIENTE | APLICA | `appendExternalImport` hace `find_if` con `foldedName` de ambos lados y `find` lineal de símbolos (`LibraryReferenceValidation.cpp:11-23`). |
| PERF-0171-A | Deduplicación lineal de símbolos por módulo | PENDIENTE | APLICA | `appendExternalImport` hace `find_if` con `foldedName` de ambos lados y `find` lineal de símbolos (`LibraryReferenceValidation.cpp:11-23`). |
| PERF-0171-B | Falta de reutilización del índice en el primer llamador de artefactos | PENDIENTE | APLICA | `appendExternalImport` hace `find_if` con `foldedName` de ambos lados y `find` lineal de símbolos (`LibraryReferenceValidation.cpp:11-23`). |
| PERF-0171-C | Falta de reutilización del índice en el segundo llamador de artefactos | PENDIENTE | APLICA | `appendExternalImport` hace `find_if` con `foldedName` de ambos lados y `find` lineal de símbolos (`LibraryReferenceValidation.cpp:11-23`). |
| PERF-0171-D | Falta de reutilización del índice en el tercer llamador de artefactos | PENDIENTE | APLICA | `appendExternalImport` hace `find_if` con `foldedName` de ambos lados y `find` lineal de símbolos (`LibraryReferenceValidation.cpp:11-23`). |
| PERF-0171-E | Falta de reutilización del índice en validación de referencias | PENDIENTE | APLICA | `appendExternalImport` hace `find_if` con `foldedName` de ambos lados y `find` lineal de símbolos (`LibraryReferenceValidation.cpp:11-23`). |
| PERF-0172 | Índice LLVM existente sin acceso de solo lectura | PENDIENTE | RESUELTO | `functionForMangledName` ya da acceso indexado (`OopLlvmClosuresDelegatesEvents.cpp:251`, `OopLlvmThunksAndWrappers.cpp:147`). |
| PERF-0173 | Búsqueda lineal de la función destructora de closures | PENDIENTE | RESUELTO | `functionForMangledName` ya da acceso indexado (`OopLlvmClosuresDelegatesEvents.cpp:251`, `OopLlvmThunksAndWrappers.cpp:147`). |
| PERF-0174 | Búsqueda lineal de funciones callback MIR | PENDIENTE | RESUELTO | `functionForMangledName` ya da acceso indexado (`OopLlvmClosuresDelegatesEvents.cpp:251`, `OopLlvmThunksAndWrappers.cpp:147`). |
| PERF-0175 | Revisión exacta de Git que servirá como origen del oráculo de rendimiento | PENDIENTE | APLICA | Proceso de medición: no hay revisión, comandos ni separación frío/caliente congelados en el repositorio. |
| PERF-0176 | Registro reproducible del ejecutable, versión y `CodegenIdentity` del oráculo | PENDIENTE | APLICA | Proceso de medición: no hay revisión, comandos ni separación frío/caliente congelados en el repositorio. |
| PERF-0177 | Registro reproducible del toolchain y configuración usados por el host O2 | PENDIENTE | APLICA | Proceso de medición: no hay revisión, comandos ni separación frío/caliente congelados en el repositorio. |
| PERF-0178 | Registro de procesador, memoria, almacenamiento y estado térmico de la máquina de medición | PENDIENTE | APLICA | Proceso de medición: no hay revisión, comandos ni separación frío/caliente congelados en el repositorio. |
| PERF-0179 | Congelación del comando exacto del escenario hola mundo con Core | PENDIENTE | APLICA | Proceso de medición: no hay revisión, comandos ni separación frío/caliente congelados en el repositorio. |
| PERF-0180 | Congelación del comando exacto del proyecto mediano | PENDIENTE | APLICA | Proceso de medición: no hay revisión, comandos ni separación frío/caliente congelados en el repositorio. |
| PERF-0181 | Congelación del comando exacto del build repetido sin cambios | PENDIENTE | APLICA | Proceso de medición: no hay revisión, comandos ni separación frío/caliente congelados en el repositorio. |
| PERF-0182 | Separación reproducible entre caché fría y caché caliente | PENDIENTE | APLICA | Proceso de medición: no hay revisión, comandos ni separación frío/caliente congelados en el repositorio. |
| PERF-0183 | Captura basal de cinco muestras del escenario hola mundo frío | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0184 | Captura basal de cinco muestras del escenario hola mundo caliente | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0185 | Captura basal de cinco muestras del proyecto mediano frío | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0186 | Captura basal de cinco muestras del proyecto mediano caliente | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0187 | Manifiesto basal de dumps HIR por escenario | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0188 | Manifiesto basal de dumps MIR por escenario | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0189 | Manifiesto basal de LLVM IR por escenario | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0190 | Manifiesto basal de hashes de objetos por unidad | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0191 | Manifiesto basal de ejecutables, bibliotecas y PDB | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0192 | Manifiesto basal de NSMETA y superficies públicas generadas | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0193 | Captura basal de diagnósticos, códigos de salida y salida estándar | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0194 | Cierre verificable del oráculo R0 con hashes, comandos y procedencia | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Proceso de medición. La auditoría incremental del 2026-10-22 midió escenarios y fríos con scripts en `A:` (ver su documento), pero el oráculo R0 con estos campos no está formalizado. |
| PERF-0195 | Ausencia de un campo `time` en las opciones de `nsc build` | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0196 | Ausencia de un campo `timeTrace` en las opciones de `nsc build` | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0197 | Falta de reconocimiento de `--time` por el parser de `nsc build` | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0198 | Falta de reconocimiento de `--time-trace=<ruta>` por `nsc build` | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0199 | Falta de validación de argumento vacío para `--time-trace` | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0200 | Falta de reenvío de `--time` al driver anidado | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0201 | Falta de reenvío de `--time-trace` al driver anidado | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0202 | Ausencia de ayuda CLI para las dos opciones de medición | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0203 | Cobertura focal del parseo y reenvío de `--time` | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0204 | Cobertura focal del parseo y reenvío de `--time-trace` | PENDIENTE | APLICA | `nsc build` rechaza cualquier opción desconocida (`ProjectCommands.cpp:1025-1028`) y no conoce `--time`/`--time-trace`; `appendNestedCompilerOptions` sólo reenvía raíz de bibliotecas, `-j` y progreso (`:1391-1417`). `--time-trace` sólo lo maneja el lanzador `src/nsc/main.cpp:267,302`. |
| PERF-0205 | Ausencia de interruptores centralizados para instrumentación LLVM | PENDIENTE | RESUELTO | `LlvmInstrumentationSwitches.h:5-6` centraliza `kColdLlvmTraceEnabled = false` y `kLoweringPhaseTraceEnabled = false`; `ColdLlvmTrace` y `tracePhase` salen por `if constexpr` antes de abrir archivo, cronómetro o mutex (`ColdLlvmTrace.cpp:25,49,70`; `OopLlvmImplementation.cpp:214`). |
| PERF-0206 | Apertura y truncado incondicional del archivo de `ColdLlvmTrace` | PENDIENTE — REQUIERE AUTORIZACIÓN | RESUELTO | `LlvmInstrumentationSwitches.h:5-6` centraliza `kColdLlvmTraceEnabled = false` y `kLoweringPhaseTraceEnabled = false`; `ColdLlvmTrace` y `tracePhase` salen por `if constexpr` antes de abrir archivo, cronómetro o mutex (`ColdLlvmTrace.cpp:25,49,70`; `OopLlvmImplementation.cpp:214`). |
| PERF-0207 | `flush` incondicional por unidad en `ColdLlvmTrace` | PENDIENTE — REQUIERE AUTORIZACIÓN | RESUELTO | `LlvmInstrumentationSwitches.h:5-6` centraliza `kColdLlvmTraceEnabled = false` y `kLoweringPhaseTraceEnabled = false`; `ColdLlvmTrace` y `tracePhase` salen por `if constexpr` antes de abrir archivo, cronómetro o mutex (`ColdLlvmTrace.cpp:25,49,70`; `OopLlvmImplementation.cpp:214`). |
| PERF-0208 | Estado de stream construido aun con la traza LLVM desactivada | PENDIENTE — REQUIERE AUTORIZACIÓN | RESUELTO | `LlvmInstrumentationSwitches.h:5-6` centraliza `kColdLlvmTraceEnabled = false` y `kLoweringPhaseTraceEnabled = false`; `ColdLlvmTrace` y `tracePhase` salen por `if constexpr` antes de abrir archivo, cronómetro o mutex (`ColdLlvmTrace.cpp:25,49,70`; `OopLlvmImplementation.cpp:214`). |
| PERF-0209 | Cronómetro adquirido aunque `tracePhase` no deba registrar | PENDIENTE — REQUIERE AUTORIZACIÓN | RESUELTO | `LlvmInstrumentationSwitches.h:5-6` centraliza `kColdLlvmTraceEnabled = false` y `kLoweringPhaseTraceEnabled = false`; `ColdLlvmTrace` y `tracePhase` salen por `if constexpr` antes de abrir archivo, cronómetro o mutex (`ColdLlvmTrace.cpp:25,49,70`; `OopLlvmImplementation.cpp:214`). |
| PERF-0210 | Mutex global adquirido aunque `tracePhase` no deba registrar | PENDIENTE — REQUIERE AUTORIZACIÓN | RESUELTO | `LlvmInstrumentationSwitches.h:5-6` centraliza `kColdLlvmTraceEnabled = false` y `kLoweringPhaseTraceEnabled = false`; `ColdLlvmTrace` y `tracePhase` salen por `if constexpr` antes de abrir archivo, cronómetro o mutex (`ColdLlvmTrace.cpp:25,49,70`; `OopLlvmImplementation.cpp:214`). |
| PERF-0211 | Apertura y cierre de archivo por fase y módulo en `tracePhase` | PENDIENTE — REQUIERE AUTORIZACIÓN | RESUELTO | `LlvmInstrumentationSwitches.h:5-6` centraliza `kColdLlvmTraceEnabled = false` y `kLoweringPhaseTraceEnabled = false`; `ColdLlvmTrace` y `tracePhase` salen por `if constexpr` antes de abrir archivo, cronómetro o mutex (`ColdLlvmTrace.cpp:25,49,70`; `OopLlvmImplementation.cpp:214`). |
| PERF-0212 | Generación incondicional de la función de traza runtime con debug completo | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | Sigue activa con `debug=full`: `ensureRuntimeDebugTrace` genera la función, `flags = 7` y la ruta fija `A:\temp\nsc-runtime-debug-trace.log` dentro del programa (`OopLlvmRuntimeDebugTrace.cpp:13-39`). Ver hallazgo N-04. |
| PERF-0213 | Emisión de llamadas de entrada de función para la traza runtime | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | Sigue activa con `debug=full`: `ensureRuntimeDebugTrace` genera la función, `flags = 7` y la ruta fija `A:\temp\nsc-runtime-debug-trace.log` dentro del programa (`OopLlvmRuntimeDebugTrace.cpp:13-39`). Ver hallazgo N-04. |
| PERF-0214 | Emisión de llamadas de asignación y liberación para la traza runtime | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | Sigue activa con `debug=full`: `ensureRuntimeDebugTrace` genera la función, `flags = 7` y la ruta fija `A:\temp\nsc-runtime-debug-trace.log` dentro del programa (`OopLlvmRuntimeDebugTrace.cpp:13-39`). Ver hallazgo N-04. |
| PERF-0215 | Emisión de llamadas de stores de campo para la traza runtime | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | Sigue activa con `debug=full`: `ensureRuntimeDebugTrace` genera la función, `flags = 7` y la ruta fija `A:\temp\nsc-runtime-debug-trace.log` dentro del programa (`OopLlvmRuntimeDebugTrace.cpp:13-39`). Ver hallazgo N-04. |
| PERF-0216 | Equivalencia de IR y artefactos cuando las trazas permanecen habilitadas | PENDIENTE | COBERTURA PENDIENTE | Sin prueba de equivalencia de IR con trazas encendidas. |
| PERF-0217 | Medición comparativa individual de cada traza habilitada y deshabilitada | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Medición pendiente. |
| PERF-0218 | Registro explícito de la decisión contractual sobre la traza runtime | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | Falta la decisión contractual (ver N-04). |
| PERF-0219 | Inventario completo de entradas que determinan `runtime.obj` | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0220 | Identidad estable de versión del generador de runtime | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0221 | Inclusión del perfil de pánico en la clave de `runtime.obj` | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0222 | Inclusión de debug, optimización y características en la clave de `runtime.obj` | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0223 | Namespace persistente propio para entradas de `runtime.obj` | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0224 | Metadata persistente de tamaño y digest para `runtime.obj` | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0225 | Consulta de caché antes de construir el módulo runtime | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0226 | Restauración atómica de un `runtime.obj` válido | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0227 | Verificación COFF del `runtime.obj` restaurado | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0228 | Construcción del runtime únicamente después de un miss válido | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0229 | Optimización LLVM del runtime únicamente después de un miss | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0230 | Almacenamiento atómico del `runtime.obj` recién producido | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0231 | Cuarentena y reconstrucción de entradas corruptas de runtime | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0232 | Exclusión mutua de escritores concurrentes de `runtime.obj` | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0233 | Cobertura focal de hit, miss, corrupción e invalidación de runtime | PENDIENTE | PARCIAL | `WindowsLibraryBuilder` sí restaura/guarda `runtime.obj` en caché (`WindowsLibraryBuilder.cpp:547-585`), pero `WindowsExecutableBuilder` construye y optimiza el runtime en cada enlace (`WindowsExecutableBuilder.cpp:587-590`) y no tiene caché de `runtime.obj` (0 coincidencias de `restoreObject/storeObject`). |
| PERF-0234 | Inventario canónico por DLL de símbolos requeridos para import libraries | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0235 | Orden canónico de símbolos usado en la clave de cada import library | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0236 | Inclusión de arquitectura e identidad de NSLink en la clave de import libraries | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0237 | Namespace persistente propio para import libraries | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0238 | Metadata persistente de tamaño y digest para cada import library | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0239 | Consulta de caché antes de lanzar NSLink por DLL | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0240 | Restauración atómica de una import library válida | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0241 | Verificación COFF de la import library restaurada | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0242 | Lanzamiento de NSLink únicamente después de un miss válido | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0243 | Almacenamiento atómico de la import library recién producida | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0244 | Cuarentena y reconstrucción de import libraries corruptas | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0245 | Exclusión mutua de escritores concurrentes por identidad de DLL | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0246 | Preservación textual de diagnósticos cuando falla NSLink | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0247 | Cobertura focal fría, caliente, corrupta e invalidada de import libraries | PENDIENTE | PARCIAL | Las import libraries de las DLL canónicas del runtime vienen precompiladas del SDK (`findCanonicalWindowsImportLibraries`, `WindowsRuntimeImports.cpp:387-406`); las de DLL externas del proyecto se regeneran en cada build con `createImportLibrary` sin caché (`:356-371`). |
| PERF-0248 | Demostración de cero procesos NSLink en un build caliente sin cambios | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Demostrable sólo en proyectos sin imports externos; con imports externos sigue habiendo proceso por DLL. |
| PERF-0249 | Inventario de todas las entradas que determinan `public-api` precompilada | PENDIENTE — REQUIERE DISEÑO | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0250 | Inventario de todas las entradas que determinan `public-templates` | PENDIENTE — REQUIERE DISEÑO | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0251 | Identidad estable de versión del emisor de superficie pública | PENDIENTE — REQUIERE DISEÑO | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0252 | Inclusión del digest del payload NSMETA en la clave de superficie | PENDIENTE — REQUIERE DISEÑO | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0253 | Inclusión de `CodegenIdentity` en la clave de superficie | PENDIENTE — REQUIERE DISEÑO | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0254 | Inclusión de opciones semánticas relevantes en la clave de superficie | PENDIENTE — REQUIERE DISEÑO | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0255 | Tipo propietario de la caché de superficies en archivos propios | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0256 | Namespace persistente independiente para superficies precompiladas | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0257 | Cabecera versionada del payload de superficie pública | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0258 | Escritura atómica del texto `public-api` | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0259 | Escritura atómica del texto `public-templates` | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0260 | Metadata de tamaño y digest para ambos textos | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0261 | Bloqueo de escritores concurrentes por módulo e identidad | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0262 | Cuarentena de superficies truncadas o corruptas | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0263 | Consulta de caché antes de ejecutar `PublicSurfaceBuilder` | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0264 | Omisión completa de `PublicSurfaceBuilder` durante un hit válido | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0265 | Restauración del nombre virtual original de `public-api` | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0266 | Restauración del nombre virtual original de `public-templates` | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0267 | Preservación del orden y ordinales de las unidades virtuales restauradas | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0268 | Fallback frío ante miss, versión distinta o entrada corrupta | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0269 | Invalidación por cambio del NSMETA de entrada | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0270 | Invalidación por cambio del compilador o del emisor | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0271 | Integración de superficies con la política común de poda | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0272 | Contadores separados de hit, miss, corrupción e invalidación | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0273 | Cobertura de determinismo al emitir dos veces la misma metadata | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0274 | Cobertura focal de caché fría y caliente por módulo | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0275 | Cobertura focal de corrupción e invalidación de superficies | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0276 | Igualdad byte a byte de `public-api` y `public-templates` restauradas | PENDIENTE | SUPERADO | Superado por T3/T4: el consumidor ya no ejecuta `PublicSurfaceBuilder`; materializa las unidades `public-api`/`public-templates` desde el producto sintáctico publicado (`ProjectCompilation.cpp:1360-1369,1494-1496`). |
| PERF-0277 | Igualdad de dumps y artefactos entre superficie fría y caliente | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Medición pendiente. |
| PERF-0278 | Medición del frontend evitado por módulo precompilado | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Medición pendiente. |
| PERF-0279 | Inventario de productores y consumidores actuales de `TemplateParseCache` | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0280 | Llave calculable antes de lexear o parsear una plantilla | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0281 | Inclusión del digest del texto fuente en la llave de plantillas | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0282 | Inclusión de versión de lenguaje, parser y compilador en la llave | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0283 | Inclusión de opciones e imports que alteran el parseo en la llave | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0284 | Separación de la raíz persistente respecto de la caché MIR | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0285 | Extensión y cabecera exclusivas para payloads de plantillas | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0286 | Serialización completa del resultado de parseo reutilizable | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0287 | Preservación de rangos, archivos y posiciones de cada nodo restaurado | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0288 | Restauración del AST dentro del ownership de la arena activa | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0289 | Reasignación determinista de ordinales de archivo y unidad restaurados | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0290 | Consulta de caché antes del lexer de plantillas | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0291 | Omisión del parser durante un hit válido de plantillas | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0292 | Política que impide almacenar resultados con errores | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0293 | Fallback al lexer y parser ante miss o incompatibilidad | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0294 | Cuarentena de payloads de plantillas corruptos | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0295 | Escritura atómica sin `WRITE_THROUGH` por cada archivo | PENDIENTE — REQUIERE AUTORIZACIÓN | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0296 | Exclusión mutua de escritores concurrentes de la misma plantilla | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0297 | Invalidación por cambio de fuente, opciones o identidad del parser | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0298 | Integración de la caché de plantillas con la poda común | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0299 | Contadores separados de parseos ejecutados y evitados | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0300 | Cobertura focal de round trip del AST de plantillas | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0301 | Cobertura de diagnósticos idénticos en miss y entrada inválida | PENDIENTE | DESCONECTADO | HECHO: los consumidores existen (`Generics/Cache/TemplateCacheBridge.cpp`, `SpecializationExecutor.cpp:264-469`), pero `ProjectGenericModel::setTemplateCacheRoot` (`ProjectGenericModel.cpp:825`) no tiene llamadores, así que `templateCache_` nunca se habilita. Desconexión de cf0a94725. |
| PERF-0302 | Igualdad de dumps y artefactos con caché de plantillas fría y caliente | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Medición pendiente; requiere reconectar. |
| PERF-0303 | Medición del lexing y parseo evitados por la caché de plantillas | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Medición pendiente; requiere reconectar. |
| PERF-0304 | Inventario de productores y consumidores actuales de `MirCache` | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0305 | Llave MIR calculable antes de construir las funciones MIR | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0306 | Huella determinista del HIR de entrada para la llave MIR | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0307 | Inclusión de layouts, bindings y estáticos relevantes en la llave MIR | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0308 | Inclusión de opciones semánticas, debug y target en la llave MIR | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0309 | Inclusión de versión de formato y compilador en la llave MIR | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0310 | Separación de la raíz persistente respecto de plantillas | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0311 | Extensión y cabecera exclusivas para payloads MIR | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0312 | Consulta de caché antes de `buildMirFunctions` | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0313 | Uso efectivo del resultado restaurado en vez de descartarlo | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0314 | Restauración ordenada de `functions_` y sus ordinales | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0315 | Reconstrucción de mapas auxiliares desde funciones MIR restauradas | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0316 | Integración de resultados restaurados con layouts y dispatch actuales | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0317 | Composición determinista de hits y misses dentro de un proyecto | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0318 | Preservación del orden global cuando sólo algunas funciones se restauran | PENDIENTE — REQUIERE DISEÑO | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0319 | Política que impide almacenar MIR después de un error | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0320 | Fallback a lowering MIR ante miss o incompatibilidad | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0321 | Cuarentena de payloads MIR corruptos | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0322 | Escritura atómica y bloqueo por identidad MIR | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0323 | Invalidación por cambios de HIR, modelos, opciones o compilador | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0324 | Integración de la caché MIR con la poda común | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0325 | Contadores separados de funciones MIR construidas y restauradas | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0326 | Cobertura focal de round trip de todos los kinds MIR soportados | PENDIENTE | DESCONECTADO | HECHO: el flujo está cableado (`MirBuildCoordinator.cpp:713-714,762`, `MirFunctionLowerer.cpp:879`), pero `setMirCacheRoot` (`MirCache.cpp:1460`) no tiene llamadores y `mirCache_` nunca se habilita. Además la llave global incluye `ProjectOopHirModelDumper::dump(hirModel)` completo (`MirCache.cpp:1480-1481`): cualquier cambio invalida todas las funciones. Desconexión de 82615f62f. |
| PERF-0327 | Igualdad de dumps, objetos y ejecutables con MIR frío y caliente | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Medición pendiente; requiere rediseño de llave. |
| PERF-0328 | Medición del lowering MIR evitado por la caché rediseñada | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Medición pendiente; requiere rediseño de llave. |
| PERF-0329 | Registro de tamaño y mtime del artefacto al copiar durante restore | PENDIENTE — REQUIERE DISEÑO | APLICA | `verifyPlan` rehashea todo destino siempre (`ProjectCommands.cpp:564-579`) y `executeRestore` reescribe el lock con `atomicWrite` en cada restore no bloqueado (`:694`). |
| PERF-0330 | Prefiltro conservador por tamaño antes de rehash del destino | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | `verifyPlan` rehashea todo destino siempre (`ProjectCommands.cpp:564-579`) y `executeRestore` reescribe el lock con `atomicWrite` en cada restore no bloqueado (`:694`). |
| PERF-0331 | Prefiltro conservador por mtime antes de rehash del destino | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | `verifyPlan` rehashea todo destino siempre (`ProjectCommands.cpp:564-579`) y `executeRestore` reescribe el lock con `atomicWrite` en cada restore no bloqueado (`:694`). |
| PERF-0332 | Preservación de la detección de corrupción cuando el prefiltro no coincide | PENDIENTE | APLICA | `verifyPlan` rehashea todo destino siempre (`ProjectCommands.cpp:564-579`) y `executeRestore` reescribe el lock con `atomicWrite` en cada restore no bloqueado (`:694`). |
| PERF-0333 | Lectura del lock existente antes de intentar reescribirlo | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | `verifyPlan` rehashea todo destino siempre (`ProjectCommands.cpp:564-579`) y `executeRestore` reescribe el lock con `atomicWrite` en cada restore no bloqueado (`:694`). |
| PERF-0334 | Comparación byte a byte del lock existente con el nuevo | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | `verifyPlan` rehashea todo destino siempre (`ProjectCommands.cpp:564-579`) y `executeRestore` reescribe el lock con `atomicWrite` en cada restore no bloqueado (`:694`). |
| PERF-0335 | Preservación del mtime del lock cuando su contenido no cambia | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | `verifyPlan` rehashea todo destino siempre (`ProjectCommands.cpp:564-579`) y `executeRestore` reescribe el lock con `atomicWrite` en cada restore no bloqueado (`:694`). |
| PERF-0336 | Cobertura focal de restore intacto, modificado y corrupto | PENDIENTE | APLICA | `verifyPlan` rehashea todo destino siempre (`ProjectCommands.cpp:564-579`) y `executeRestore` reescribe el lock con `atomicWrite` en cada restore no bloqueado (`:694`). |
| PERF-0337 | Consulta de `loadedByModule` antes de releer un NSMETA ya cargado | PENDIENTE | RESUELTO | INFERENCIA: la relectura se resuelve en `LibraryMetadataRepository` (memo `entries_` por ruta), no en `loadedByModule`. |
| PERF-0338 | Preservación del diagnóstico de dos rutas distintas para un módulo | PENDIENTE | COBERTURA PENDIENTE | Diagnóstico de rutas distintas presente (`PrecompiledProjectPreparation.cpp:367-381`); sin prueba focal. |
| PERF-0339 | Identidad canónica de NSMETA por ruta, tamaño, mtime y digest | PENDIENTE — REQUIERE DISEÑO | RESUELTO | `LibraryMetadataRepository` (memo por ruta) + `VerifiedLibraryMetadataCache` persistente (`LibraryMetadataRepository.cpp:40-148`), dentro de `ProjectCompilationLifetime` (T4/T7). |
| PERF-0340 | Almacén por proceso de metadata NSMETA verificada | PENDIENTE — REQUIERE DISEÑO | RESUELTO | `LibraryMetadataRepository` (memo por ruta) + `VerifiedLibraryMetadataCache` persistente (`LibraryMetadataRepository.cpp:40-148`), dentro de `ProjectCompilationLifetime` (T4/T7). |
| PERF-0341 | Propagación de metadata verificada desde configuración hasta generación | PENDIENTE — REQUIERE DISEÑO | RESUELTO | `LibraryMetadataRepository` (memo por ruta) + `VerifiedLibraryMetadataCache` persistente (`LibraryMetadataRepository.cpp:40-148`), dentro de `ProjectCompilationLifetime` (T4/T7). |
| PERF-0342 | Eliminación de la segunda decodificación en `validateLibraryReference` | PENDIENTE — REQUIERE AUTORIZACIÓN | RESUELTO | INFERENCIA: `validateLibraryReference` vuelve a pedir `decode`/`verify` al repositorio (`LibraryReferenceValidation.cpp:39-62`), que los resuelve desde su entrada memoizada. No se midió. |
| PERF-0343 | Eliminación de la segunda autenticación de metadata inmutable | PENDIENTE — REQUIERE AUTORIZACIÓN | RESUELTO | INFERENCIA: `validateLibraryReference` vuelve a pedir `decode`/`verify` al repositorio (`LibraryReferenceValidation.cpp:39-62`), que los resuelve desde su entrada memoizada. No se midió. |
| PERF-0344 | Reutilización de `publishedNativeSymbols` junto con la metadata | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | No se verificó. |
| PERF-0345 | Detección de reemplazo de NSMETA entre configuración y enlace | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | No se verificó. |
| PERF-0346 | Ownership acotado de la caché NSMETA sin estado estático residual | PENDIENTE | RESUELTO | La caché vive en `ProjectCompilationLifetime` (`ProjectCompilationLifetime.h:25-35`), sin estado estático. |
| PERF-0347 | Cobertura focal de NSMETA repetido, reemplazado y conflictivo | PENDIENTE | COBERTURA PENDIENTE | Sin prueba focal de NSMETA reemplazado. |
| PERF-0348 | Identidad canónica de una DLL PE por ruta, tamaño, mtime y digest | PENDIENTE — REQUIERE DISEÑO | APLICA | `PeExecutableVerifier` no memoiza; se instancia y verifica por referencia y otra vez después del enlace (`LibraryReferenceValidation.cpp:123-124,437`). |
| PERF-0349 | Memo por proceso del resultado de verificación PE | PENDIENTE — REQUIERE DISEÑO | APLICA | `PeExecutableVerifier` no memoiza; se instancia y verifica por referencia y otra vez después del enlace (`LibraryReferenceValidation.cpp:123-124,437`). |
| PERF-0350 | Reutilización del resultado PE en validación inicial de runtime | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | `PeExecutableVerifier` no memoiza; se instancia y verifica por referencia y otra vez después del enlace (`LibraryReferenceValidation.cpp:123-124,437`). |
| PERF-0351 | Reutilización del resultado PE en verificación posterior al enlace | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | `PeExecutableVerifier` no memoiza; se instancia y verifica por referencia y otra vez después del enlace (`LibraryReferenceValidation.cpp:123-124,437`). |
| PERF-0352 | Detección de reemplazo de DLL entre ambas verificaciones | PENDIENTE — REQUIERE DISEÑO | APLICA | `PeExecutableVerifier` no memoiza; se instancia y verifica por referencia y otra vez después del enlace (`LibraryReferenceValidation.cpp:123-124,437`). |
| PERF-0353 | Preservación textual de todos los diagnósticos PE | PENDIENTE | APLICA | `PeExecutableVerifier` no memoiza; se instancia y verifica por referencia y otra vez después del enlace (`LibraryReferenceValidation.cpp:123-124,437`). |
| PERF-0354 | Cobertura focal de DLL válida, inválida, truncada y reemplazada | PENDIENTE | APLICA | `PeExecutableVerifier` no memoiza; se instancia y verifica por referencia y otra vez después del enlace (`LibraryReferenceValidation.cpp:123-124,437`). |
| PERF-0355 | Formato versionado del sello `last-prune` de la caché | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay sello `last-prune`; con caché por defecto `pruneInvocationArtifactCache` inspecciona todas las entradas en cada invocación (`CompilerSession.cpp:78-96`, `CompilationArtifactCache.cpp:522-575`). |
| PERF-0356 | Lectura tolerante a ausencia o corrupción del sello de poda | PENDIENTE | APLICA | No hay sello `last-prune`; con caché por defecto `pruneInvocationArtifactCache` inspecciona todas las entradas en cada invocación (`CompilerSession.cpp:78-96`, `CompilationArtifactCache.cpp:522-575`). |
| PERF-0357 | Política explícita del intervalo mínimo entre podas | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | No hay sello `last-prune`; con caché por defecto `pruneInvocationArtifactCache` inspecciona todas las entradas en cada invocación (`CompilerSession.cpp:78-96`, `CompilationArtifactCache.cpp:522-575`). |
| PERF-0358 | Exclusión mutua de procesos que intentan podar la misma raíz | PENDIENTE | APLICA | No hay sello `last-prune`; con caché por defecto `pruneInvocationArtifactCache` inspecciona todas las entradas en cada invocación (`CompilerSession.cpp:78-96`, `CompilationArtifactCache.cpp:522-575`). |
| PERF-0359 | Actualización atómica del sello después de una poda completa | PENDIENTE | APLICA | No hay sello `last-prune`; con caché por defecto `pruneInvocationArtifactCache` inspecciona todas las entradas en cada invocación (`CompilerSession.cpp:78-96`, `CompilationArtifactCache.cpp:522-575`). |
| PERF-0360 | Poda inmediata conservada cuando se excede el límite duro | PENDIENTE | APLICA | No hay sello `last-prune`; con caché por defecto `pruneInvocationArtifactCache` inspecciona todas las entradas en cada invocación (`CompilerSession.cpp:78-96`, `CompilationArtifactCache.cpp:522-575`). |
| PERF-0361 | Recuperación automática si una poda queda interrumpida | PENDIENTE | APLICA | No hay sello `last-prune`; con caché por defecto `pruneInvocationArtifactCache` inspecciona todas las entradas en cada invocación (`CompilerSession.cpp:78-96`, `CompilationArtifactCache.cpp:522-575`). |
| PERF-0362 | Cobertura focal de sello reciente, vencido, corrupto y concurrente | PENDIENTE | APLICA | No hay sello `last-prune`; con caché por defecto `pruneInvocationArtifactCache` inspecciona todas las entradas en cada invocación (`CompilerSession.cpp:78-96`, `CompilationArtifactCache.cpp:522-575`). |
| PERF-0363 | Medición conjunta de hashing, verificación y poda evitados | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Medición pendiente. |
| PERF-0364 | Inventario de todos los mutadores en sitio de `TypeSyntax` | PENDIENTE — REQUIERE DISEÑO | APLICA | `cloneAliasType` se sigue llamando en 20 sitios, incluido cada parámetro y campo en `prepareNodeScope` (`GenericNodeProcessor.cpp:160,170`). |
| PERF-0365 | Demostración de qué nodos hoja permanecen inmutables tras la instantánea | PENDIENTE — REQUIERE DISEÑO | APLICA | `cloneAliasType` se sigue llamando en 20 sitios, incluido cada parámetro y campo en `prepareNodeScope` (`GenericNodeProcessor.cpp:160,170`). |
| PERF-0366 | Contrato de ownership y lifetime para compartir nodos hoja | PENDIENTE — REQUIERE DISEÑO | APLICA | `cloneAliasType` se sigue llamando en 20 sitios, incluido cada parámetro y campo en `prepareNodeScope` (`GenericNodeProcessor.cpp:160,170`). |
| PERF-0367 | Omisión de `cloneAliasType` únicamente para hojas demostradas inmutables | PENDIENTE — REQUIERE DISEÑO | APLICA | `cloneAliasType` se sigue llamando en 20 sitios, incluido cada parámetro y campo en `prepareNodeScope` (`GenericNodeProcessor.cpp:160,170`). |
| PERF-0368 | Cobertura de alias, const genéricos, orígenes y mutaciones tardías | PENDIENTE | APLICA | `cloneAliasType` se sigue llamando en 20 sitios, incluido cada parámetro y campo en `prepareNodeScope` (`GenericNodeProcessor.cpp:160,170`). |
| PERF-0369 | Inventario de lectura y escritura de las ocho pasadas genéricas | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: no se observó rediseño de las ocho pasadas; requiere diseño. |
| PERF-0370 | Grafo de dependencias de datos entre las ocho pasadas genéricas | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: no se observó rediseño de las ocho pasadas; requiere diseño. |
| PERF-0371 | Demostración del orden mínimo equivalente entre pasadas | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: no se observó rediseño de las ocho pasadas; requiere diseño. |
| PERF-0372 | Banderas por unidad para resultados genéricos ya calculados | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: no se observó rediseño de las ocho pasadas; requiere diseño. |
| PERF-0373 | Reutilización de resultados genéricos precompilados junto con NSMETA | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: no se observó rediseño de las ocho pasadas; requiere diseño. |
| PERF-0374 | Fusión de pasadas que compartan recorrido sin dependencia cruzada | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: no se observó rediseño de las ocho pasadas; requiere diseño. |
| PERF-0375 | Preservación del orden exacto de diagnósticos de genéricos | PENDIENTE | APLICA | INFERENCIA: no se observó rediseño de las ocho pasadas; requiere diseño. |
| PERF-0376 | Cobertura diferencial de las ocho pasadas antes y después del rediseño | PENDIENTE | APLICA | INFERENCIA: no se observó rediseño de las ocho pasadas; requiere diseño. |
| PERF-0377 | Inventario de dependencias en los ordinales de las unidades públicas 0 y 1 | PENDIENTE — REQUIERE DISEÑO | APLICA | Siguen dos unidades virtuales fijas por módulo, ordinales 0 y 1 (`ProjectCompilation.cpp:1367,1495`). |
| PERF-0378 | Regla determinista para particionar superficie pública por namespace | PENDIENTE — REQUIERE DISEÑO | APLICA | Siguen dos unidades virtuales fijas por módulo, ordinales 0 y 1 (`ProjectCompilation.cpp:1367,1495`). |
| PERF-0379 | Nombres virtuales estables para unidades públicas particionadas | PENDIENTE — REQUIERE DISEÑO | APLICA | Siguen dos unidades virtuales fijas por módulo, ordinales 0 y 1 (`ProjectCompilation.cpp:1367,1495`). |
| PERF-0380 | Orden estable de unidades y declaraciones después de particionar | PENDIENTE — REQUIERE DISEÑO | APLICA | Siguen dos unidades virtuales fijas por módulo, ordinales 0 y 1 (`ProjectCompilation.cpp:1367,1495`). |
| PERF-0381 | Preservación de posiciones y diagnósticos entre unidades particionadas | PENDIENTE — REQUIERE DISEÑO | APLICA | Siguen dos unidades virtuales fijas por módulo, ordinales 0 y 1 (`ProjectCompilation.cpp:1367,1495`). |
| PERF-0382 | Cobertura byte a byte del texto agregado antes y después de particionar | PENDIENTE | APLICA | Siguen dos unidades virtuales fijas por módulo, ordinales 0 y 1 (`ProjectCompilation.cpp:1367,1495`). |
| PERF-0383 | Medición del paralelismo real de lex y parse por unidad pública | PENDIENTE — VALIDACIÓN DEL USUARIO | APLICA | Siguen dos unidades virtuales fijas por módulo, ordinales 0 y 1 (`ProjectCompilation.cpp:1367,1495`). |
| PERF-0384 | Medición certificada del tamaño y población real de cada clase AST | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0385 | Demostración del orden actual de destrucción de nodos AST | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0386 | Diseño de bloques de arena con tamaño y alineación correctos | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0387 | Construcción por placement new conservando direcciones estables | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0388 | Registro de destructores y destrucción inversa equivalente | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0389 | Recuperación correcta cuando un constructor AST lanza o falla | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0390 | Inventario de campos raros candidatos a `AstNodeExtras` | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0391 | Almacenamiento perezoso de extras sin cambiar accesores públicos | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0392 | Preservación de semántica de copia y movimiento de nodos AST | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0393 | Compatibilidad con pruebas que inspeccionan miembros privados del AST | PENDIENTE | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0394 | Cobertura de lifetime, direcciones y destrucción de la arena por bloques | PENDIENTE | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0395 | Medición de bytes y asignaciones por nodo antes y después | PENDIENTE — VALIDACIÓN DEL USUARIO | NO VERIFICADO | Existe `AstArena` con `absorb` (`AST/AstArena.h`); no se verificó si ya es arena por bloques con placement new. |
| PERF-0396 | Ownership explícito del `ProjectGraph` compartido con el driver anidado | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: `executeBuild` carga el grafo (`ProjectCommands.cpp:1324`) y el driver anidado lo vuelve a cargar desde el proyecto generado. |
| PERF-0397 | Interfaz interna para entregar el grafo ya cargado al driver anidado | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: `executeBuild` carga el grafo (`ProjectCommands.cpp:1324`) y el driver anidado lo vuelve a cargar desde el proyecto generado. |
| PERF-0398 | Eliminación de la recarga de la raíz dentro del driver anidado | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: `executeBuild` carga el grafo (`ProjectCommands.cpp:1324`) y el driver anidado lo vuelve a cargar desde el proyecto generado. |
| PERF-0399 | Preservación del orden, diagnósticos y reemplazos del grafo recargado | PENDIENTE | APLICA | INFERENCIA: `executeBuild` carga el grafo (`ProjectCommands.cpp:1324`) y el driver anidado lo vuelve a cargar desde el proyecto generado. |
| PERF-0400 | Cobertura diferencial de carga única frente a carga repetida del grafo | PENDIENTE | APLICA | INFERENCIA: `executeBuild` carga el grafo (`ProjectCommands.cpp:1324`) y el driver anidado lo vuelve a cargar desde el proyecto generado. |
| PERF-0401 | Medición aislada del costo de destruir `ProjectCompilation` | PENDIENTE — VALIDACIÓN DEL USUARIO | APLICA | No hay ruta de salida rápida ni abandono controlado de `ProjectCompilation`. |
| PERF-0402 | Ownership explícito de `ProjectCompilation` en la ruta CLI de vida corta | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay ruta de salida rápida ni abandono controlado de `ProjectCompilation`. |
| PERF-0403 | Conservación de destrucción normal en bibliotecas, IDE y pruebas | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay ruta de salida rápida ni abandono controlado de `ProjectCompilation`. |
| PERF-0404 | Decisión contractual sobre liberar o abandonar memoria al salir del proceso | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | No hay ruta de salida rápida ni abandono controlado de `ProjectCompilation`. |
| PERF-0405 | Inventario de mapas de shards de cuerpos construidos sin ser consultados | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | No se localizaron mapas de shards con esos nombres. |
| PERF-0406 | Inicialización perezosa independiente de cada mapa de shard | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | No se localizaron mapas de shards con esos nombres. |
| PERF-0407 | Invalidación y reconstrucción correcta de mapas perezosos reutilizados | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | No se localizaron mapas de shards con esos nombres. |
| PERF-0408 | Compatibilidad de mapas perezosos con pruebas que acceden miembros privados | PENDIENTE | NO VERIFICADO | No se localizaron mapas de shards con esos nombres. |
| PERF-0409 | Inventario de todo el estado mutable de un lowerer MIR por función | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: no existe lowerer MIR reutilizable por hilo. |
| PERF-0410 | Rutina completa de reset que no conserve estado entre funciones | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: no existe lowerer MIR reutilizable por hilo. |
| PERF-0411 | Ownership de un lowerer MIR reutilizable por hilo de trabajo | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: no existe lowerer MIR reutilizable por hilo. |
| PERF-0412 | Cobertura de equivalencia y contaminación entre funciones consecutivas | PENDIENTE | APLICA | INFERENCIA: no existe lowerer MIR reutilizable por hilo. |
| PERF-0413 | Inventario del estado capturado por la `std::function` de `buildVisibility` | PENDIENTE — REQUIERE DISEÑO | APLICA | `std::function<void(std::size_t)> visit` recursiva (`TypeSpellingAndVisibility.cpp:415`). |
| PERF-0414 | Sustitución iterativa de la recursión de visibilidad con el mismo orden | PENDIENTE — REQUIERE DISEÑO | APLICA | `std::function<void(std::size_t)> visit` recursiva (`TypeSpellingAndVisibility.cpp:415`). |
| PERF-0415 | Cobertura de ciclos, profundidad y diagnósticos de visibilidad iterativa | PENDIENTE | APLICA | `std::function<void(std::size_t)> visit` recursiva (`TypeSpellingAndVisibility.cpp:415`). |
| PERF-0416 | Hash transparente de `knownTypeNames_` sin alterar su conjunto ni orden observable | PENDIENTE — REQUIERE DISEÑO | RESUELTO | Implementado (T3.3): `knownTypeNames_` es `KnownTypeNameSet` con hash transparente (`Parser.h:1692`). |
| PERF-0417 | Cobertura comparativa del hash transparente frente al scratch reutilizado | PENDIENTE | COBERTURA PENDIENTE | La validación unitaria de T3.3 sigue pendiente (plan O0, 59/60). |
| PERF-0418 | Estado explícito para sustituir la recursión de `callTypeIsSend` | PENDIENTE — REQUIERE DISEÑO | APLICA | `callTypeIsSend`/`callTypeIsSync` siguen recursivas (`CallAnalyzer.cpp:4874-4937`). |
| PERF-0419 | Estado explícito para sustituir la recursión de `callTypeIsSync` | PENDIENTE — REQUIERE DISEÑO | APLICA | `callTypeIsSend`/`callTypeIsSync` siguen recursivas (`CallAnalyzer.cpp:4874-4937`). |
| PERF-0420 | Cobertura de ciclos y resultados idénticos para `Send` y `Sync` iterativos | PENDIENTE | APLICA | `callTypeIsSend`/`callTypeIsSync` siguen recursivas (`CallAnalyzer.cpp:4874-4937`). |
| PERF-0421 | Inventario de strings y vectores copiados de HIR a MIR por nodo | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay identidad internada de `typeKey`. |
| PERF-0422 | Identidad internada de `typeKey` compartible entre HIR y MIR | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay identidad internada de `typeKey`. |
| PERF-0423 | Cobertura de ownership y equivalencia para datos HIR/MIR compartidos | PENDIENTE | APLICA | No hay identidad internada de `typeKey`. |
| PERF-0424 | Ejecución focal de cada target nuevo de las tandas 1 a 7 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0425 | Ejecución focal de cada target nuevo de las tandas 8 a 14 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0426 | Confirmación de 3,149 de 3,149 unitarias con el producto final | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0427 | Confirmación de 6,026 de 6,026 pruebas externas con el producto final | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0428 | Comparación final byte a byte de dumps HIR contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0429 | Comparación final byte a byte de dumps MIR contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0430 | Comparación final byte a byte de LLVM IR contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0431 | Comparación final de NSMETA contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0432 | Comparación final de objetos por unidad contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0433 | Comparación final de ejecutables y bibliotecas contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0434 | Comparación final de PDB y rutas de depuración contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0435 | Comparación final de diagnósticos y códigos de salida contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0436 | Cinco mediciones finales de hola mundo con caché fría | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0437 | Cinco mediciones finales de hola mundo con caché caliente | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0438 | Cinco mediciones finales del proyecto mediano con caché fría | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0439 | Cinco mediciones finales del proyecto mediano con caché caliente | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación final del usuario. |
| PERF-0440 | Informe de medianas, dispersión, hits, misses y procesos evitados por tanda | PENDIENTE | APLICA | Proceso: informe y gate final pendientes. |
| PERF-0441 | Gate aritmético final de 670 terminados y cero pendientes | PENDIENTE | APLICA | Proceso: informe y gate final pendientes. |
| PERF-0442 | Directorio O2 independiente en `A:` para el host de medición | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación del usuario. |
| PERF-0443 | Confirmación binaria de que el host medido usa O2 y no conserva `/Od` | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Validación del usuario. |
| PERF-0444 | Inventario de dependencias y orden entre verificaciones NSMETA | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay verificación paralela de NSMETA: el pool sólo se usa para cargar proyectos (`ProjectGraphLoader.cpp:286-303`). |
| PERF-0445 | Lectura y autenticación paralela de NSMETA mediante el pool existente | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay verificación paralela de NSMETA: el pool sólo se usa para cargar proyectos (`ProjectGraphLoader.cpp:286-303`). |
| PERF-0446 | Resultados NSMETA aislados por ordinal durante el trabajo paralelo | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay verificación paralela de NSMETA: el pool sólo se usa para cargar proyectos (`ProjectGraphLoader.cpp:286-303`). |
| PERF-0447 | Publicación secuencial de resultados NSMETA en el orden vigente | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay verificación paralela de NSMETA: el pool sólo se usa para cargar proyectos (`ProjectGraphLoader.cpp:286-303`). |
| PERF-0448 | Preservación del primer diagnóstico NSMETA según el orden actual | PENDIENTE | APLICA | No hay verificación paralela de NSMETA: el pool sólo se usa para cargar proyectos (`ProjectGraphLoader.cpp:286-303`). |
| PERF-0449 | Cancelación determinista de workers NSMETA después del primer fallo publicable | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay verificación paralela de NSMETA: el pool sólo se usa para cargar proyectos (`ProjectGraphLoader.cpp:286-303`). |
| PERF-0450 | Cobertura diferencial de verificación NSMETA secuencial y paralela | PENDIENTE | APLICA | No hay verificación paralela de NSMETA: el pool sólo se usa para cargar proyectos (`ProjectGraphLoader.cpp:286-303`). |
| PERF-0451 | Inventario de rutas absolutas incorporadas a debug info y a claves de objetos | PENDIENTE — REQUIERE DISEÑO | APLICA | Inventario no hecho. |
| PERF-0452 | Reenvío de `-fdebug-prefix-map` desde `nsc build` al driver anidado | PENDIENTE | APLICA | `nsc build` no acepta ni reenvía `-fdebug-prefix-map` (`ProjectCommands.cpp:968-1031,1391-1417`); el compilador sí lo soporta (`invocation_.debugPrefixMaps`, `CompilerActionDispatcher.cpp:711-726`). |
| PERF-0453 | Regla estable para relativizar raíces temporales sin tocar fuentes reales | PENDIENTE — REQUIERE DISEÑO | APLICA | Pendiente de diseño; ver PERF-0452. |
| PERF-0454 | Inclusión determinista del prefix map en la identidad de compilación | PENDIENTE — REQUIERE DISEÑO | APLICA | Pendiente de diseño; ver PERF-0452. |
| PERF-0455 | Opción del compilador para una raíz de caché de artefactos estable y externa al proyecto | PENDIENTE — REQUIERE DISEÑO | APLICA | Pendiente de diseño; ver PERF-0452. |
| PERF-0456 | Preservación explícita del contrato de rutas del PDB | PENDIENTE — REQUIERE AUTORIZACIÓN | APLICA | Pendiente de diseño; ver PERF-0452. |
| PERF-0457 | Cobertura de PDB y diagnósticos con y sin prefix map | PENDIENTE | APLICA | Pendiente de diseño; ver PERF-0452. |
| PERF-0458 | Demostración de hit de objetos entre dos raíces temporales equivalentes | PENDIENTE — VALIDACIÓN DEL USUARIO | APLICA | Pendiente de diseño; ver PERF-0452. |
| PERF-0459 | Verificación de disponibilidad del escritor COFF de import libraries en LLVM vigente | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay escritor en proceso de import libraries (0 coincidencias de `writeImportLibrary`/`COFFShortExport`). |
| PERF-0460 | Modelo de entrada canónico para una import library escrita en proceso | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay escritor en proceso de import libraries (0 coincidencias de `writeImportLibrary`/`COFFShortExport`). |
| PERF-0461 | Escritura en proceso del formato short import COFF | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay escritor en proceso de import libraries (0 coincidencias de `writeImportLibrary`/`COFFShortExport`). |
| PERF-0462 | Orden determinista de miembros y tabla de símbolos de la import library | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay escritor en proceso de import libraries (0 coincidencias de `writeImportLibrary`/`COFFShortExport`). |
| PERF-0463 | Diagnósticos equivalentes del escritor en proceso | PENDIENTE | APLICA | No hay escritor en proceso de import libraries (0 coincidencias de `writeImportLibrary`/`COFFShortExport`). |
| PERF-0464 | Comparación de símbolos y bytes frente a la salida canónica de NSLink | PENDIENTE | APLICA | No hay escritor en proceso de import libraries (0 coincidencias de `writeImportLibrary`/`COFFShortExport`). |
| PERF-0465 | Fallback a NSLink cuando el escritor en proceso no soporta una entrada | PENDIENTE — REQUIERE DISEÑO | APLICA | No hay escritor en proceso de import libraries (0 coincidencias de `writeImportLibrary`/`COFFShortExport`). |
| PERF-0466 | Reserva superior de `loaded` antes de expandir `binaryDependencies` | PENDIENTE | APLICA | `expandBinaryClosure` copia el vector `binaryDependencies` de cada nodo para no invalidar referencias y no reserva `loaded` (`PrecompiledProjectPreparation.cpp:628-651`). |
| PERF-0467 | Sustitución de referencias inestables por ordinales al expandir dependencias binarias | PENDIENTE — REQUIERE DISEÑO | APLICA | `expandBinaryClosure` copia el vector `binaryDependencies` de cada nodo para no invalidar referencias y no reserva `loaded` (`PrecompiledProjectPreparation.cpp:628-651`). |
| PERF-0468 | Eliminación de la copia completa de texto IR al calcular la clave incremental del runtime | PENDIENTE — REQUIERE DISEÑO | APLICA | INFERENCIA: `prepareIncrementalExecutable` recibe `programLlvmText` y el módulo runtime (`WindowsExecutableBuilder.cpp:593-597`); no se verificó la materialización de texto. |
| PERF-0469 | Cobertura de identidad incremental del runtime sin materializar su texto completo | PENDIENTE | APLICA | INFERENCIA: `prepareIncrementalExecutable` recibe `programLlvmText` y el módulo runtime (`WindowsExecutableBuilder.cpp:593-597`); no se verificó la materialización de texto. |
| PERF-0470 | Precomputación de identidades canónicas de módulos en `ProjectGraph` | PENDIENTE | APLICA | `canonicalWindowsModuleIdentity` se recalcula, incluso dentro de un comparador (`ProjectGraph.cpp:461-463`). |
| PERF-0471 | Índice de descriptores precompilados únicos sin `find_if` por elemento | PENDIENTE | APLICA | `collectUniquePrecompiledDescriptors` hace `find_if` sobre proyectos por descriptor (`ProjectGraph.cpp:209-211`). |
| PERF-0472 | Cobertura de caché global estable sin modificar NSC Builder ni el IDE | PENDIENTE | NO VERIFICADO | No verificado. |
| PERF-0473 | Inventario de la condición que desactiva la caché pre-LLVM con debug completo | PENDIENTE — REQUIERE DISEÑO | APLICA | HECHO: `usePreLlvmCache` exige `debugInformation != Full` y no ThinLTO (`ProjectOopObjectGenerator.cpp:1011-1013`). |
| PERF-0474 | Inventario de datos de debug que deben formar parte de la llave pre-LLVM | PENDIENTE — REQUIERE DISEÑO | APLICA | HECHO: `usePreLlvmCache` exige `debugInformation != Full` y no ThinLTO (`ProjectOopObjectGenerator.cpp:1011-1013`). |
| PERF-0475 | Identidad estable de rutas y checksums para objetos con debug completo | PENDIENTE — REQUIERE DISEÑO | APLICA | HECHO: `usePreLlvmCache` exige `debugInformation != Full` y no ThinLTO (`ProjectOopObjectGenerator.cpp:1011-1013`). |
| PERF-0476 | Restauración de objetos pre-LLVM con debug completo sin perder información | PENDIENTE — REQUIERE DISEÑO | APLICA | HECHO: `usePreLlvmCache` exige `debugInformation != Full` y no ThinLTO (`ProjectOopObjectGenerator.cpp:1011-1013`). |
| PERF-0477 | Invalidación de objetos debug por fuente, prefix map, target y opciones | PENDIENTE — REQUIERE DISEÑO | APLICA | HECHO: `usePreLlvmCache` exige `debugInformation != Full` y no ThinLTO (`ProjectOopObjectGenerator.cpp:1011-1013`). |
| PERF-0478 | Cobertura focal fría y caliente de la caché pre-LLVM con `debug=full` | PENDIENTE | APLICA | HECHO: `usePreLlvmCache` exige `debugInformation != Full` y no ThinLTO (`ProjectOopObjectGenerator.cpp:1011-1013`). |
| PERF-0479 | Medición de hits pre-LLVM con `debug=full` | PENDIENTE — VALIDACIÓN DEL USUARIO | APLICA | HECHO: `usePreLlvmCache` exige `debugInformation != Full` y no ThinLTO (`ProjectOopObjectGenerator.cpp:1011-1013`). |
| PERF-0480 | Inventario de entradas que determinan la llave de enlace incremental | PENDIENTE — REQUIERE DISEÑO | PARCIAL | El enlace incremental existe como opción (`--incremental-link`, `WindowsExecutableBuilder.cpp:593-617`), pero el runtime se construye y optimiza antes de calcular la llave (`:587-590`); PDB y llave por texto no verificados. |
| PERF-0481 | Inventario de artefactos de enlace y PDB reutilizables entre builds | PENDIENTE — REQUIERE DISEÑO | PARCIAL | El enlace incremental existe como opción (`--incremental-link`, `WindowsExecutableBuilder.cpp:593-617`), pero el runtime se construye y optimiza antes de calcular la llave (`:587-590`); PDB y llave por texto no verificados. |
| PERF-0482 | Eliminación de materializaciones de texto usadas sólo para la llave incremental | PENDIENTE — REQUIERE DISEÑO | PARCIAL | El enlace incremental existe como opción (`--incremental-link`, `WindowsExecutableBuilder.cpp:593-617`), pero el runtime se construye y optimiza antes de calcular la llave (`:587-590`); PDB y llave por texto no verificados. |
| PERF-0483 | Validación del ejecutable restaurado antes de omitir el enlace final | PENDIENTE — REQUIERE DISEÑO | PARCIAL | El enlace incremental existe como opción (`--incremental-link`, `WindowsExecutableBuilder.cpp:593-617`), pero el runtime se construye y optimiza antes de calcular la llave (`:587-590`); PDB y llave por texto no verificados. |
| PERF-0484 | Validación y restauración coherente del PDB asociado | PENDIENTE — REQUIERE DISEÑO | PARCIAL | El enlace incremental existe como opción (`--incremental-link`, `WindowsExecutableBuilder.cpp:593-617`), pero el runtime se construye y optimiza antes de calcular la llave (`:587-590`); PDB y llave por texto no verificados. |
| PERF-0485 | Invalidación por cambio de objeto, import library, opción o linker | PENDIENTE — REQUIERE DISEÑO | PARCIAL | El enlace incremental existe como opción (`--incremental-link`, `WindowsExecutableBuilder.cpp:593-617`), pero el runtime se construye y optimiza antes de calcular la llave (`:587-590`); PDB y llave por texto no verificados. |
| PERF-0486 | Cobertura focal de enlace incremental frío, caliente y corrupto | PENDIENTE | PARCIAL | El enlace incremental existe como opción (`--incremental-link`, `WindowsExecutableBuilder.cpp:593-617`), pero el runtime se construye y optimiza antes de calcular la llave (`:587-590`); PDB y llave por texto no verificados. |
| PERF-0487 | Medición de procesos de enlace final y generación PDB evitados | PENDIENTE — VALIDACIÓN DEL USUARIO | VALIDACIÓN DEL USUARIO | Medición pendiente. |
| PERF-0488 | Inventario de claves y lifetime del `ProjectLoaderCache` actual | PENDIENTE — REQUIERE DISEÑO | RESUELTO | Inventario: `ProjectLoaderCache` es un mapa por ruta con mutex, propiedad compartida del `ProjectLoader` (`Compilation/Internal/ProjectLoaderCache.h:13-19`). |
| PERF-0489 | Demostración de qué cargas se repiten dentro de un mismo proceso | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | No se verificó invalidación ni repetición real. |
| PERF-0490 | Extensión acotada del lifetime del loader sin conservar archivos obsoletos | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | No se verificó invalidación ni repetición real. |
| PERF-0491 | Invalidación del loader por ruta, tamaño, mtime y contenido | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | No se verificó invalidación ni repetición real. |
| PERF-0492 | Cobertura de hit, modificación y desaparición para `ProjectLoaderCache` | PENDIENTE | NO VERIFICADO | No se verificó invalidación ni repetición real. |
| PERF-0493 | Inventario de consultas y tasa de hit del `ComptimeCache` en una compilación | PENDIENTE — REQUIERE DISEÑO | APLICA | `ComptimeCache` se crea por fase sintáctica sólo si hay comptime (`ProjectCompilation.cpp:1294-1299`); no hay reutilización entre compilaciones. |
| PERF-0494 | Demostración de reutilización posible entre unidades o compilaciones | PENDIENTE — REQUIERE DISEÑO | APLICA | `ComptimeCache` se crea por fase sintáctica sólo si hay comptime (`ProjectCompilation.cpp:1294-1299`); no hay reutilización entre compilaciones. |
| PERF-0495 | Identidad e invalidación completas para resultados comptime persistentes | PENDIENTE — REQUIERE DISEÑO | APLICA | `ComptimeCache` se crea por fase sintáctica sólo si hay comptime (`ProjectCompilation.cpp:1294-1299`); no hay reutilización entre compilaciones. |
| PERF-0496 | Cobertura diferencial de resultados comptime reutilizados | PENDIENTE | APLICA | `ComptimeCache` se crea por fase sintáctica sólo si hay comptime (`ProjectCompilation.cpp:1294-1299`); no hay reutilización entre compilaciones. |
| PERF-0497 | Cierre del inventario de cachés con productor, consumidor, llave e invalidador para cada una | PENDIENTE | APLICA | Este documento avanza el inventario (sección 4), pero el cierre formal sigue pendiente. |
| PERF-0498 | Consumo del índice de templates por nombre en `resolveNameOfTemplateGenericMethod` | PENDIENTE | APLICA | `resolveNameOfTemplateGenericMethod` recorre todos los templates y arma nombres por llamada (`PrimaryExpressions.cpp:588-614`). |
| PERF-0499 | Cobertura de orden, duplicados y fallback de `resolveNameOfTemplateGenericMethod` | PENDIENTE | APLICA | `resolveNameOfTemplateGenericMethod` recorre todos los templates y arma nombres por llamada (`PrimaryExpressions.cpp:588-614`). |
| PERF-0500 | Medición de llamadas y locales inspeccionados por las consultas de ownership | PENDIENTE — REQUIERE DISEÑO | APLICA | `conflictingMutableReference` y `hasActiveBorrowOfOwnerLocal` siguen barriendo locales (`OwnershipAnalyzer.cpp:623,745`). |
| PERF-0501 | Índice por lugar y alcance para referencias mutables activas del callable | PENDIENTE — REQUIERE DISEÑO | APLICA | `conflictingMutableReference` y `hasActiveBorrowOfOwnerLocal` siguen barriendo locales (`OwnershipAnalyzer.cpp:623,745`). |
| PERF-0502 | Sustitución del barrido completo de `locals_` en `conflictingMutableReference` | PENDIENTE — REQUIERE DISEÑO | APLICA | `conflictingMutableReference` y `hasActiveBorrowOfOwnerLocal` siguen barriendo locales (`OwnershipAnalyzer.cpp:623,745`). |
| PERF-0503 | Eliminación del string temporal `"L" + to_string(localOrdinal)` en ownership | PENDIENTE | NO VERIFICADO | No se verificó el string temporal. |
| PERF-0504 | Índice de préstamos locales activos por ordinal del propietario | PENDIENTE — REQUIERE DISEÑO | APLICA | `conflictingMutableReference` y `hasActiveBorrowOfOwnerLocal` siguen barriendo locales (`OwnershipAnalyzer.cpp:623,745`). |
| PERF-0505 | Índice de reasignaciones de parámetros prestados por ordinal del propietario | PENDIENTE — REQUIERE DISEÑO | APLICA | `conflictingMutableReference` y `hasActiveBorrowOfOwnerLocal` siguen barriendo locales (`OwnershipAnalyzer.cpp:623,745`). |
| PERF-0506 | Índice de vistas vivas por raíz local sin recorrer todos los locales | PENDIENTE — REQUIERE DISEÑO | APLICA | `conflictingMutableReference` y `hasActiveBorrowOfOwnerLocal` siguen barriendo locales (`OwnershipAnalyzer.cpp:623,745`). |
| PERF-0507 | Cobertura diferencial de ownership con scopes, reborrows, invalidación y vistas | PENDIENTE | APLICA | `conflictingMutableReference` y `hasActiveBorrowOfOwnerLocal` siguen barriendo locales (`OwnershipAnalyzer.cpp:623,745`). |
| PERF-0508 | Medición de expresiones y búsquedas ejecutadas por `resolveCapabilityCalls` | PENDIENTE — REQUIERE DISEÑO | APLICA | Pendiente de medición. |
| PERF-0509 | Eliminación de la copia por iteración de los descriptores de capability | PENDIENTE | RESUELTO | Los descriptores son `constexpr` de `string_view`; la copia por iteración es trivial (`ComptimeEngine.cpp:363-368`). |
| PERF-0510 | Resolución en una sola pasada de llamadas capability si la medición la justifica | PENDIENTE — REQUIERE DISEÑO | APLICA | Pendiente de diseño. |
| PERF-0511 | Cobertura diferencial de sustituciones, dependencias y diagnósticos comptime | PENDIENTE | COBERTURA PENDIENTE | Sin cobertura diferencial focal. |
| PERF-0512 | Índice por clave compuesta para propietarios de implementaciones de traits | PENDIENTE — REQUIERE DISEÑO | APLICA | `findTraitImplementationOwner` es lineal (`TraitImplementationMerger.cpp:152-166`). |
| PERF-0513 | Preservación del primer template coincidente en el índice de traits | PENDIENTE — REQUIERE DISEÑO | APLICA | `findTraitImplementationOwner` es lineal (`TraitImplementationMerger.cpp:152-166`). |
| PERF-0514 | Sustitución del recorrido lineal en `findTraitImplementationOwner` | PENDIENTE — REQUIERE DISEÑO | APLICA | `findTraitImplementationOwner` es lineal (`TraitImplementationMerger.cpp:152-166`). |
| PERF-0515 | Cobertura diferencial de propietarios de traits con claves repetidas | PENDIENTE | APLICA | `findTraitImplementationOwner` es lineal (`TraitImplementationMerger.cpp:152-166`). |
| PERF-0516 | Índice MIR compartido `mangledName -> primer ordinal` durante el lowerer | PENDIENTE — REQUIERE DISEÑO | APLICA | `inheritMoveNextDropFrames` e `inheritConstructorFrames` son O(F²) por nombre (`MirFunctionLowerer.cpp:646-699`). |
| PERF-0517 | Sustitución del bucle interno de `inheritMoveNextDropFrames` | PENDIENTE | APLICA | `inheritMoveNextDropFrames` e `inheritConstructorFrames` son O(F²) por nombre (`MirFunctionLowerer.cpp:646-699`). |
| PERF-0518 | Sustitución del bucle interno de `inheritConstructorFrames` | PENDIENTE | APLICA | `inheritMoveNextDropFrames` e `inheritConstructorFrames` son O(F²) por nombre (`MirFunctionLowerer.cpp:646-699`). |
| PERF-0519 | Preservación determinista del primer nombre MIR coincidente y fallback | PENDIENTE | APLICA | `inheritMoveNextDropFrames` e `inheritConstructorFrames` son O(F²) por nombre (`MirFunctionLowerer.cpp:646-699`). |
| PERF-0520 | Cobertura diferencial de frames async, generators y funciones ausentes | PENDIENTE | APLICA | `inheritMoveNextDropFrames` e `inheritConstructorFrames` son O(F²) por nombre (`MirFunctionLowerer.cpp:646-699`). |
| PERF-0521 | Índice por proyecto, fuente y offset para targets de atributos comptime | PENDIENTE — REQUIERE DISEÑO | APLICA | `findAttributeTarget` recorre tipos y miembros; `findType` es `ranges::find`; `visibleMembers` copia a scratch (`ProjectMetaprogrammingModel.cpp:186-229,702-721`). |
| PERF-0522 | Sustitución de los recorridos de tipos y miembros en `findAttributeTarget` | PENDIENTE — REQUIERE DISEÑO | APLICA | `findAttributeTarget` recorre tipos y miembros; `findType` es `ranges::find`; `visibleMembers` copia a scratch (`ProjectMetaprogrammingModel.cpp:186-229,702-721`). |
| PERF-0523 | Índice por identidad para `ProjectMetaprogrammingModel::findType` | PENDIENTE — REQUIERE DISEÑO | APLICA | `findAttributeTarget` recorre tipos y miembros; `findType` es `ranges::find`; `visibleMembers` copia a scratch (`ProjectMetaprogrammingModel.cpp:186-229,702-721`). |
| PERF-0524 | Vista pública precalculada de miembros para `visibleMembers` | PENDIENTE — REQUIERE DISEÑO | APLICA | `findAttributeTarget` recorre tipos y miembros; `findType` es `ranges::find`; `visibleMembers` copia a scratch (`ProjectMetaprogrammingModel.cpp:186-229,702-721`). |
| PERF-0525 | Cobertura de orden y visibilidad de consultas de metaprogramación | PENDIENTE | APLICA | `findAttributeTarget` recorre tipos y miembros; `findType` es `ranges::find`; `visibleMembers` copia a scratch (`ProjectMetaprogrammingModel.cpp:186-229,702-721`). |
| PERF-0526 | Verificación estática y medida de que `finalizeExternalMetadataTypes` es una sola pasada | PENDIENTE — REQUIERE DISEÑO | RESUELTO | Verificado estáticamente: una sola pasada sobre dispatch y exports (`LibraryMetadataConstruction.cpp:969-1011`). |
| PERF-0527 | Criterio explícito para no reescribir `finalizeExternalMetadataTypes` si no existe producto cartesiano | PENDIENTE — REQUIERE DISEÑO | NO APLICA | No existe producto cartesiano; no se reescribe. |
| PERF-0528 | Índice de constructores generator por `memberOrdinal` en MIR | PENDIENTE — REQUIERE DISEÑO | APLICA | `resolveMemberPhysicalIdentity` recorre todas las funciones MIR si no es constructor generator (`LibraryMetadataConstruction.cpp:2362-2373`). |
| PERF-0529 | Sustitución del recorrido de funciones en `resolveMemberPhysicalIdentity` | PENDIENTE | APLICA | `resolveMemberPhysicalIdentity` recorre todas las funciones MIR si no es constructor generator (`LibraryMetadataConstruction.cpp:2362-2373`). |
| PERF-0530 | Cobertura de identidad física normal, async y generator | PENDIENTE | APLICA | `resolveMemberPhysicalIdentity` recorre todas las funciones MIR si no es constructor generator (`LibraryMetadataConstruction.cpp:2362-2373`). |
| PERF-0531 | Decoración o comparación sin strings temporales en `canonicalize` de NSMETA | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | La instanciación revisada devuelve `const std::string&` (`LibraryMetadataMerger.cpp:241-256`); las otras instanciaciones no se revisaron. |
| PERF-0532 | Preservación exacta de precedencia, conflicto y texto diagnóstico de `canonicalize` | PENDIENTE — REQUIERE DISEÑO | NO VERIFICADO | La instanciación revisada devuelve `const std::string&` (`LibraryMetadataMerger.cpp:241-256`); las otras instanciaciones no se revisaron. |
| PERF-0533 | Medición de copias y ordenamientos en `canonicalizeNestedRecords` | PENDIENTE — REQUIERE DISEÑO | APLICA | Pendiente de medición. |
| PERF-0534 | Cobertura byte a byte del merge y canonicalización de NSMETA | PENDIENTE | COBERTURA PENDIENTE | Sin cobertura byte a byte focal. |
| PERF-0535 | Verificación de que el accesor de evento pertenece al tipo declarante | PENDIENTE — REQUIERE DISEÑO | RESUELTO | `recordEventAssignmentBinding` usa `findMembers(event.declaringTypeOrdinal, ...)` (`AssignmentAnalyzer.cpp:788-800`). |
| PERF-0536 | Sustitución segura del recorrido de `recordEventAssignmentBinding` por lookup indexado | PENDIENTE — REQUIERE DISEÑO | RESUELTO | `recordEventAssignmentBinding` usa `findMembers(event.declaringTypeOrdinal, ...)` (`AssignmentAnalyzer.cpp:788-800`). |
| PERF-0537 | Cobertura de orden y diagnóstico para bindings de asignación de eventos | PENDIENTE | COBERTURA PENDIENTE | Sin cobertura focal. |
| PERF-0538 | Clasificación trazable de las 150 entradas del ranking del apéndice B | PENDIENTE — REQUIERE DISEÑO | APLICA | Proceso documental pendiente. |
| PERF-0539 | Gate inverso: cada ID `PERF-*` debe declarar un hallazgo fuente | PENDIENTE | APLICA | Proceso documental pendiente. |
| PERF-0540 | Gate directo: cada hallazgo accionable debe declarar uno o más IDs `PERF-*` | PENDIENTE | APLICA | Proceso documental pendiente. |
| PERF-0541 | Reconciliación del cierre histórico de C01/C04 con su ineficacia actual | PENDIENTE | APLICA | Este documento deja constancia (secciones 3 y 4): C01/C04 están implementados y desconectados; falta reconciliar el cierre histórico. |
| PERF-0542 | Clasificación explícita de H-1..H-9 entre rendimiento, refactor y fuera de alcance | PENDIENTE | APLICA | Proceso documental pendiente. |

## 8. Limitaciones

- No se ejecutó nada para esta auditoría. Lo marcado INFERENCIA debe confirmarse con medición o prueba antes de
  actuar.
- Se revisaron las funciones nombradas en el tracking y sus llamadores directos. Funciones renombradas o movidas
  que no se encontraron por nombre quedaron en NO VERIFICADO.
- La clasificación de cobertura sólo busca targets focales en `CMakeLists.txt`. Una prueba general puede cubrir parte
  de lo pedido sin ser el target focal que el renglón exige.

## Apéndice A. Herramientas usadas

- Scripts temporales de solo lectura en `A:\temp\nsc-audit`, con TEMP en `A:`: `funcscan.py`, `probe.py`, `show.py`
  y sus listas de sondas. Están fuera del repositorio; su borrado se confirma aparte con el usuario.
- No se escribió nada en `C:` ni se crearon worktrees o copias del repositorio.
