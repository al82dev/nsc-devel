# Tanda 09-01 — BuildSystem — implementación e integración

## SCOPE LOCK operativo

- **Objetivo autorizado:** después de constatar que Ola 8 no tenía un componente elegible, materializar únicamente
  el primer componente canónico de Ola 9 que pertenece a la suite NSC de Fase 2: `BuildSystem`.
- **Naturaleza:** infraestructura nueva de pruebas de procesos CMake; no modifica producto, scripts canónicos,
  suite histórica, bibliotecas externas ni cobertura instrumental.
- **Comportamiento preservado:** los 76 archivos inventariados, sus variables, targets, errores, side effects,
  instalación y adaptadores históricos permanecen intactos.
- **Revisión conocida:** revisión documentada por Fase 1; no aplica un rango correctivo de commits.
- **Rango:** `T12B-00001..T12B-00456`; contratos `C12B-BLD-001..C12B-BLD-076`.
- **Archivos autorizados:** probes y catálogos nuevos bajo `tests/compiler-suite/buildsystem`, gate aislado,
  las 36 filas nuevas, integración CMake mínima, tracking, cursor y este Markdown.
- **Archivos protegidos:** `CMakeLists.txt` raíz, `cmake/*.cmake`, fuentes del producto, las 684 pruebas y gates
  históricos, bibliotecas, cobertura, `C:`, remotos y cambios concurrentes.
- **Pruebas inmutables:** todas las existentes. Los 420 casos `legacy_historical_suite` conservan su dueño y
  ejecución semanal; esta tanda sólo registra la delegación y no copia sus algoritmos.
- **Acciones prohibidas:** build, configure, pruebas, gates, validación, instalación, despliegue, ejecución
  histórica, recursión nueva, fallback, artefactos previos, `SKIP`, `XFAIL` y oráculos vacíos.
- **Criterio de terminación:** 456/456 casos materializados entre flujo nuevo y dueño histórico, 36 casos del
  flujo nuevo con oráculos ejecutables, KPD preservados como pruebas rojas y cero caso omitido o duplicado.

## Selección y denominador canónicos

La Ola 8 contiene sólo `SelfHost.Bootstrap.Infrastructure` y `SelfHost.Contracts`, ambos excluidos por orden
explícita. La siguiente ola es la 9 y su primer componente es `BuildSystem`; `Packaging` es el segundo y no se
incluye en esta tanda.

| Dimensión | Conteo |
|---|---:|
| Archivos | 76 |
| Contratos | 76 |
| Casos canónicos | 456 |
| `build-contract` / `historical-weekly` | 36 / 420 |
| Perfil `NIGHTLY` / `WEEKLY` | 36 / 420 |
| Ejecutables nuevos `Required` | 34 |
| Ejecutables nuevos `KnownProductDefect` | 2 |
| `HistoricalDelegated` al dueño congelado | 420 |
| Omitidos / desconocidos | 0 / 0 |

La partición es:

```text
456 = 34 Required + 2 KnownProductDefect + 420 HistoricalDelegated
36 ejecutables del controller nuevo = contratos 001..005 y 076, seis casos por contrato
420 ejecutables históricos delegados = contratos 006..075, seis casos por contrato
```

`HistoricalDelegated` no significa aprobado, omitido ni sustituido. Indica que la propia matriz de Fase 1 asigna
esos IDs a `legacy_historical_suite`, perfil `WEEKLY` y aislamiento `LEGACY_OWNED`; duplicarlos dentro del controller
nuevo violaría la inmutabilidad de la suite histórica.

## Contratos del flujo nuevo

| Contrato | Casos | Probe real | Oracle |
|---|---:|---|---|
| `C12B-BLD-001` | `T12B-00001..00006` | `GenerateBuildIdentity.cmake` con template aislado | campos, bytes, errores y determinismo exactos |
| `C12B-BLD-002` | `T12B-00007..00012` | proyecto CMake con árboles zlib/zstd sintéticos | targets, aliases, propiedades y fallos de configuración |
| `C12B-BLD-003` | `T12B-00013..00018` | invocación de `nsc_select_selfhost_profiles` | lista ordenada o diagnóstico propietario |
| `C12B-BLD-004` | `T12B-00019..00024` | invocación de `nsc_resolve_selfhost_runtime` | rutas library/ABI o rechazo antes de publicación |
| `C12B-BLD-005` | `T12B-00025..00030` | copia sandbox de una instalación explícita | manifiesto, update plan, limpieza y verificación |
| `C12B-BLD-076` | `T12B-00451..00456` | configure del grafo raíz en build dirs frescos | targets, configuración inválida y determinismo del grafo |

