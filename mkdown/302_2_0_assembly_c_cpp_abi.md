# 302 — Certificación ABI C/C++ de asm / C and C++ assembly ABI certification

## Resultado / Result

Las funciones públicas `export extern C` que contienen assembly integrado están certificadas por
hosts independientes C11 y C++20. Los dos hosts compilan contra el header generado, enlazan la
import library real y ejecutan la DLL O0. / Public `export extern C` functions containing integrated
assembly are certified by independent C11 and C++20 hosts. Both compile against the generated
header, link the real import library, and execute the O0 DLL.

## Archivos y funciones / Files and functions

- `tests/integration/asm_abi/AssemblyAbi.ns`
  - `NSAsmAdd()` certifica inputs y output de registros.
  - `NSAsmAddMemory()` certifica acceso visible por puntero y clobbers `cc`/`memory`.
  - `NSAsmUseNonvolatile()` modifica `rbx` y obliga al backend a preservarlo.
- `tests/integration/asm_abi/host.c`
  - Host C11 freestanding que consume sólo `AssemblyAbi.h` y `AssemblyAbi.lib`.
- `tests/integration/asm_abi/host.cpp`
  - Host C++20 freestanding contra la misma frontera C.
- `cmake/VerifyNativeSharpAssemblyAbiO0.cmake`
  - Produce la DLL, compila/enlaza ambos hosts y exige código de salida 42.
- `CMakeLists.txt`
  - Registra `nsc.assembly.abi-c-cpp-o0`.
- `docs/1.0.1`
  - Actualiza especificación, contrato, mapas, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

La prueba no acepta sólo que el símbolo enlace. Cada host coloca un sentinel distinto en `rbx`,
llama una función cuyo asm sobrescribe ese registro y comprueba el sentinel al volver. Esto mide la
obligación no volátil desde fuera del compilador. Cálculo escalar y memoria se prueban por separado
para evitar que una ruta correcta oculte otra rota. / The test does not merely accept successful
symbol linkage. Each host places a different sentinel in `rbx`, calls a function whose assembly
overwrites that register, and checks the sentinel after return. Scalar calculation and pointer
memory are exercised separately so one working path cannot hide another broken path.

## Evidencia enfocada / Focused evidence

```text
nsc.assembly.abi-c-cpp-o0: passed
NativeSharp DLL/header/import library: generated
C11 host: compiled, linked, exit 42
C++20 host: compiled, linked, exit 42
Win64 rbx preservation: verified by both hosts
```

No se ejecutó baseline. / No baseline was run.

## Mensaje de commit / Commit message

**Título / Title**

`Certifica ABI C y C++ de assembly / Certify the C and C++ assembly ABI`

**Descripción / Description**

`Ejecuta exports asm desde hosts independientes y verifica registros, memoria y preservación no volátil Win64. / Execute asm exports from independent hosts and verify registers, memory, and Win64 nonvolatile preservation.`
