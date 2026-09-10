# 1.0.1 — Matriz cruzada de DLLs y linkers / Cross-linker DLL matrix

## Resultado / Outcome

**ES.** DLL-P1 certifica el producto cartesiano de productor y consumidor con
lld-link y NSLink, combinando de forma independiente O0 y O2. Cada uno de los 16
casos produce DLL/import library/NSMETA, verifica conjuntamente los artefactos,
enlaza un consumidor source-free, comprueba su importación PE y ejecuta 42.

**EN.** DLL-P1 certifies the full Cartesian product of lld-link and NSLink
producers and consumers with independent O0 and O2 settings. Each of the 16 cases
builds a DLL/import-library/NSMETA triplet, jointly verifies the artifacts, links
a source-free consumer, checks its PE import, and executes to 42.

## Archivos / Files

- `tests/unit/Stage46_1DllCrossLinkerMatrixTests.cpp`: fixture autocontenido,
  selección acotada del linker y matriz 2×2×2×2.
- `CMakeLists.txt`: registra `nsc.stage46-1.dll-cross-linker-matrix`.

- `tests/unit/Stage46_1DllCrossLinkerMatrixTests.cpp`: self-contained fixture,
  scoped linker selection, and 2×2×2×2 matrix.
- `CMakeLists.txt`: registers `nsc.stage46-1.dll-cross-linker-matrix`.

## Decisiones / Decisions

- `NSC_LLD_LINK` se restaura después de cada fase; productor y consumidor no
  comparten selección accidental.
- `NSC_P1_LLD_LINK` y `NSC_P1_NSLINK` permiten toolchains alternativos; los
  defaults de Windows son `C:\clang-llvm\bin\lld-link.exe` y
  `C:\native-sharp\bin\nslink.exe`.
- El gate exige ambos linkers: no convierte una ausencia en falso verde.
- Los artefactos de prueba viven bajo `NSC_TEST_EVIDENCE_DIR` y se borran en
  verde.

- `NSC_LLD_LINK` is restored after every phase, so producer and consumer cannot
  accidentally share a selection.
- `NSC_P1_LLD_LINK` and `NSC_P1_NSLINK` allow alternate toolchains.
- The gate requires both linkers instead of turning absence into a false pass.
- Test artifacts live under `NSC_TEST_EVIDENCE_DIR` and are removed on success.

## Evidencia / Evidence

```text
nsc_stage46_1_dll_cross_linker_matrix_tests: 98/98 PASS
cases: 2 producer linkers × 2 consumer linkers × 2 producer opts × 2 consumer opts
execution: 16/16 returned 42
build root: A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls\build-clang-lldlink
```

No se instaló producto ni se usó `A:\nsc-devel`.
No product was installed and `A:\nsc-devel` was not used.

## Commit / Commit

```text
Certifica la matriz cruzada de DLLs y linkers / Certify the cross-linker DLL matrix
```
