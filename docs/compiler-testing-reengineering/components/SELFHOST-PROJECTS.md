# SelfHost.Projects — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 2 archivos, 723 líneas, 34869 bytes, 79 declaraciones físicas.
- **Diseño:** 12 contratos y 95 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Cargar XML de proyecto, resolver grafos de fuentes/dependencias, leer/escribir metadata, derivar identidad de cache y planear productos.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Source;SelfHost.Parser;SelfHost.Sema;filesystem.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000606 | `selfhost/nsc/Projects/ProjectSystem.ns` | 328 | 46 |
| PF-000607 | `selfhost/nsc/Projects/SourceGraph.ns` | 395 | 33 |

## 4. Contratos

### 4.1 CSH-PRJ-001 — Project model values and ordered references

- **Símbolos:** NativeSharp.Compiler.Projects.ProjectModel;NativeSharp.Compiler.Projects.ProjectModel.ProjectModel;NativeSharp.Compiler.Projects.ProjectModel.PathValue;NativeSharp.Compiler.Projects.ProjectModel.Name;NativeSharp.Compiler.Projects.ProjectModel.OutputType;NativeSharp.Compiler.Projects.ProjectModel.Sdk;NativeSharp.Compiler.Projects.ProjectModel.SetName;NativeSharp.Compiler.Projects.ProjectModel.SetOutputType;NativeSharp.Compiler.Projects.ProjectModel.SetSdk;NativeSharp.Compiler.Projects.ProjectModel.AddSource;NativeSharp.Compiler.Projects.ProjectModel.AddReference;NativeSharp.Compiler.Projects.ProjectModel.AddMetadataReference;NativeSharp.Compiler.Projects.ProjectModel.AddPackageReference;NativeSharp.Compiler.Projects.ProjectModel.AddNativeReference;NativeSharp.Compiler.Projects.ProjectModel.SourceCount;NativeSharp.Compiler.Projects.ProjectModel.SourceAt;NativeSharp.Compiler.Projects.ProjectModel.ReferenceCount;NativeSharp.Compiler.Projects.ProjectModel.ReferenceAt;NativeSharp.Compiler.Projects.ProjectModel.MetadataReferenceCount;NativeSharp.Compiler.Projects.ProjectModel.MetadataReferenceAt;NativeSharp.Compiler.Projects.ProjectModel.PackageReferenceCount;NativeSharp.Compiler.Projects.ProjectModel.PackageReferenceAt;NativeSharp.Compiler.Projects.ProjectModel.NativeReferenceCount;NativeSharp.Compiler.Projects.ProjectModel.NativeReferenceAt;NativeSharp.Compiler.Projects.ProjectLoader;NativeSharp.Compiler.Projects.MetadataContract.Write;NativeSharp.Compiler.Projects.MetadataContract.ReadCompatible;NativeSharp.Compiler.Projects.ProjectCache.Key;NativeSharp.Compiler.Projects.SourceUnit.Relative;NativeSharp.Compiler.Projects.SourceUnit.PathValue;NativeSharp.Compiler.Projects.SourceUnit.DisplayPath;NativeSharp.Compiler.Projects.SourceUnit.RelativePath;NativeSharp.Compiler.Projects.SourceUnit.Owner;NativeSharp.Compiler.Projects.SourceUnit.Order;NativeSharp.Compiler.Projects.SourceGraph.UnitCount;NativeSharp.Compiler.Projects.SourceGraph.UnitAt;NativeSharp.Compiler.Projects.SourceGraph.ProjectCount;NativeSharp.Compiler.Projects.SourceGraph.ProjectAt;NativeSharp.Compiler.Projects.SourceGraph.Diagnostic;NativeSharp.Compiler.Projects.SourceGraph.Failed;NativeSharp.Compiler.Projects.SourceGraph.Fail;NativeSharp.Compiler.Projects.SourceGraph.Contains;NativeSharp.Compiler.Projects.SourceGraph.ContainsProject;NativeSharp.Compiler.Projects.SourceGraph.AddProject;NativeSharp.Compiler.Projects.SourceGraph.AddUnit;NativeSharp.Compiler.Projects.IncludeExpander.PatternExtension;NativeSharp.Compiler.Projects.IncludeExpander.TrimTrailingSeparators;NativeSharp.Compiler.Projects.IncludeExpander.PatternRoot.
- **Entradas:** path name output SDK sources and references.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** mutable ordered project model; mutable ordered project model are coherent and observable.
- **Invariante:** insertion order and reference category remain distinct.
- **Errores:** none declared beyond invalid caller index.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact fields counts and indexed values.

### 4.2 CSH-PRJ-002 — Project XML extraction

- **Símbolos:** NativeSharp.Compiler.Projects.ProjectLoader.FindFirstSource;NativeSharp.Compiler.Projects.ProjectLoader.Tag;NativeSharp.Compiler.Projects.ProjectLoader.Attributes;NativeSharp.Compiler.Projects.ProjectLoader.MetadataAttributes;NativeSharp.Compiler.Projects.ProjectLoader.DependencyAttributes;NativeSharp.Compiler.Projects.ProjectLoader.Load.
- **Entradas:** project XML and base path.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** populated ProjectModel; populated ProjectModel are coherent and observable.
- **Invariante:** recognized tags/attributes map once and paths retain project provenance.
- **Errores:** missing source malformed tag/attribute or unreadable project returns failure.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** project model and first failure.

### 4.3 CSH-PRJ-003 — Project verification and first source

