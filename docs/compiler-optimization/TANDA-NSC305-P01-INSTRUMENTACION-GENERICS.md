# Tanda NSC305-P01 — Instrumentación causal interna de `generics`

> Campaña NativeSharp Compiler 3.0.5 (rendimiento, incrementalidad, caché, paralelismo determinista,
> eliminación de recursión caliente y modularización interna). Único producto: `nsc`.
> Tanda: P01 (observabilidad; sin optimización, sin caché, sin cambios de scheduling).

## 1. SCOPE LOCK de la tanda

| Campo | Valor vinculante |
|---|---|
| Objetivo exacto | Añadir observabilidad interna a la fase `generics`: 14 subfases temporizadas, contadores agregados y desglose por template. No se optimiza ningún algoritmo, no se introduce caché, no se cambia scheduling ni se reescribe `ProjectGenericModel`. |
| Naturaleza del cambio | Instrumentación no funcional (observabilidad). |
| Comportamiento idéntico | Sintaxis, semántica observable, diagnósticos (orden y códigos), ABI/layouts, ownership/lifetime, mangling/nombres físicos, formatos públicos, dumps, artefactos, O0/O1/O2/O3. |
| Revisión correcta de referencia | `development` en `f2e9418663f0ff2c90776b445cdbf8430415708f` (antecesor directo de esta tanda). |
| Rango de commits evaluado | `a96572eaf..f2e941866` (auditado íntegro antes de editar). |
| Archivos permitidos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`, `src/Frontend/ProjectCompilation.cpp`, `tests/compiler-suite/host/modules/FrontendGenericsFlowModule.cpp`. |
| Símbolos permitidos | `ProjectGenericModel::build` y sus colaboradores (`collectTemplates`, `resolveTypeTemplate`, `templatesNamedBy`, `ensureTypeSpecialization`, `ensureMethodSpecialization`, `cloneNode`, `cloneType`, `collectBodyTypeSpecializationDemand`); `ProjectCompilation::compileProjectGenerics`; nuevo tipo `GenericTelemetry`. |
| Archivos/subsistemas protegidos | `Driver.cpp`, `CompilationTelemetry.*`, `ProjectGenericModelDumper.*`, `LibraryMetadata.*`, `CMakeLists.txt`, NSMETA, caché de objetos, y todo lo no listado arriba. |
| Pruebas que deben pasar sin modificarse | Toda la suite existente (componente `Host.Frontend`, 536 casos previos) y la nueva prueba focal. |
| Acciones prohibidas | Cambiar control de flujo, orden, recursión, algoritmos, caché o scheduling; tocar dumps o formatos públicos; push/pull/fetch; escribir fuera de `A:`; modificar configuración. |
| Criterio de terminación | Subfases y contadores instrumentados; telemetría desactivable con overhead <1% apagada; trace on/off produce mismos diagnósticos y artefactos; suma de subfases coherente con el timer padre; prueba focal verde; expediente, tracking, `git diff --check` y commit local aislado. |

## 2. Estado de Git

- HEAD inicial: `f2e9418663f0ff2c90776b445cdbf8430415708f` (`docs(3.0.5): scope optimization strictly to NSC`).
- Rama: `development` (no se creó ni cambió ninguna rama).
- Worktree inicial: limpio.
- `v.3.0.4-baseline`: el punto de rescate congelado existe como `origin/v.3.0.4-baseline` apuntando a `a96572eaf73b0f9ef78f64c7d5290a658ed6a976`; no se tocó.

## 3. Cambios realizados

### 3.1 `include/nativesharp/Frontend/ProjectGenericModel.h`

- Se añadió el struct `GenericTelemetry` (observabilidad pura): flags `enabled`/`detailed`, sink de
  temporización, acumulador de subfases (`subPhaseMicros`), 14 contadores agregados, desglose por
  template (`templates`, solo con `detailed`), un temporizador RAII `GenericTelemetry::Scope` y
  helpers `note*` que son no-op si `enabled == false`.
- Se añadió el miembro `telemetry_` y los accesores `telemetry()`.

### 3.2 `src/Frontend/ProjectGenericModel.cpp`

- `build()` envuelve cada bloque secuencial con `GenericTelemetry::Scope`, en este orden:
  `generics.visibility`, `generics.alias-expansion`, `generics.trait-merge`, `generics.opaque-results`,
  `generics.ordinary-return-index`, `generics.associated-items`, `generics.template-collection`,
  `generics.template-index` (dos tramos: desambiguación y detección de duplicados + copia de records),
  `generics.dependency-discovery`, `generics.imported-specialization-seed`,
  `generics.root-demand-discovery` y `generics.body-type-demands`.
- `ensureTypeSpecialization` / `ensureMethodSpecialization`: contador de hit de especialización en el
  camino de caché; `SpecializationWaveTimer` (RAII) alrededor de la materialización que registra
  `generics.specialization-waves` y atribuye el tiempo exclusivo por template; `generics.rewrite-commit`
  alrededor del attach del clon al AST.
- `collectTemplates`: `noteTemplate` en los dos `push_back`.
- `resolveTypeTemplate` / `templatesNamedBy`: `noteResolutionQuery` (consultas + candidatos examinados).
- `cloneNode` / `cloneType`: `noteNodesCloned` / `noteTypesCloned`.
- `collectBodyTypeSpecializationDemand`: `noteDemand` (descubiertas / duplicadas).
- `maximumLogicalDepth` se captura de `instantiationBudget_.maximumObservedDepth()` al final de `build()`.
- `GenericTelemetry::renderSummary()` produce un resumen determinista (orden fijo de subfases y claves).

### 3.3 `src/Frontend/ProjectCompilation.cpp`

- `compileProjectGenerics` configura la telemetría: `timingSink = phaseTimingSink_`,
  `enabled = (phaseTimingSink_ != nullptr)`, `detailed = enabled`, y cuenta `ownUnits` y
  `templateSourceBytes` (de `precompiledModules_`). Sin sink (compilación normal) la telemetría queda
  totalmente inactiva.

### 3.4 `tests/compiler-suite/host/modules/FrontendGenericsFlowModule.cpp`

- Nueva prueba focal `genericTelemetryOracle` (contrato `CF-FRONT-068`, casos `TF-FRONT-537..544`).

## 4. Contadores implementados (mínimos del plan)

`unidades procesadas`, `bytes de template source`, `templates` totales y por clase
(`templatesByKind[5]`), `consultas realizadas`, `candidatos examinados`, `demandas descubiertas`,
`demandas duplicadas`, `demandas materializadas`, `hits de especialización`, `nodos clonados`,
`tipos clonados`, `profundidad lógica`, y `tiempo + cardinalidad por template` (con detalle habilitado).

## 5. Evidencia

### HECHO COMPROBADO

- Con `--time`, `nsc` emite las 14 subfases como líneas `timing phase=generics.*` y el timer padre
  `generics` permanece. Ejemplo real (fixture `Box<int>` + `Identity<int>`):
  - top-level: visibility 110, alias-expansion 72, trait-merge 12, opaque-results 30,
    ordinary-return-index 5, associated-items 12, template-collection 68, template-index 7+16,
    dependency-discovery 61, seed 0, root-demand-discovery 339, body-type-demands 0 → suma ≈ 732 µs.
  - padre `generics` = 832 µs. `suma top-level (732) ≤ padre (832)` (coherente; el hueco es overhead
    entre bloques).
  - anidados `specialization-waves` 99+53 y `rewrite-commit` 4+1 = 157 µs ≤ root-demand-discovery 339 µs.
- El artefacto emitido (`--emit-project-oop-exe`) es **byte-idéntico** con y sin `--time`
  (SHA-256 `EED411DB…093F` en ambos casos).
- La prueba focal (8 casos) pasa; el componente `Host.Frontend` completo pasa **544/544** (0 fallos).

### RESULTADO MEDIDO

- Fixture de 300 especializaciones (`--emit-project-oop-lib`), mediana de 5 ejecuciones:
  - telemetría desactivada (sin `--time`): 1.016,6 ms.
  - telemetría habilitada (`--time`): 1.026,2 ms.
  - **overhead habilitado: 9,54 ms (0,94 %)**.
- El overhead deshabilitado es estructuralmente un subconjunto del habilitado (cada sitio instrumentado
  es una única rama predecible `if (enabled)`), por lo que queda por debajo del 1 % exigido.

### INFERENCIA

- La medición 0,94 % corresponde al camino **habilitado** (temporizadores + sink + mapa por template);
  el camino deshabilitado no ejecuta ese trabajo, de modo que su overhead es inferior a 0,94 %.

## 6. Pruebas

- `nsc_component_tests --case TF-FRONT-537` … `--case TF-FRONT-544`: 8/8 PASS.
- `nsc_component_tests --component Host.Frontend`: 544 PASS / 0 FAIL.
- Smoke manual (`--time`): subfases emitidas y coherentes; artefacto idéntico con/sin `--time`.

## 7. Limitaciones

- El desglose por template (`detailed`) se activa hoy junto con `--time`; no existe todavía una flag CLI
  independiente (`--generic-hotspots`), que corresponde a una tanda posterior si la evidencia lo pide.
- `candidatesExamined` cuenta el catálogo completo por consulta de resolución (proxy del coste de barrido
  lineal), no el número exacto de candidatos realmente comparados por filtro.
- La medición rigurosa del overhead deshabilitado sobre el corpus controlado reproducible queda para
  NSC305-P03; aquí se aporta la cota del camino habilitado (0,94 %).

## 8. Siguiente acción determinista

NSC305-P02: instrumentar por unidad LLVM (`fingerprint`, `lookup`, `restore`, `lowering`,
`optimization`, `emission`, `verification`, `store`) con razón de miss estable por unidad, en un commit
separado. Archivos objetivo: `ProjectOopObjectGenerator::generate`, `CompilationArtifactCache` y
reportes EXE/LIB/DLL.
