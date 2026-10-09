# SelfHost.Project — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 23 líneas, 1091 bytes, 0 declaraciones físicas.
- **Diseño:** 3 contratos y 22 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Definir el conjunto físico de fuentes del proyecto self-host y sus cuatro dependencias explícitas.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** NativeSharp.Core;NativeSharp.Core.Collections.Generic;NativeSharp.Core.IO;NativeSharp.LLVM.C.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000604 | `selfhost/nsc/NativeSharp.Compiler.nsproj` | 23 | 0 |

## 4. Contratos

### 4.1 CSH-PRO-001 — Self-host project identity and source globs

- **Símbolos:** selfhost/nsc/NativeSharp.Compiler.nsproj.
- **Entradas:** project XML.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** project name and ordered source include patterns; project name and ordered source include patterns are coherent and observable.
- **Invariante:** all self-host source domains are included without circular generated wrappers.
- **Errores:** missing domain or malformed XML prevents project admission.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** resolved source set compared with canonical 27 sources.

### 4.2 CSH-PRO-002 — Self-host project dependency closure

- **Símbolos:** selfhost/nsc/NativeSharp.Compiler.nsproj.
- **Entradas:** four project references.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** Core Collections IO and LLVM-C dependency graph; Core Collections IO and LLVM-C dependency graph are coherent and observable.
- **Invariante:** references are explicit and acyclic at project level.
- **Errores:** missing or incompatible dependency blocks build producer.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact normalized reference set and order.

### 4.3 CSH-PRO-003 — Project/source inventory coherence

- **Símbolos:** selfhost/nsc/NativeSharp.Compiler.nsproj.
- **Entradas:** manifest and filesystem inventory.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** coherent 27-source compilation set; coherent 27-source compilation set are coherent and observable.
- **Invariante:** every source belongs to one declared domain and no hidden implementation is introduced.
- **Errores:** unmatched or duplicate source is a gate failure.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** resolved paths cardinality uniqueness and component ownership.

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

La integración directa es: NativeSharp.Core;NativeSharp.Core.Collections.Generic;NativeSharp.Core.IO;NativeSharp.LLVM.C. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 0 declaraciones, 3 contratos y 22 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
