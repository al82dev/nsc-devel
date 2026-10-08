# Tanda 12 — HIR, MIR y pases

## Propósito y alcance

La tanda integra los contratos HIR, MIR y de pases en el ejecutable C++ consolidado y externaliza las entradas
que históricamente se invocaban directamente mediante CTest. No modifica código de producción del compilador,
semántica, diagnósticos, API, ABI ni comandos públicos.

El inventario cerrado contiene 49 identificadores:

| Representación nueva | Cantidad |
|---|---:|
| Casos C++ en `IrLegacyEntries.def` | 24 |
| Fixtures externos bajo `tests/cases/ir` | 25 |
| Total | 49 |

Los 46 casos clasificados originalmente como HIR, MIR o pases se complementan con tres casos enrutados desde la
Tanda 10: Stage 44 monomorfización y los dos cierres de colecciones genéricas de Stage 52.

## SCOPE LOCK aplicado

- Repositorio: `A:\nsc-devel`.
- Cambio autorizado: infraestructura, refactor de pruebas y migración con paridad.
- Código de producción, `nscbuilder.py`, `_nscbuilder` y C: permanecen protegidos.
- Configuración funcional: Windows x64, O0 y NSLink explícito de A:.
- Prohibido: baseline, promoción, certificación, despliegue, instalación, O1, O2, O3 y remotos Git.
- Se preservan identificadores, fuentes, proyectos, argumentos, expectativas y aserciones históricas.
- Los ejecutables históricos permanecen disponibles hasta el corte de la Tanda 18.

## Casos C++ consolidados

`IrUnitTests.cpp` registra 24 owners mediante el inventario determinista `IrLegacyEntries.def`:

| Componente | Seleccionados |
|---|---:|
| HIR | 11 |
| MIR | 8 |
| Pases | 5 |

Cada adapter renombra únicamente el `main` histórico e incluye el mismo owner. Las aserciones, fixtures
internos y agregación de errores siguen siendo las del ejecutable anterior. El registro añade un descriptor y
una llamada directa; no crea un proceso por caso ni duplica el cuerpo de la prueba.

## Perfil O0

Los owners históricos de Stage 44, Stage 52.1 y Stage 52 contienen una ruta LLVM O2. El target histórico
conserva O2 cuando se compila por separado. Bajo `NSC_TEST_O0_ONLY`, definido exclusivamente para
`nsc_unit_tests.exe`, esos tres owners seleccionan `LlvmOptimizationLevel::O0`. No se ejecutó ninguna ruta
NativeSharp O1, O2 u O3.

La configuración Release del host C++ no cambia el nivel funcional de los programas NativeSharp examinados.

## Fixtures externos

Se copiaron byte por byte 22 archivos históricos:

- cuatro fuentes de integración: `full_frontend.ns`, `hir_features.ns`, `mir_features.ns` y
  `pass_features.ns`;
- nueve proyectos `.nsproj` de Stage 35, 36, 37, 38, 39, 40, 42, 43 y 44;
- las nueve fuentes NativeSharp correspondientes a esos proyectos.

Los hashes SHA-256 de las 22 copias coincidieron con sus fuentes. Los 25 manifests conservan los comandos y
expectativas CTest:

| Componente | Directos | Proyectos | Total |
|---|---:|---:|---:|
| HIR | 4 | 5 | 9 |
| MIR | 5 | 9 | 14 |
| Pases | 2 | 0 | 2 |

El caso negativo `nsc.invalid.optimization-level` conserva su entrada `-O4`. No ejecuta optimización: comprueba
el rechazo del nivel inexistente, código 2 y diagnóstico contractual.

## Expresiones regulares

CTest conservaba varias expectativas ordenadas mediante `PASS_REGULAR_EXPRESSION`. El harness sólo admitía
igualdad y búsqueda literal, por lo que trasladarlas como `contains` habría debilitado o cambiado el contrato.
Se añadió el modo `regex` a `TextExpectation` y `ResultVerifier`:

- usa `re.search` con `DOTALL` para conservar coincidencias entre líneas;
- no transforma la expresión;
- reporta el mismo tipo de fallo de stream que los demás modos;
- tiene una prueba unitaria multilinea.