- **Símbolos:** NativeSharp.Compiler.Projects.ProjectLoader.Verify;NativeSharp.Compiler.Projects.ProjectLoader.FirstSourcePath.
- **Entradas:** loaded ProjectModel and filesystem.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** verification result and first source path; verification result and first source path are coherent and observable.
- **Invariante:** required properties sources and references are coherent before consumers run.
- **Errores:** missing duplicate or incompatible entries reject the project.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** validation status reason and selected path.

### 4.4 CSH-PRJ-004 — Project deterministic dump

- **Símbolos:** NativeSharp.Compiler.Projects.ProjectDumper;NativeSharp.Compiler.Projects.ProjectDumper.NextSource;NativeSharp.Compiler.Projects.ProjectDumper.Dump;NativeSharp.Compiler.Projects.SourceGraphDumper.Dump.
- **Entradas:** ProjectModel.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable text report; stable text report are coherent and observable.
- **Invariante:** sources and references are emitted in deterministic order.
- **Errores:** none beyond admitted model.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact normalized dump and source order.

### 4.5 CSH-PRJ-005 — NSMETA compatibility contract

- **Símbolos:** NativeSharp.Compiler.Projects.MetadataContract.
- **Entradas:** metadata name and bytes.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** NSMETA/2 text or compatibility decision; NSMETA/2 text or compatibility decision are coherent and observable.
- **Invariante:** writer fields are stable and reader admits only declared compatible prefixes.
- **Errores:** unknown/truncated metadata is incompatible.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** header producer ABI compatibility fields and decision.

### 4.6 CSH-PRJ-006 — Project cache identity

- **Símbolos:** NativeSharp.Compiler.Projects.ProjectCache.
- **Entradas:** project content references toolchain and options.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable cache key; stable cache key are coherent and observable.
- **Invariante:** semantically identical ordered input yields identical key.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** known vectors and mutation sensitivity.

### 4.7 CSH-PRJ-007 — Toolchain manifest

- **Símbolos:** NativeSharp.Compiler.Projects.ToolchainManifest.
- **Entradas:** tool paths versions ABI and language metadata.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** deterministic manifest text; deterministic manifest text are coherent and observable.
- **Invariante:** all selected producer identities are represented.
- **Errores:** missing producer data remains explicit rather than guessed.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** parsed fields and exact stable ordering.

### 4.8 CSH-PRJ-008 — Product artifact plan

- **Símbolos:** NativeSharp.Compiler.Projects.ToolchainManifest.Create;NativeSharp.Compiler.Projects.ProductPlan;NativeSharp.Compiler.Projects.ProductPlan.Extension;NativeSharp.Compiler.Projects.ProductPlan.Create.
- **Entradas:** output type name root and artifact options.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** ordered product paths and kinds; ordered product paths and kinds are coherent and observable.
- **Invariante:** extension and artifact set follow output type without silent fallback.
- **Errores:** unsupported output type is rejected.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** artifact list extensions roots and order.

### 4.9 CSH-PRJ-009 — Source graph units and identity

- **Símbolos:** NativeSharp.Compiler.Projects.SourceUnit;NativeSharp.Compiler.Projects.SourceUnit.SourceUnit;NativeSharp.Compiler.Projects.SourceGraph;NativeSharp.Compiler.Projects.SourceGraph.SourceGraph.
- **Entradas:** owner paths relative paths and order.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** deduplicated ordered graph; deduplicated ordered graph are coherent and observable.
- **Invariante:** one canonical path maps to one unit and project identity.
- **Errores:** duplicate/conflicting path is reported by graph.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** unit/project tables order and diagnostic.

### 4.10 CSH-PRJ-010 — Include expansion and deterministic enumeration

- **Símbolos:** NativeSharp.Compiler.Projects.IncludeExpander;NativeSharp.Compiler.Projects.IncludeExpander.CollectSorted;NativeSharp.Compiler.Projects.IncludeExpander.Expand;NativeSharp.Compiler.Projects.IncludeExpander.ExpandFrom.
- **Entradas:** project include patterns root and filesystem tree.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** sorted source candidates; sorted source candidates are coherent and observable.
- **Invariante:** recursive/nonrecursive patterns remain confined and results are stable.
- **Errores:** invalid root pattern or enumeration failure is diagnosed.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** normalized paths sort order duplicates and error.

### 4.11 CSH-PRJ-011 — Project dependency traversal and source graph build

- **Símbolos:** NativeSharp.Compiler.Projects.SourceGraphBuilder;NativeSharp.Compiler.Projects.SourceGraphBuilder.Visit;NativeSharp.Compiler.Projects.SourceGraphBuilder.Build.
- **Entradas:** root project and project-reference graph.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** ordered SourceGraph; ordered SourceGraph are coherent and observable.
- **Invariante:** dependencies precede consumers and cycles/duplicates terminate deterministically.
- **Errores:** unreadable invalid or cyclic project records a graph failure.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** project/unit order edges and diagnostic.

### 4.12 CSH-PRJ-012 — Source graph dump

- **Símbolos:** NativeSharp.Compiler.Projects.SourceGraphDumper.
- **Entradas:** SourceGraph.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable textual graph; stable textual graph are coherent and observable.
- **Invariante:** dump order matches graph order and preserves owner/path identity.
- **Errores:** none beyond failed graph precondition.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact normalized report.

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

La integración directa es: SelfHost.Source;SelfHost.Parser;SelfHost.Sema;filesystem. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 2 archivos, 79 declaraciones, 12 contratos y 95 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
