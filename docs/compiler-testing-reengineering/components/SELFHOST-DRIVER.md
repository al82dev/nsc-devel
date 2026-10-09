# SelfHost.Driver — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 797 líneas, 45390 bytes, 26 declaraciones físicas.
- **Diseño:** 10 contratos y 79 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Exponer la frontera CLI que compone carga, frontend, IR, codegen, auditorías de proyecto y verificaciones bootstrap.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** all self-host compiler components;NativeSharp.Core.IO.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000598 | `selfhost/nsc/Driver/Program.ns` | 797 | 26 |

## 4. Contratos

### 4.1 CSH-DRV-001 — CLI identity help and diagnostic presentation

- **Símbolos:** NativeSharp.Compiler.Driver.Program;NativeSharp.Compiler.Driver.Program.PrintVersion;NativeSharp.Compiler.Driver.Program.PrintHelp;NativeSharp.Compiler.Driver.Program.EmitDiagnostic.
- **Entradas:** command line and diagnostic format selection.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** version/help text or diagnostic on selected stream; version/help text or diagnostic on selected stream are coherent and observable.
- **Invariante:** read-only commands do not enter compilation.
- **Errores:** unknown options use catalogued exit identity.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** exit code streams and semantic fields.

### 4.2 CSH-DRV-002 — Source loading and frontend dumps

- **Símbolos:** NativeSharp.Compiler.Driver.Program.ReadSource;NativeSharp.Compiler.Driver.Program.DumpTokens;NativeSharp.Compiler.Driver.Program.DumpAst;NativeSharp.Compiler.Driver.Program.DumpSemantic.
- **Entradas:** source path and dump command.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** tokens AST symbols or semantic dump; tokens AST symbols or semantic dump are coherent and observable.
- **Invariante:** each dump is deterministic for the same source and stops after owned frontend failure.
- **Errores:** read lex parse or semantic failure maps to nonzero result.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** normalized dump text and exit code.

### 4.3 CSH-DRV-003 — IR preparation verification and dumps

- **Símbolos:** NativeSharp.Compiler.Driver.Program.RunIr;NativeSharp.Compiler.Driver.Program.DumpProjectIr.
- **Entradas:** source/project and IR command.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** HIR MIR CFG dominance SSA or pass dump; HIR MIR CFG dominance SSA or pass dump are coherent and observable.
- **Invariante:** IR is admitted only through ValidatedIrProgram.
- **Errores:** failed producer prevents downstream dump.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** selected dump exact structure and exit code.

### 4.4 CSH-DRV-004 — Self-test command

- **Símbolos:** NativeSharp.Compiler.Driver.Program.SelfTest.
- **Entradas:** self-test command.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** aggregate internal result; aggregate internal result are coherent and observable.
- **Invariante:** the command exercises declared bootstrap primitives without filesystem publication.
- **Errores:** first failed check returns stable nonzero result.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** check identities and final exit code.

### 4.5 CSH-DRV-005 — Single-source backend emission

- **Símbolos:** NativeSharp.Compiler.Driver.Program.EmitBackend.
- **Entradas:** source output optimization and artifact kind.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** object or assembly artifact; object or assembly artifact are coherent and observable.
- **Invariante:** frontend and IR success precede backend emission.
- **Errores:** producer/backend code is preserved and no success is printed on failure.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** stage order artifact bytes and exit code.

### 4.6 CSH-DRV-006 — Project backend emission

- **Símbolos:** NativeSharp.Compiler.Driver.Program.EmitProjectBackend.
- **Entradas:** project graph output optimization artifact and library mode.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** project object or library object set; project object or library object set are coherent and observable.
- **Invariante:** source graph module set semantic and IR stages complete before codegen.
- **Errores:** any unit or dependency failure blocks emission.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** unit order stage diagnostics artifacts and exit code.

### 4.7 CSH-DRV-007 — Bootstrap backend emission

