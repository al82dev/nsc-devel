# NS3-BASELINE-SELFHOST-RESERVED-ALIASES

## Problema

El reporte NSLink de la corrida `20261004-211739-4bfcc1c2` registró 71 fallos, 70 de ellos asociados a una sola causa. El compilador detenía el análisis de `selfhost/nsc/CodeGen/LlvmBackend.ns` en la línea 849 y producía miles de diagnósticos derivados.

Los alias numéricos `i8`, `i32` e `i64` pasaron a ser palabras reservadas del lenguaje en `NS3-T13-F02`, pero el backend autocontenido todavía los utilizaba como nombres de variables y parámetros. El lexer los clasificaba como tipos y el parser ya no podía interpretarlos como identificadores.

## Cambio

- Se conservaron los alias numéricos públicos; no se debilitó ni revirtió `NS3-T13-F02`.
- Se migraron exclusivamente los identificadores internos del backend:
  - `i8` a `int8Type`;
  - `i32` a `int32Type`;
  - `i64` a `int64Type`.
- No se modificaron cadenas, nombres de símbolos LLVM, comentarios ni comportamiento de generación de código.
- Se auditó todo `selfhost/nsc`: el único archivo con identificadores incompatibles era `CodeGen/LlvmBackend.ns`.

## Archivos

- Modificado: `selfhost/nsc/CodeGen/LlvmBackend.ns`.
- Creado: `docs/3.0/items/NS3-BASELINE-SELFHOST-RESERVED-ALIASES.md`, para conservar causa, alcance y evidencia del arreglo.

## Mejora

El compilador autocontenido vuelve a ser fuente válida bajo las reglas de NativeSharp 3. Esto elimina la causa común que cerraba anticipadamente 70 pruebas de NS3, self-host, bibliotecas y compatibilidad.

Commit funcional: `80e4e959a`.

## Validación puntual

- Auditoría de alias reservados usados como identificadores en `selfhost/nsc`: cero restantes.
- `--check-project-syntax` sobre `NativeSharp.Compiler.nsproj`: **PASS**, 98 unidades sintácticas.
- Emisión directa del compilador autocontenido con O0, 12 trabajos, `NS_LIB_ROOT=A:\native-sharp-3.0-baseline\lib` y NSLink de `A:\native-sharp-3.0-baseline\bin\nslink.exe`: **PASS** en 2.73 s.
- Artefacto de comprobación: `build-test-nslink/selfhost-sh27/nsc-next-o0-reserved-alias-fix.exe`.
- La invocación aislada inicial de SH27 quedó bloqueada antes de llegar al self-host porque el staging local no tenía `lib/NativeSharp.Core.IO/NativeSharpCore.lib`; no se trató como fallo funcional y se sustituyó por la emisión directa con `NS_LIB_ROOT`.

No se ejecutó baseline, minibaseline ni CTest general.
