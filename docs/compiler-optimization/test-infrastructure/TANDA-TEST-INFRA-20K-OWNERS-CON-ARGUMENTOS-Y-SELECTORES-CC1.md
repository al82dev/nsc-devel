# Tanda 20K — Owners con argumentos y selectores CC1

## Objetivo

Restaurar las 22 rutas históricas que no podían incorporarse mediante el
generador de owners `main()` sin argumentos:

- 18 identificadores pertenecientes al selector multipropósito de CC1;
- cuatro owners que reciben `argc/argv` o `wmain`.

## Implementación

### CC1

`SemanticLegacyEntries.def` registra los 18 IDs que faltaban. Cada registro
invoca `Cc1CorrectnessTests.cpp` con el selector histórico exacto; `--rest`
conserva la entrada agregada `nsc.cc1.correctness`. El adaptador reinicia el
estado global antes de cada llamada, por lo que una selección no contamina a la
siguiente.

El target consolidado define `NSC_TEST_O0_ONLY`. Por ello incluso las fixtures
cuyo nombre histórico menciona O2 se ejecutaron exclusivamente con `-O0` en
esta validación; no se suavizó ninguna aserción.

### Owners con argumentos

`ArgumentLegacyUnitTests.cpp` registra cuatro rutas separadas:

| ID | Componente | Particularidad preservada |
|---|---|---|
| `nsc.llvm-optimization` | llvm | protocolo hijo `--nsc-test-llvm-trace-child` |
| `nsc.ns3.e046.smart-pointer-contract` | frontend | `argv[0]` real para el contrato junto al ejecutable |
| `nsc.ns3.t10.coroutines` | frontend | `argv[0]` real para el contrato junto al ejecutable |
| `nsc.stage46.native-libraries` | interop | entrada Windows `wmain` y ruta del NSC del mismo árbol |

Cada owner permanece en una unidad de traducción separada. El host consolidado
intercepta únicamente el argumento privado del hijo LLVM y lo reenvía al owner
original. Stage 46 recibe `NSC_TEST_NSC_EXECUTABLE` como definición exclusiva
de su source adapter; no se propaga a otros casos.

## Validación dirigida

- Construcción de `nsc_unit_tests`: correcta.
- Catálogo unitario: 498 IDs enumerados.
- Auditoría nominal: 599/684 con ruta ejecutable.
- Veinte rutas compatibles con O0 ejecutadas individualmente: 20 aprobadas,
  cero fallos.
- `nsc.llvm-optimization` y `nsc.stage46.native-libraries`: ruta construida y
  seleccionable, ejecución omitida porque sus cuerpos históricos contienen
  trabajo O1/O2 prohibido durante esta migración.

## Estado global después de la tanda

| Estado | Cantidad |
|---|---:|
| Con ruta ejecutable | 599 |
| En destino final correcto | 161 |
| Transitorio, capa final pendiente | 438 |
| Bloqueado | 41 |
| Sin mapear | 43 |
| Ambiguo | 1 |

## Archivos

- `tests/unit/include/nsc_test/SemanticLegacyEntries.def`
- `tests/unit/modules/ArgumentLegacyUnitTests.cpp`
- `tests/unit/argument_legacy_adapters/LlvmOptimizationAdapter.cpp`
- `tests/unit/argument_legacy_adapters/NativeSharp3E046SmartPointerAdapter.cpp`
- `tests/unit/argument_legacy_adapters/NativeSharp3T10CoroutineAdapter.cpp`
- `tests/unit/argument_legacy_adapters/Stage46NativeLibrariesAdapter.cpp`
- `tests/unit/consolidated/main.cpp`
- `tests/unit/include/nsc_test/Modules.h`
- `tests/unit/CMakeLists.txt`
- `docs/compiler-optimization/test-infrastructure/test-execution-parity.*`

## Límite de la afirmación

Las 22 rutas siguen siendo transitorias porque la matriz las destina a fixtures
NativeSharp. Esta tanda no declara migración final ni ejecución de perfiles
prohibidos. No se modificó el compilador, el builder anterior, sus datos, C: ni
ningún repositorio de herramientas.