- **Símbolos:** NativeSharp.Compiler.Driver.Program.EmitBootstrapBackend.
- **Entradas:** bootstrap project/output request.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** bootstrap compiler object; bootstrap compiler object are coherent and observable.
- **Invariante:** generation profile verification precedes all self-host emission.
- **Errores:** profile/dependency/backend failure remains attributable.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** verified stage sequence artifact and exit result.

### 4.8 CSH-DRV-008 — Project semantic audit commands

- **Símbolos:** NativeSharp.Compiler.Driver.Program.AuditProjectPipeline;NativeSharp.Compiler.Driver.Program.DumpProjectLayout;NativeSharp.Compiler.Driver.Program.DumpProjectGenerics;NativeSharp.Compiler.Driver.Program.DumpProjectCallables;NativeSharp.Compiler.Driver.Program.DumpProjectEh;NativeSharp.Compiler.Driver.Program.DumpSourceGraph;NativeSharp.Compiler.Driver.Program.DumpModuleSet.
- **Entradas:** project and audit subcommand.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** deterministic structural report; deterministic structural report are coherent and observable.
- **Invariante:** commands observe one coherent project/module model.
- **Errores:** project or semantic failure blocks report success.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** report records ordering identities counts and exit code.

### 4.9 CSH-DRV-009 — Frozen bootstrap dependency verification

- **Símbolos:** NativeSharp.Compiler.Driver.Program.VerifyBootstrapDependencies.
- **Entradas:** dependency inventory and expected contract.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** pass/fail dependency report; pass/fail dependency report are coherent and observable.
- **Invariante:** all required producer artifacts are checked without accepting stale substitutes.
- **Errores:** missing or mismatched dependency fails the verification.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** artifact identities reasons and final status.

### 4.10 CSH-DRV-010 — Project command dispatch and response files

- **Símbolos:** NativeSharp.Compiler.Driver.Program.CheckProject;NativeSharp.Compiler.Driver.Program.ProjectCommand;NativeSharp.Compiler.Driver.Program.RunResponseFile;NativeSharp.Compiler.Driver.Program.Main.
- **Entradas:** argv or response-file tokens.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** selected command result; selected command result are coherent and observable.
- **Invariante:** dispatch consumes each option exactly once and preserves nested command exit code.
- **Errores:** unknown malformed recursive or unreadable input is rejected.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** selected handler arguments recursion boundary and exit code.

## 5. Estructuras, estados y algoritmos

Las estructuras concretas son las declaraciones trazadas en `inventory/symbols.csv`. Los algoritmos se dividen por contrato para impedir que una prueba de integración oculte una falla de cursor, identidad, grafo, ownership, ABI o publicación. Los builders conservan estado sólo dentro de su owner; los modelos aceptados se publican después de sus verificadores.

## 6. Invariantes y transiciones

El estado inicial, las mutaciones admitidas, la publicación y el fallo se prueban por contrato. Los contratos de valor son puros; los de builder, grafo, pipeline, proceso o emisión comprueban transiciones y ausencia de éxito parcial.

## 7. Ownership y lifetime

Los tipos `owned`, `raii`, handles, arenas, listas y punteros se caracterizan según la implementación congelada. Una prueba no debe forzar un feature de ownership ajeno al contrato ni convertir una observación actual en política futura del lenguaje.

## 8. Complejidad y límites

Las búsquedas lineales, recorridos de árboles/grafos, cierres y pipelines se ejercitan con vacío, caso nominal y escala. La Fase 1 no promete cotas no declaradas: cualquier recorrido recursivo vigente queda como riesgo observable y contradicción, no como refactor autorizado.

## 9. Errores y recuperación

El oracle identifica categoría, propietario, progreso y publicación. Parser/Lexer deben avanzar o terminar; builders y pipelines detienen consumidores; diagnósticos conservan identidad; emisores no publican éxito si un productor falla.

## 10. Integración

La integración directa es: all self-host compiler components;NativeSharp.Core.IO. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 26 declaraciones, 10 contratos y 79 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
