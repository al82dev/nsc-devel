# Tanda 11 — Ownership y flujo

## Propósito y alcance

La tanda migra los contratos de CFG, dominancia, SSA, asignación definida y ownership de ejecución al único
ejecutable C++ consolidado y al catálogo común de fixtures NativeSharp. No modifica el compilador ni crea
semántica nueva.

El inventario cerrado contiene 32 identificadores:

| Representación nueva | Cantidad |
|---|---:|
| Casos C++ en `FlowLegacyEntries.def` | 20 |
| Fixtures externos bajo `tests/cases/flow` | 12 |
| Total | 32 |

Los 30 casos clasificados originalmente como CFG, dominancia, ownership o SSA se complementan con
`nsc.stage50-11.ownership-execution` y `nsc.stage56.c7-definite-assignment`, enrutados explícitamente desde la
Tanda 10. Ninguna fila queda sin destino.

## SCOPE LOCK aplicado

- Repositorio: `A:\nsc-devel`.
- Cambio autorizado: infraestructura, refactor de pruebas y migración con paridad.
- Código de producción, `nscbuilder.py`, `_nscbuilder` y C: permanecen protegidos.
- Configuración funcional: Windows x64, O0 y NSLink explícito de A:.
- Prohibido: baseline, promoción, certificación, despliegue, instalación, O1, O2, O3 y remotos Git.
- Se preservan identificadores, fuentes, proyectos, expectativas y aserciones históricas.
- Los adaptadores y ejecutables anteriores permanecen hasta el corte de la Tanda 18.

## Casos C++ consolidados

`FlowUnitTests.cpp` registra las entradas históricas sin crear procesos adicionales. El registro determinista
`FlowLegacyEntries.def` contiene:

- 12 contratos CFG: seis selectores CC1, CFG interno, verificador CFG, dos rutas excepcionales, control de
  ownership y asignación definida;
- 2 contratos de dominancia;
- 3 contratos de ownership y lifetime;
- 3 contratos SSA.

Los catorce dueños independientes usan adaptadores bajo `tests/unit/flow_adapters`. Los seis selectores CC1
reutilizan el único adaptador CC1 ya enlazado y llaman sólo a su fixture.

## Perfil O0

Los targets históricos de Stage 50.11 y Stage 56 conservan O0/O2 por defecto. Bajo `NSC_TEST_O0_ONLY`, definido
exclusivamente para el ejecutable consolidado:

- Stage 50.11 ejecuta sólo sus rutas O0;
- Stage 56 ejecuta sólo O0 y omite sus inspecciones exclusivas de código O2;
- el ejemplo final de Stage 56 se compila con O0;
- no se ejecuta ninguna ruta NativeSharp O1, O2 u O3.

El código C++ host se construye con la configuración Release existente; esa optimización de la herramienta no
es una variante funcional de los programas NativeSharp probados.

## Fixtures externos

Se copiaron textualmente, salvo finales de línea, cuatro fuentes históricas:

- `control_flow.ns`;
- `cfg_features.ns`;
- `dominance_features.ns`;
- `ssa_features.ns`.

La comparación normalizada confirmó equivalencia 4/4. Esas cuatro entradas alimentan doce manifiestos:

| Componente | Integración | Dump | Verificación | Total |
|---|---:|---:|---:|---:|
| CFG | 2 | 2 | 2 | 6 |
| Dominancia | 1 | 1 | 1 | 3 |
| SSA | 1 | 1 | 1 | 3 |

Las expectativas se trasladaron desde CTest sin reinterpretación:

- `cfg.module`;
- `[unreachable]`;
- `CFG verification: ok`;
- `dominance.module`;
- `Dominance verification: ok`;
- `phi @value`;
- `SSA verification: ok`;
- código de salida cero para los cuatro checks de integración.

## Aislamiento y shell

La primera invocación dirigida no creó el directorio indicado por `TEMP` y `TMP`; los casos que usan
`temp_directory_path` fallaron antes de ejecutar su contrato. Se creó el sandbox autorizado bajo
`A:\nsc-devel\_test-harness\work` y se repitió el mismo conjunto.

Stage 56 intentaba buscar Wine mediante `command -v` incluso en Windows. Para el adaptador consolidado de
Windows, `wineCommand` devuelve directamente que Wine no está disponible, que es el mismo estado lógico que
producía la búsqueda fallida, pero sin emitir errores de un shell incompatible. El target histórico conserva
su implementación. La reproducción aislada de Stage 56 pasó 55/55 comprobaciones sin ese ruido.

## Archivos creados

- `tests/unit/modules/FlowUnitTests.cpp`.
- `tests/unit/include/nsc_test/FlowLegacyEntries.def`.
- catorce adaptadores bajo `tests/unit/flow_adapters`.
- cuatro fuentes y doce manifiestos bajo `tests/cases/flow`.
- este documento editorial.

## Archivos modificados

- `tests/unit/CMakeLists.txt`: incorpora módulo y adaptadores.
- `tests/unit/consolidated/main.cpp`: registra flujo.
- `tests/unit/include/nsc_test/Modules.h`: declara el registro.
- `Stage50_11OwnershipExecutionTests.cpp`: frontera O0 del consolidado.
- `Stage56C7DefiniteAssignmentTests.cpp`: frontera O0 y shell consolidado Windows.
- `test-migration-state.json`: persistencia de los 32 casos.
- `TRACKING.md`: avance 11/20.

## Complejidad y ausencia de recursión

El registro recorre linealmente 20 descriptores O(n). Cada selector CC1 ejecuta exactamente un fixture. Los
fixtures externos se descubren mediante el catálogo iterativo existente. No se añadió recursión, estado global
nuevo ni una ruta de ejecución paralela.

## Validación dirigida

Rutas efectivas:

- compilador: `A:\nsc-devel\build-clang\bin\nsc.exe`;
- NSLink: `A:\nslink-devel\bin\release\nslink.exe`;
- build del consolidado: `A:\nsc-devel\build-test-infrastructure`;
- optimización NativeSharp: O0.

Resultados C++:

| Componente | Seleccionadas | Aprobadas | Fallidas |
|---|---:|---:|---:|
| CFG | 12 | 12 | 0 |
| Dominancia | 2 | 2 | 0 |
| Ownership | 3 | 3 | 0 |
| SSA | 3 | 3 | 0 |

Resultados externos:

| Componente | Seleccionadas | Aprobadas | Fallidas |
|---|---:|---:|---:|
| CFG | 6 | 6 | 0 |
| Dominancia | 3 | 3 | 0 |
| SSA | 3 | 3 | 0 |

La construcción dirigida de `nsc_unit_tests.exe` terminó con código cero. El gate final valida JSON, registro,
líneas añadidas, diferencial y residuos antes del commit.

## Comportamiento preservado

- Código de producción del compilador: sin cambios.
- Semántica, diagnósticos, API y ABI: sin cambios.
- Entradas y expectativas históricas: equivalentes y todavía disponibles.
- `nscbuilder.py` y `_nscbuilder`: sin cambios.
- C: no se consultó como fuente de NSC o NSLink y no recibió escrituras.
- Baseline, promoción, certificación, instalación y remotos: no ejecutados.
- O1/O2/O3 NativeSharp: no ejecutados.

## Estado

Tanda 11 `TERMINADA` con paridad dirigida. La eliminación de adaptadores, ejecutables antiguos y duplicados de
fuentes permanece reservada a la Tanda 18.
