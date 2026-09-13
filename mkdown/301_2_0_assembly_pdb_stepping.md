# 301 — PDB y stepping dentro de asm / Assembly PDB and stepping

## Resultado / Result

Los perfiles de depuración estándar ya atribuyen cada instrucción física de un bloque `asm` a su
línea NativeSharp. El PDB conserva direcciones distintas para instrucciones consecutivas sin
dividir el bloque ni cambiar su semántica. / Standard debug profiles now attribute each physical
instruction in an `asm` block to its NativeSharp source line. The PDB retains distinct addresses
for consecutive instructions without splitting the block or changing its semantics.

## Archivos y funciones / Files and functions

- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - `debugFile()` asigna números CodeView estables en orden de primera utilización.
  - `debugCodeViewFileNumber()` recupera la identidad del `DIFile` checksummed y remapeado.
  - `lowerInlineAssembly()` genera `.cv_loc` por línea cuando el perfil de debug está habilitado.
- `tests/integration/asm_debug`
  - Contiene un programa O0 real con dos instrucciones asm y salida observable 42.
- `cmake/VerifyNativeSharpAssemblyDebugO0.cmake`
  - Compila, enlaza, ejecuta e inspecciona el PDB con `llvm-pdbutil`.
- `CMakeLists.txt`
  - Registra la prueba enfocada `nsc.debug.assembly-stepping-o0`.
- `docs/1.0.1`
  - Actualiza contrato de debug, especificación, contrato del compilador, mapas, estado y roadmap.

## Implementación y decisiones / Implementation and design decisions

El bloque permanece como una sola operación LLVM inline asm. Separarlo por líneas habría cambiado
la vida de registros, los resultados aggregate, los constraints `+r` y los branches locales. Las
directivas estándar `.cv_loc` cambian únicamente el programa de líneas CodeView y se omiten con
`-g0`. El número de archivo se deriva del mismo `DIFile` que ya contiene path remapeado y SHA-256. /
The block remains one LLVM inline-asm operation. Splitting it by line would change register
lifetimes, aggregate results, `+r` constraints, and local branches. Standard `.cv_loc` directives
affect only the CodeView line program and are omitted under `-g0`; their file number comes from the
same remapped, SHA-256-identified `DIFile`.

## Evidencia enfocada / Focused evidence

```text
nsc.debug.assembly-stepping-o0: passed
program exit code: 42
PDB source identity: remapped path plus SHA-256
asm line 10 address != asm line 11 address
```

No se ejecutó baseline. / No baseline was run.

## Mensaje de commit / Commit message

**Título / Title**

`Integra stepping PDB por instrucción asm / Add per-instruction PDB stepping for assembly`

**Descripción / Description**

`Emite ubicaciones CodeView por línea sin dividir el contrato asm y certifica direcciones distintas en O0. / Emit per-line CodeView locations without splitting the asm contract and certify distinct O0 addresses.`