La primera ejecución reveló que Stage 37 requería precisamente el regex histórico
`abi=.*static,p0:builtin:int`; su manifest se corrigió sin modificar el proyecto ni el compilador.

## Archivos creados

- `tests/unit/modules/IrUnitTests.cpp`.
- `tests/unit/include/nsc_test/IrLegacyEntries.def`.
- 24 adapters bajo `tests/unit/hir_mir_pass_adapters`.
- 22 archivos de entrada y 25 manifests bajo `tests/cases/ir`.
- este documento editorial.

## Archivos modificados

- `tests/unit/CMakeLists.txt`: incorpora el módulo y los adapters.
- `tests/unit/consolidated/main.cpp`: registra el módulo IR.
- `tests/unit/include/nsc_test/Modules.h`: declara el registro.
- tres owners Stage 44/52: selección O0 exclusiva del consolidado.
- `tests/harness/nsc_test/model.py`: declara el modo de texto `regex`.
- `tests/harness/nsc_test/verify.py`: verifica regex multilinea.
- `tests/harness/tests/test_verification.py`: prueba dirigida del nuevo modo.
- `test-migration-state.json`: persistencia de los 49 casos.
- `TRACKING.md`: avance 12/20.

## Complejidad y ausencia de recursión

El registro recorre linealmente 24 descriptores, O(n) tiempo y O(1) espacio adicional por alta. La verificación
regex delega en el motor estándar de Python sobre un stream ya capturado. El catálogo y el copiado de workspace
siguen usando worklists iterativas. No se añadió recursión, estado global mutable ni concurrencia.

## Validación dirigida

Rutas efectivas:

- compilador: `A:\nsc-devel\build-clang\bin\nsc.exe`;
- NSLink: `A:\nslink-devel\bin\release\nslink.exe`;
- build del consolidado: `A:\nsc-devel\build-test-infrastructure`;
- optimización NativeSharp: O0, salvo el rechazo intencional de `-O4`;
- runtime de prueba: `A:\nsc-devel\build-clang\generated\NativeSharp.Runtime.Win32.lib`.

Resultados:

| Conjunto | Seleccionadas | Aprobadas | Fallidas |
|---|---:|---:|---:|
| Harness Python | 16 | 16 | 0 |
| HIR C++ | 11 | 11 | 0 |
| MIR C++ | 8 | 8 | 0 |
| Pases C++ | 5 | 5 | 0 |
| HIR externo | 9 | 9 | 0 |
| MIR externo | 14 | 14 | 0 |
| Pases externo | 2 | 2 | 0 |

La construcción dirigida de `nsc_unit_tests.exe` terminó con código cero y warnings tratados como errores. La
comparación determinista entre matriz y catálogo obtuvo 25 esperados y 25 presentes, sin diferencias.

## Análisis estático y austeridad

- `git diff --check` no informó errores de whitespace.
- El build estricto no produjo warnings.
- Los adapters contienen únicamente el puente de entrada indispensable.
- No se añadieron wrappers de proceso, runners por componente, excepciones ni fallbacks.
- Los proyectos siguen siendo fuentes visibles; cambiar un `.ns` no obliga a recompilar C++.

No se ejecutó una herramienta de análisis estático C++ completa en esta tanda; la verificación disponible fue
el compilador con warnings-as-errors, la auditoría de banderas y las pruebas dirigidas.

## Comportamiento preservado

- Código de producción del compilador: sin cambios.
- Semántica, diagnósticos, API y ABI: sin cambios.
- Entradas, proyectos y expectativas históricas: preservados.
- `nscbuilder.py` y `_nscbuilder`: sin cambios.
- C: no se utilizó como fuente de NSC o NSLink y no recibió escrituras.
- Baseline, promoción, certificación, instalación y remotos: no ejecutados.
- O1/O2/O3 NativeSharp: no ejecutados.

## Estado

Tanda 12 `TERMINADA` con paridad dirigida 49/49. La eliminación de adapters, ejecutables históricos y fuentes
duplicadas permanece reservada a la Tanda 18.
