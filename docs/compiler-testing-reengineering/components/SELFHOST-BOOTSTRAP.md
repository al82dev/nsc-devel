# SelfHost.Bootstrap — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 1 archivo, 94 líneas, 4177 bytes, 4 declaraciones físicas.
- **Diseño:** 3 contratos y 23 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Admitir sólo el subconjunto congelado de fuente/proyecto bootstrap y orquestar su primera generación sin fallback oculto.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Basic;SelfHost.Project.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000595 | `selfhost/nsc/Bootstrap/GenerationBuilder.ns` | 94 | 4 |

## 4. Contratos

### 4.1 CSH-BOO-001 — Bootstrap source profile verification

- **Símbolos:** NativeSharp.Compiler.Bootstrap.GenerationBuilder.VerifySource.
- **Entradas:** self-host source text and path.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** accepted source or first profile violation; accepted source or first profile violation are coherent and observable.
- **Invariante:** only the frozen bootstrap subset is admitted.
- **Errores:** forbidden syntax ownership or profile constructs return stable failure.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** accept/reject matrix with first rejected construct.

### 4.2 CSH-BOO-002 — Bootstrap manifest verification

- **Símbolos:** NativeSharp.Compiler.Bootstrap.GenerationBuilder.VerifyManifest.
- **Entradas:** project manifest text.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** validated bootstrap project description; validated bootstrap project description are coherent and observable.
- **Invariante:** required profile references and source declarations are coherent.
- **Errores:** missing conflicting or unsupported manifest entries are rejected.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** normalized manifest model or exact rejection category.

### 4.3 CSH-BOO-003 — Bootstrap generation build orchestration

- **Símbolos:** NativeSharp.Compiler.Bootstrap.GenerationBuilder;NativeSharp.Compiler.Bootstrap.GenerationBuilder.Build.
- **Entradas:** verified sources manifest compiler and output request.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** bootstrap generation result and diagnostics; bootstrap generation result and diagnostics are coherent and observable.
- **Invariante:** no downstream generation begins after admission failure.
- **Errores:** producer failure stops publication and preserves its exit identity.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** stage sequence outputs diagnostics and exit result.

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

La integración directa es: SelfHost.Basic;SelfHost.Project. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 1 archivos, 4 declaraciones, 3 contratos y 23 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
