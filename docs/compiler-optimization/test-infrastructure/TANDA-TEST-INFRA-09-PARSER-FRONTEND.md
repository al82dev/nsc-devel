# Tanda 9 — Parser y frontend sintáctico

## Propósito

Consolidar en un solo ejecutable las unidades C++ que ejercitan el parser y externalizar las entradas NativeSharp necesarias para probar el frontend sintáctico con el `nsc.exe` construido, sin modificar el compilador ni retirar todavía la infraestructura histórica.

## Cambios realizados

- `nsc_unit_tests.exe` registra once identificadores históricos del parser bajo el componente `parser`.
- Cada identificador invoca directamente la función de entrada de su unidad histórica mediante un adaptador de transición. No se crea un proceso por prueba y se conservan sin reinterpretación las aserciones existentes.
- Se añadió el caso externo histórico `nsc.parser` con su fuente `full-grammar.ns` y una ejecución `--dump-ast` en O0.
- Se añadió a `nsc.dump-tokens` un manifiesto auxiliar para el identificador histórico `nsc.dump-ast`, reutilizando `full_frontend.ns`.
- El registro consolidado enlaza `nsc_driver` porque los casos sintácticos de bitfields ejercitan la frontera del driver dentro del proceso.
- `NSC_BINARY_DIR` se inyecta al ejecutable consolidado para conservar el contrato utilizado por las unidades históricas.

## Adaptadores de transición

Los once archivos bajo `tests/unit/adapters` renombran la función `main` de cada unidad histórica y la incorporan en `nsc_unit_tests.exe`. Esta decisión es deliberadamente transitoria:

- conserva todas las comprobaciones originales durante la fase de paridad;
- evita duplicar manualmente cientos de aserciones;
- mantiene los ejecutables históricos disponibles;
- será retirada en la Tanda 18, después del gate de paridad y del traslado definitivo de cada cuerpo al registro común.

Los adaptadores no son runners separados: todos forman parte del único ejecutable consolidado.

## Archivos principales

- `tests/unit/modules/ParserUnitTests.cpp`
- `tests/unit/adapters/*.cpp`
- `tests/unit/CMakeLists.txt`
- `tests/unit/consolidated/main.cpp`
- `tests/unit/include/nsc_test/Modules.h`
- `tests/cases/parser/nsc.parser/*`
- `tests/cases/lexer/nsc.dump-tokens/dump-ast.case.json`
- `docs/compiler-optimization/test-infrastructure/test-migration-state.json`

## Cobertura y enrutamiento

- Consolidado C++: 11 identificadores históricos.
- Fixtures externos: 2 identificadores históricos.
- `nsc.stage33.constructor-chaining` permanece asignado a la Tanda 14 porque su contrato principal es un proyecto `.nsproj`, no una unidad sintáctica aislada.
- No se añadieron características ni identificadores nuevos de lenguaje.

## Validación dirigida

- Construcción: `nsc_unit_tests.exe` enlazó correctamente.
- Parser consolidado: 11 seleccionadas, 11 aprobadas, 0 fallidas.
- Fixtures parser con `A:\nsc-devel\build-clang\bin\nsc.exe`, `A:\nslink-devel\bin\release\nslink.exe` y O0: 2 seleccionadas, 2 aprobadas, 0 fallidas.
- Infraestructura Python: 15 pruebas aprobadas, 0 fallidas.
- La primera ejecución del fixture `nsc.parser` detectó una expectativa nueva mal escrita: el AST emite `FunctionDeclaration \"Main\"`. Sólo se corrigió ese manifiesto nuevo; no se cambió el compilador ni una expectativa histórica.

## Comportamiento preservado

- Código de producción del compilador: sin cambios.
- `nscbuilder.py` y `_nscbuilder`: sin cambios.
- Instalaciones en `C:`: no consultadas ni modificadas.
- Ejecutables y fuentes históricos: conservados.
- Baseline, promoción, certificación y perfiles O1/O2/O3: no ejecutados.

## Estado

`TERMINADA` con paridad dirigida. El corte de adaptadores y ejecutables históricos sigue reservado a la Tanda 18.
