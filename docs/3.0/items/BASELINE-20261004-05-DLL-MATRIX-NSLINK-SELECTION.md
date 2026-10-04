# Baseline 2026-10-04: selección de NSLink en DLL-P1

## Problema

La matriz DLL-P1 sólo consultaba `NSC_P1_NSLINK` y el alias histórico `NSLINK_EXE`; si ninguno
estaba definido caía en `A:/nslink-devel/build-debug/bin/nslink.exe`. NSCBuilder publica el linker
seleccionado mediante `NSC_LINKER`, por lo que la prueba ignoraba deliberadamente el artefacto de
la corrida y buscaba una ruta obsoleta.

## Cambio

La resolución queda, en orden:

1. `NSC_P1_NSLINK`, override exclusivo de la matriz.
2. `NSC_LINKER`, selección común proporcionada por NSCBuilder.
3. `NSLINK_EXE`, compatibilidad con invocaciones anteriores.
4. `A:/nslink-devel/bin/release/nslink.exe`, ruta productiva predeterminada.

## Archivos

- `tests/unit/Stage46_1DllCrossLinkerMatrixTests.cpp`
- `docs/3.0/items/BASELINE-20261004-05-DLL-MATRIX-NSLINK-SELECTION.md`

## Mejora

DLL-P1 prueba exactamente el NSLink seleccionado para la corrida, incluyendo el ejecutable staged
del baseline, y deja de depender de un directorio histórico de compilación Debug.

## Validación dirigida

- Target y prueba exacta:
  `nsc_stage46_1_dll_cross_linker_matrix_tests` /
  `nsc.stage46-1.dll-cross-linker-matrix`.
- No se ejecutó baseline ni CTest general.
