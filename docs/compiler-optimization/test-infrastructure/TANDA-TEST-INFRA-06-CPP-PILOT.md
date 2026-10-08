# Tanda 6 — Piloto de unidad C++ consolidada

## Objetivo

Migrar una unidad interna pequeña al registro consolidado y demostrar paridad sin modificar ni retirar su ejecutable legado.

## Caso seleccionado

- ID: `nsc.version.identity`.
- Owner legado: `tests/unit/Cc1BuildIdentityTests.cpp`.
- Dependencia: `nsc_basic` y el header generado `nativesharp/Basic/Version.h`.
- Entrada NativeSharp: ninguna.
- Razón: verifica directamente constantes internas de identidad y pertenece legítimamente a la suite C++.

## Archivos creados

- `tests/unit/include/nsc_test/Modules.h`
- `tests/unit/modules/BasicUnitTests.cpp`
- `docs/compiler-optimization/test-infrastructure/test-migration-state.json`

## Archivos modificados

- `tests/unit/CMakeLists.txt`
- `tests/unit/consolidated/main.cpp`

## Paridad preservada

El módulo nuevo conserva las nueve expectativas del owner legado:

1. build ID no vacío;
2. placeholder sustituido;
3. longitud de 16 caracteres;
4. hexadecimal minúsculo;
5. release `3.0.0`;
6. versión visible con metadata de build;
7. build ID exacto dentro de la versión;
8. versión de lenguaje `3.0.0`;
9. stage `70`.

El archivo legado y su target permanecen intactos.

## Validación ejecutada

- Ejecutar el binario legado ya construido.
- Reconstruir únicamente `nsc_unit_tests` y su dependencia mínima.
- Ejecutar `nsc_unit_tests --case nsc.version.identity`.
- Ejecutar `nsc_unit_tests --component basic`.
- Comparar código de salida y resultado agregado.

```text
Legado:
PASS: 9 checks
exit=0

Consolidado por ID:
PASS nsc.version.identity
selected=1 passed=1 failed=0
exit=0

Consolidado por componente:
PASS nsc.version.identity
selected=1 passed=1 failed=0
exit=0
```

`--list` enumeró exactamente `nsc.version.identity` dentro de `basic`. La matriz inicial permanece como snapshot; `test-migration-state.json` registra el estado vivo `DUAL_PARITY`.

## Pendiente deliberado

El ejecutable legado sólo podrá retirarse en la tanda de corte después de conservar la evidencia de paridad.
