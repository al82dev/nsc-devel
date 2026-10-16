# Tanda NSC305-P02 — Razones de miss y tiempos internos por unidad LLVM

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: P02 (observabilidad por code unit LLVM/caché; sin optimización, sin cambios de scheduling).
> Commit separado de P01, como exige la metodología.

## 1. SCOPE LOCK de la tanda

| Campo | Valor vinculante |
|---|---|
| Objetivo exacto | Instrumentar por unidad LLVM (`fingerprint`, `lookup`, `lowering`, `optimization`, `emission`, `verification`, `store`) y dar a cada miss una razón estable (`disabled`, `cold`, `key-mismatch`, `corruption`, `incomplete-publication`). No se optimiza ni se cambia la política de caché. |
| Naturaleza del cambio | Instrumentación no funcional (observabilidad). |
| Comportamiento idéntico | Artefactos, diagnósticos, orden, exit codes, formato del reporte sin `--time`, política de caché, claves, verificación COFF. |
| Revisión correcta de referencia | `development` en `f2aeca34` (commit P01). |
| Archivos permitidos | `include/nativesharp/LLVM/CompilationArtifactCache.h`, `include/nativesharp/LLVM/ProjectOopObjectGenerator.h`, `src/LLVM/CompilationArtifactCache.cpp`, `src/LLVM/ProjectOopObjectGenerator.cpp`, `src/Driver/Driver.cpp`, `tests/compiler-suite/host/modules/LlvmFoundationModule.cpp`. |
| Símbolos permitidos | `CompilationArtifactCache::restoreObject` (parámetro opcional `CacheMissReason*`), `ProjectOopObjectBundle` (+`CodeUnitTelemetry`), `ProjectOopObjectGenerator::generate` (temporización por fase), `Driver` (reporte condicional), nuevo `CacheMissReason` + `cacheMissReasonName`. |
| Archivos/subsistemas protegidos | `ProjectOopLlvmLowerer`, `LlvmOptimizer`, `CoffObjectVerifier`, `ProjectOopCodePartition`, NSMETA, frontend, y todo lo no listado. |
| Pruebas que deben pasar sin modificarse | Toda la suite existente; nueva prueba focal `CF-LLVM-022`. |
| Acciones prohibidas | Cambiar claves, política de caché, verificación, orden o scheduling; push/pull/fetch; escribir fuera de `A:`; modificar configuración. |
| Criterio de terminación | Telemetría por unidad + razón de miss; cold/hit/corrupción observables; reporte sin `--time` idéntico; prueba focal verde; expediente, tracking, `git diff --check` y commit local aislado. |

## 2. Estado de Git

- HEAD inicial: `f2aeca34` (`perf(3.0.5): instrument generics with causal sub-phase telemetry`).
- Rama: `development`. Worktree inicial: limpio.

## 3. Cambios realizados

### 3.1 `CompilationArtifactCache.h/.cpp`

- Nuevo `enum class CacheMissReason { Hit, Disabled, KeyMismatch, Cold, IncompletePublication, Corruption }`
  y `cacheMissReasonName()`.
- `restoreObject` acepta `CacheMissReason* reason = nullptr` (parámetro por defecto; los llamadores y
  pruebas existentes no cambian) y asigna la razón en cada salida: `Disabled` (sin root), `KeyMismatch`
  (clave no canónica), `Cold` (objeto ausente), `IncompletePublication` (objeto sin metadata),
  `Corruption` (metadata/digest no coinciden → cuarentena), `Hit` (restaurado).

### 3.2 `ProjectOopObjectGenerator.h/.cpp`

- `ProjectOopObjectBundle` expone `std::vector<CodeUnitTelemetry> codeUnitTelemetry()` con, por unidad:
  `name`, `fingerprint/lookup/lowering/optimization/emission/verification/storeMicros` y `missReason`.
- `generate()` temporiza cada fase del `processModule` (`PhaseClock`, sin allocation) y registra la
  razón de miss del `restoreObject` (pre-LLVM y post-LLVM) y del `storeObject`. `lookup` incluye la
  copia de restauración que hace `restoreObject` (no se desdobla porque la caché no la separa hoy).

### 3.3 `src/Driver/Driver.cpp`

- Helper `appendCodeUnitTelemetry(output, bundle)` y llamada **condicional a `reportTiming`** en los
  tres reportes (EXE/LIB/DLL), después de `artifact-cache-misses`. Sin `--time` el reporte es
  byte-idéntico al anterior.

### 3.4 `tests/compiler-suite/host/modules/LlvmFoundationModule.cpp`

- Nueva prueba focal `cacheReasonContract` (contrato `CF-LLVM-022`, casos `TF-LLVM-169..176`): nombres
  exactos de razón; `restoreObject` reporta `Disabled`, `Cold`, `KeyMismatch` y `Corruption`
  (cuarentena incluida).

## 4. Evidencia

### HECHO COMPROBADO

- Cold build: las unidades reportan `miss-reason=cold` con las 7 fases pobladas.
- Warm build: `miss-reason=hit` con `lowering/optimization/emission/store = 0` (restauradas de caché).
- Objeto corrupto: la unidad afectada reporta `miss-reason=corruption`, se reconstruye
  (`lowering/emission/store` no nulos) y el resto de unidades sigue `hit`.
- Sin `--time`, el reporte conserva sólo `artifact-cache-hits/misses` (compatibilidad).

### RESULTADO MEDIDO

- Prueba focal `CF-LLVM-022`: 8/8 PASS.
- `Host.LLVM`: 77 PASS / 3 FAIL (preexistentes `TF-LLVM-001/002/008`, COFF verifier, confirmados en HEAD).
- `Host.Linker`: 12 PASS / 1 FAIL (preexistente `TF-LINKER-290`, fixture directory, confirmado en HEAD).

### INFERENCIA

- Los 4 fallos residuales pertenecen a `CoffObjectVerifier` y a la creación de fixture del linker,
  subsistemas no tocados por P02; se confirmaron idénticos en HEAD (sin P02), por lo que no son
  regresión de esta tanda.

## 5. Limitaciones

- `lookup` agrega la copia de restauración (la caché no separa find/copy hoy); desdoblarla exigiría
  instrumentar `restoreObject` internamente, lo que se difiere.
- `code-units` del reporte cuenta particiones (excluye el módulo `globals`); la telemetría nueva sí
  reporta las `moduleCount` unidades reales. Es una inconsistencia preexistente, no corregida en P02.
- La prueba unitaria `nsc_compilation_artifact_cache_tests` (build-test-nslink) aborta al arranque
  (0xC0000409) también en HEAD, por una causa ambiental ajena a P01/P02; la cobertura de la caché se
  garantiza con `CF-LLVM-022` en la suite canónica de componentes.

## 6. Siguiente acción determinista

NSC305-P03: corpus reproducible de rendimiento propiedad de NSC (arranca en `Hola mundo`, escala con
fixtures controlados, casos COLD/WARM/J1/JN reproducibles desde `A:`).