Los probes incluyen los helpers reales desde el repositorio. Los árboles zlib/zstd sólo satisfacen la frontera
de archivos de CMake con unidades C mínimas; no reimplementan las bibliotecas ni se publican. La instalación se
copia desde una raíz explícita a cada sandbox antes de cualquier operación destructiva del verificador. El grafo
raíz exige `LLVM_DIR` y demás argumentos explícitos; no consulta instalaciones implícitas.

## Delegación histórica exacta

`BuildSystemContractCatalog.jsonl` registra los 76 contratos y su archivo canónico. Los contratos
`C12B-BLD-006..075`, casos `T12B-00031..00450`, permanecen asignados uno a uno a sus verificadores
`cmake/VerifyNativeSharp*.cmake`. `BuildSystemCaseClassifications.jsonl` enumera los 420 IDs individualmente con
disposición `HistoricalDelegated` y resolución hacia el archivo dueño.

La delegación conserva el denominador y permite comprobar mecánicamente que:

- cada contrato histórico tiene exactamente seis IDs;
- ningún ID delegado aparece en el fragmento nuevo;
- ningún adaptador histórico fue editado, copiado o ejecutado;
- los resultados históricos futuros no pueden presentarse como aprobación del controller nuevo.

## Defectos de producto conservados como pruebas rojas

### `T12B-00021` — productor NSLink ausente

El contrato exige que una dependencia ausente falle en la frontera propietaria sin publicar downstream.
`nsc_resolve_selfhost_runtime` acepta actualmente un path NSLink vacío y deriva rutas desde una raíz accidental.
El oracle exige fallo y ausencia del archivo de salida.

### `T12B-00022` — productor NSLink malformado

El helper tampoco valida estructuralmente un valor como `not-a-path`; calcula y publica rutas runtime derivadas.
El oracle conserva el rechazo contractual. Ninguno de los dos casos usa `XFAIL`, invierte el resultado ni convierte
la conducta vigente en éxito esperado.

El gate informa `KNOWN_PRODUCT_DEFECT` cuando el contrato sigue incumplido y `UNEXPECTED_PASS` cuando ya se cumple;
ambos estados exigen acción y mantienen el gate rojo hasta actualizar la clasificación.

## Artefactos exclusivos

- `buildsystem/fixtures/BuildIdentity.in.h`: template observable mínimo.
- `BuildSystemProfilesProbe.cmake`: frontera pública de selección de perfiles.
- `BuildSystemRuntimeProbe.cmake`: frontera pública de resolución runtime.
- `LlvmStaticDependencies/CMakeLists.txt`: probe de targets y propiedades zlib/zstd.
- `BuildSystemContractCatalog.jsonl`: 76 contratos, rangos, fuentes y dueño de ejecución.
- `BuildSystemCaseClassifications.jsonl`: 420 delegaciones históricas y 2 KPD.
- `manifest/cases.jsonl`: sólo los 36 ejecutables del controller nuevo; el fragmento fue retirado.
- `run_buildsystem_gate.py`: linaje, partición, sandboxes, procesos CMake, oráculos y evidencia por caso.

Las 36 filas nuevas y el controlador quedaron integrados en CMake. La raíz de instalación se deriva del NSC
seleccionado y CMake, LLVM, NSLink y compiladores se propagan desde la configuración vigente; no se fijan rutas.
No se modificaron producto, scripts CMake canónicos ni historia.

## Algoritmos, aislamiento y evidencia

- La validación de linaje es `O(456)` tiempo y memoria acotada por el inventario.
- La generación de árboles fixture es iterativa y lineal en los archivos declarados; no agrega recursión.
- Cada caso usa un directorio fresco. Las configuraciones deterministas usan dos build dirs independientes.
- Los procesos registran command context, stdout, stderr, resultado y duración; un productor fallido no autoriza
  interpretar consumidores posteriores.
- No existe fallback a `C:`, PATH implícito, caché, instalación anterior o salida de otra corrida.
- El gate requiere CMake, raíz de instalación y argumentos del grafo raíz explícitos.

## Estado exacto

No se ejecutaron builds, configure, pruebas, gates, verificadores históricos ni validaciones. No se afirma que los
probes compilen o aprueben. El estado es:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 456/456 TRAZADOS; 36 EJECUTABLES NUEVOS
(34 REQUIRED + 2 KPD ROJOS) Y 420 HISTORICALDELEGATED AL DUEÑO CONGELADO; VALIDACIÓN PENDIENTE`.
