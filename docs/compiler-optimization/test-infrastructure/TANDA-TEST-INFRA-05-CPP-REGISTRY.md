# Tanda 5 — Registro C++ consolidado

## Objetivo

Crear un único ejecutable unitario optativo, todavía sin pruebas migradas, que permita enumerar, filtrar, ejecutar y reportar módulos C++ internos.

## Archivos creados

- `tests/unit/CMakeLists.txt`
- `tests/unit/include/nsc_test/TestCase.h`
- `tests/unit/include/nsc_test/TestContext.h`
- `tests/unit/include/nsc_test/TestRegistry.h`
- `tests/unit/include/nsc_test/TestResult.h`
- `tests/unit/include/nsc_test/Assertions.h`
- `tests/unit/consolidated/TestRegistry.cpp`
- `tests/unit/consolidated/main.cpp`

## Integración CMake

`NSC_ENABLE_CONSOLIDATED_TESTS` tiene valor predeterminado `OFF`. Cuando está apagado no crea target ni entrada CTest. Cuando está encendido crea únicamente `nsc_unit_tests` y `nsc.unit.consolidated`.

## Diseño

- Un solo `main()`.
- Registro explícito; no hay registros globales por efectos de inicialización estática.
- IDs duplicados se rechazan al registrar.
- Selección `--list`, `--all`, `--case` y `--component`.
- Reporte JSON opcional.
- Excepciones no capturadas dentro de un caso se convierten en fallos del caso.
- No se usa recursión.

## Validación ejecutada

- Configurar un build dedicado de infraestructura en `A:`.
- Construir únicamente `nsc_unit_tests`.
- Ejecutar `--list` y `--all` con catálogo vacío.
- Solicitar un ID inexistente y comprobar código 2.

```text
cmake -S A:\nsc-devel -B A:\nsc-devel\build-test-infrastructure ...
cmake --build A:\nsc-devel\build-test-infrastructure --target nsc_unit_tests --parallel 12
```

La configuración y construcción terminaron con código cero. Sólo se compilaron `TestRegistry.cpp` y `main.cpp`, y se enlazó `tests\nsc_unit_tests.exe`.

```text
nsc_unit_tests.exe --list
cases=0

nsc_unit_tests.exe --all
selected=0 passed=0 failed=0

nsc_unit_tests.exe --case missing.case
unknown test case: missing.case
exit=2
```

El reporte JSON temporal se leyó y eliminó después de la validación.

## Comportamiento preservado

La opción está apagada por defecto. Ningún target de prueba actual se modifica o sustituye.

## Pendiente deliberado

La primera unidad real se incorpora en la Tanda 6 y conserva temporalmente su ejecutable anterior.
