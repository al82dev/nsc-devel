# SelfHost.Basic — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 3 archivos, 143 líneas, 4137 bytes, 38 declaraciones físicas.
- **Diseño:** 4 contratos y 28 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Proveer identidades tipadas, hash determinista y almacenamiento verificado por generación para todas las etapas self-host posteriores.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** NativeSharp.Core.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000592 | `selfhost/nsc/Basic/GenerationArena.ns` | 57 | 13 |
| PF-000593 | `selfhost/nsc/Basic/Identifiers.ns` | 67 | 23 |
| PF-000594 | `selfhost/nsc/Basic/StableHash.ns` | 19 | 2 |

## 4. Contratos

### 4.1 CSH-BAS-001 — Typed source identities and compiler results

- **Símbolos:** NativeSharp.Compiler.Basic.ArenaHandle.Generation;NativeSharp.Compiler.Basic.ArenaHandle.Size;NativeSharp.Compiler.Basic.ArenaHandle.IsEmpty;NativeSharp.Compiler.Basic.GenerationArena.Allocate;NativeSharp.Compiler.Basic.GenerationArena.Generation;NativeSharp.Compiler.Basic.GenerationArena.BytesUsed;NativeSharp.Compiler.Basic.FileId;NativeSharp.Compiler.Basic.FileId.FileId;NativeSharp.Compiler.Basic.FileId.Value;NativeSharp.Compiler.Basic.FileId.IsValid;NativeSharp.Compiler.Basic.FileId.Invalid;NativeSharp.Compiler.Basic.FileId.Equals;NativeSharp.Compiler.Basic.SourcePosition;NativeSharp.Compiler.Basic.SourcePosition.SourcePosition;NativeSharp.Compiler.Basic.SourcePosition.File;NativeSharp.Compiler.Basic.SourcePosition.Offset;NativeSharp.Compiler.Basic.SourcePosition.IsValid;NativeSharp.Compiler.Basic.SourceRange;NativeSharp.Compiler.Basic.SourceRange.SourceRange;NativeSharp.Compiler.Basic.SourceRange.Begin;NativeSharp.Compiler.Basic.SourceRange.End;NativeSharp.Compiler.Basic.SourceRange.Length;NativeSharp.Compiler.Basic.SourceRange.IsValid;NativeSharp.Compiler.Basic.CompilerResult;NativeSharp.Compiler.Basic.CompilerResult.CompilerResult;NativeSharp.Compiler.Basic.CompilerResult.Succeeded;NativeSharp.Compiler.Basic.CompilerResult.ExitCode;NativeSharp.Compiler.Basic.StableHash.Text.
- **Entradas:** typed IDs positions ranges and exit result.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable values validity equality and success/failure; stable values validity equality and success/failure are coherent and observable.
- **Invariante:** zero is the only invalid ID and ranges are half-open within one file.
- **Errores:** invalid identity or inconsistent range remains observably invalid.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact fields validity equality length and exit code.

### 4.2 CSH-BAS-002 — Generational arena handles

- **Símbolos:** NativeSharp.Compiler.Basic.ArenaHandle;NativeSharp.Compiler.Basic.ArenaHandle.ArenaHandle;NativeSharp.Compiler.Basic.GenerationArena;NativeSharp.Compiler.Basic.GenerationArena.GenerationArena.
- **Entradas:** byte size generation and arena state.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** handle address generation size resolution and usage; handle address generation size resolution and usage are coherent and observable.
- **Invariante:** reset advances generation and stale handles never resolve as current storage.
- **Errores:** empty stale or out-of-range handles resolve to the documented sentinel.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** address generation size usage and stale-handle truth table.

### 4.3 CSH-BAS-003 — Stable text hash

- **Símbolos:** NativeSharp.Compiler.Basic.StableHash.
- **Entradas:** UTF-16 string including empty and non-ASCII.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable unsigned hash; stable unsigned hash are coherent and observable.
- **Invariante:** same text always yields the same value independent of process order.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact known vectors and repeated-input identity.

### 4.4 CSH-BAS-004 — Basic ownership and value lifetime

- **Símbolos:** NativeSharp.Compiler.Basic.ArenaHandle.Address;NativeSharp.Compiler.Basic.GenerationArena.Resolve;NativeSharp.Compiler.Basic.GenerationArena.Reset;NativeSharp.Compiler.Basic.CompilerResult.Success;NativeSharp.Compiler.Basic.CompilerResult.Failure.
- **Entradas:** owned arena or value operation.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** non-owning view or value result; non-owning view or value result are coherent and observable.
- **Invariante:** views remain valid only for their documented owner generation.
- **Errores:** stale generation is rejected without publication.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** generation transition and lifetime boundary.

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

La integración directa es: NativeSharp.Core. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 3 archivos, 38 declaraciones, 4 contratos y 28 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
