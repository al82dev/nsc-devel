# SelfHost.Diagnostics — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 86 líneas, 2868 bytes, 15 declaraciones físicas.
- **Diseño:** 4 contratos y 28 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Representar y renderizar diagnósticos bootstrap estables en forma humana y JSON.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Basic;NativeSharp.Core.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000597 | `selfhost/nsc/Diagnostics/Diagnostics.ns` | 86 | 15 |

## 4. Contratos

### 4.1 CSH-DIA-001 — Diagnostic severity and immutable record

- **Símbolos:** NativeSharp.Compiler.Diagnostics.DiagnosticSeverity;NativeSharp.Compiler.Diagnostics.Diagnostic;NativeSharp.Compiler.Diagnostics.Diagnostic.Diagnostic;NativeSharp.Compiler.Diagnostics.Diagnostic.Code;NativeSharp.Compiler.Diagnostics.Diagnostic.Severity;NativeSharp.Compiler.Diagnostics.Diagnostic.Argument;NativeSharp.Compiler.Diagnostics.Diagnostic.ExitCode;NativeSharp.Compiler.Diagnostics.DiagnosticRenderer.
- **Entradas:** code severity argument and exit code.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable diagnostic record; stable diagnostic record are coherent and observable.
- **Invariante:** construction preserves all fields.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact field values and copy lifetime.

### 4.2 CSH-DIA-002 — Diagnostic catalog

- **Símbolos:** NativeSharp.Compiler.Diagnostics.DiagnosticCatalog;NativeSharp.Compiler.Diagnostics.DiagnosticCatalog.MissingSource;NativeSharp.Compiler.Diagnostics.DiagnosticCatalog.UnknownOption.
- **Entradas:** missing path or unknown option.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** catalogued diagnostic; catalogued diagnostic are coherent and observable.
- **Invariante:** code severity and exit mapping remain stable per catalogue entry.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact code severity argument and exit code.

### 4.3 CSH-DIA-003 — Human diagnostic rendering

- **Símbolos:** NativeSharp.Compiler.Diagnostics.DiagnosticRenderer.Human;NativeSharp.Compiler.Diagnostics.DiagnosticRenderer.NormalizedPath.
- **Entradas:** diagnostic and source path.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** single human-readable line; single human-readable line are coherent and observable.
- **Invariante:** path normalization and stable code/argument placement do not depend on cwd.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact normalized semantic fields ignoring incidental locale.

### 4.4 CSH-DIA-004 — JSON diagnostic rendering

- **Símbolos:** NativeSharp.Compiler.Diagnostics.DiagnosticRenderer.JsonEscape;NativeSharp.Compiler.Diagnostics.DiagnosticRenderer.Json.
- **Entradas:** diagnostic and path with control/non-ASCII text.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** valid deterministic JSON object; valid deterministic JSON object are coherent and observable.
- **Invariante:** every control character is escaped and fields retain diagnostic identity.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** parse JSON then compare exact typed fields and escaping.

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

La integración directa es: SelfHost.Basic;NativeSharp.Core. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 15 declaraciones, 4 contratos y 28 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
