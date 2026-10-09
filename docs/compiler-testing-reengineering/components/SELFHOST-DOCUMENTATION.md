# SelfHost.Documentation — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 18 líneas, 1235 bytes, 0 declaraciones físicas.
- **Diseño:** 2 contratos y 12 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Declarar arquitectura self-host, historia de etapas y restricciones bootstrap como evidencia documental, no como verdad ejecutable.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** selfhost/nsc source tree;NativeSharp.Compiler.nsproj.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000608 | `selfhost/nsc/README.md` | 18 | 0 |

## 4. Contratos

### 4.1 CSH-DOC-001 — Self-host architecture declaration

- **Símbolos:** selfhost/nsc/README.md.
- **Entradas:** README at frozen revision.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** declared component boundaries and stage history; declared component boundaries and stage history are coherent and observable.
- **Invariante:** the document is evidence, not executable truth.
- **Errores:** document/code disagreement is a recorded contradiction.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** trace each claim to current source or mark historical.

### 4.2 CSH-DOC-002 — Bootstrap profile and hidden-implementation prohibition

- **Símbolos:** selfhost/nsc/README.md.
- **Entradas:** README policy statements.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** documented bootstrap restrictions; documented bootstrap restrictions are coherent and observable.
- **Invariante:** no wrapper C++ or hidden alternate frontend is permitted by the document.
- **Errores:** current version residue remains an open contradiction.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** source/project inventory and explicit contradiction record.

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

La integración directa es: selfhost/nsc source tree;NativeSharp.Compiler.nsproj. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 0 declaraciones, 2 contratos y 12 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
