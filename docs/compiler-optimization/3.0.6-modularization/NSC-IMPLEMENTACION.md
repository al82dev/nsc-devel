# NSC — IMPLEMENTACIÓN

Progreso absoluto: **642/1227**

Completados: **642**

Pendientes: **585**

Fuente canónica: `NSC-GUIA-DE-IMPLEMENTACION.md` y sus anexos A, B, C, D y F, medidos el 2026-10-19.

Cada casilla representa una unidad medible de trabajo, no un commit. Los IDs permanecen estables aunque las
casillas se agrupen por integración.

Quedan fuera del denominador: cambios a NSC Builder, cambios de CLI/sintaxis/ABI/NSMETA/contratos 3.0.4,
eliminación de recursión y las tareas de rendimiento que requieren autorización funcional independiente.

## Refactor mecánico y reorganización sin cambios funcionales — 642/1225

Política de integración: **10 commits técnicos**, uno por límite de subsistema. Un commit se crea únicamente
cuando su conjunto está completo, revisado y cuenta con la evidencia focal aplicable. Ninguna optimización entra
en estos diez commits.

### Precondición R0 — 0/9 — no genera commit de código

R0 congela el oráculo. Sus evidencias no se utilizan para inflar el número de commits del refactor.

- [ ] NSC-IMPL-0001 — Congelar el árbol R0 de src/, include/ y tests/ con ruta, tamaño y SHA-256 de cada archivo.
- [ ] NSC-IMPL-0002 — Congelar la configuración R0 guardando CMakeCache.txt y las flags efectivas de build.ninja.
- [ ] NSC-IMPL-0003 — Obtener de Alberto el nsc.exe de referencia construido desde el árbol y la configuración R0, y
  registrar su SHA-256.
- [ ] NSC-IMPL-0004 — Incorporar al oráculo R0 el resultado por caso de nsc_component_tests --all y nsc_unit_tests
  ejecutados por Alberto.
- [ ] NSC-IMPL-0005 — Cerrar con Alberto el corpus canónico R0 y guardar la lista completa y determinista de proyectos
  .nsproj.
- [ ] NSC-IMPL-0006 — Capturar para todo el corpus R0 los 18 dumps contractuales y conservarlos byte por byte como
  oráculo.
- [ ] NSC-IMPL-0007 — Capturar en R0 los artefactos O0 -g, O0 -g0 y O2 -g0 con SHA-256 de EXE, LIB, DLL, PDB y NSMETA.
- [ ] NSC-IMPL-0008 — Incorporar al oráculo R0 la cadena self-host nsc1→nsc2→nsc3 y sus SHA-256 obtenidos por Alberto.
- [ ] NSC-IMPL-0009 — Guardar como referencia R0 el .ninja_log de una construcción limpia ejecutada por Alberto.

### Commit REF-01 — Fundamentos, AST, compilación y headers públicos — 79/79

Separa tipos y cuerpos fundamentales y estabiliza los umbrellas públicos que consumen las demás tandas.

- [x] NSC-IMPL-0011 — Extraer el enum VisitState desde src/Compilation/ProjectGraphLoader.cpp:21-25 (5) hacia
  src/Compilation/Internal/VisitState.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0012 — Extraer el struct GraphLoadState desde src/Compilation/ProjectGraphLoader.cpp:27-33 (7) hacia
  src/Compilation/Internal/GraphLoadState.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0013 — Extraer el struct LoadedProject desde src/Compilation/ProjectGraphLoader.cpp:35-40 (6) hacia
  src/Compilation/Internal/LoadedProject.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0014 — Extraer el struct SourceIncludeRule desde src/Compilation/ProjectLoader.cpp:22-26 (5) hacia
  src/Compilation/Internal/SourceIncludeRule.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0015 — Extraer el struct LibraryReferenceDescriptor desde src/Compilation/ProjectLoader.cpp:28-33 (6)
  hacia src/Compilation/Internal/LibraryReferenceDescriptor.h, sin .cpp adicional; conservar texto, comentarios, firmas,
  orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0016 — Extraer el struct ProjectDescriptor desde src/Compilation/ProjectLoader.cpp:35-49 (15) hacia
  src/Compilation/Internal/ProjectDescriptor.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0017 — Extraer el class ProjectLoaderCache desde src/Compilation/ProjectLoader.cpp:51-57 (7) hacia
  src/Compilation/Internal/ProjectLoaderCache.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0194 — Extraer el enum GenericParameterSyntaxKind desde include/nativesharp/AST/AstNode.h:15-21 hacia
  include/nativesharp/AST/GenericParameterSyntaxKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0195 — Extraer el struct GenericParameterSyntax desde include/nativesharp/AST/AstNode.h:23-29 hacia
  include/nativesharp/AST/GenericParameterSyntax.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0196 — Extraer el struct LifetimeParameterSyntax desde include/nativesharp/AST/AstNode.h:33-37 hacia
  include/nativesharp/AST/LifetimeParameterSyntax.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0197 — Extraer el struct LifetimeOutlivesConstraint desde include/nativesharp/AST/AstNode.h:39-44 hacia
  include/nativesharp/AST/LifetimeOutlivesConstraint.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0198 — Extraer el enum FloatingPointModeSyntaxKind desde include/nativesharp/AST/AstNode.h:48-54 hacia
  include/nativesharp/AST/FloatingPointModeSyntaxKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0199 — Extraer el enum FfiUnwindSyntaxKind desde include/nativesharp/AST/AstNode.h:56-61 hacia
  include/nativesharp/AST/FfiUnwindSyntaxKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0200 — Extraer el struct GenericConstraintClause desde include/nativesharp/AST/AstNode.h:70-112 hacia
  include/nativesharp/AST/GenericConstraintClause.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0201 — Conservar el class AstNode como tipo principal de include/nativesharp/AST/AstNode.h:114-252 y
  convertir ese archivo en umbrella de sus tipos extraídos, sin alterar el cierre transitivo. Restricción de pruebas:
  ninguna.
- [x] NSC-IMPL-0202 — Extraer el enum OwnershipQualifierSyntaxKind desde include/nativesharp/AST/TypeSyntax.h:16-21
  hacia include/nativesharp/AST/OwnershipQualifierSyntaxKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0203 — Extraer el enum TypeModifierSyntaxKind desde include/nativesharp/AST/TypeSyntax.h:25-32 hacia
  include/nativesharp/AST/TypeModifierSyntaxKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0204 — Extraer el enum ReferenceQualifierSyntaxKind desde include/nativesharp/AST/TypeSyntax.h:36-41
  hacia include/nativesharp/AST/ReferenceQualifierSyntaxKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0205 — Extraer el enum FunctionTypeCarrierSyntaxKind desde include/nativesharp/AST/TypeSyntax.h:43-47
  hacia include/nativesharp/AST/FunctionTypeCarrierSyntaxKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0206 — Extraer el enum PointerNullabilitySyntaxKind desde include/nativesharp/AST/TypeSyntax.h:49-54
  hacia include/nativesharp/AST/PointerNullabilitySyntaxKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0207 — Extraer el enum FunctionCallingConventionSyntaxKind desde
  include/nativesharp/AST/TypeSyntax.h:56-64 hacia include/nativesharp/AST/FunctionCallingConventionSyntaxKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0208 — Extraer el struct TypeModifierSyntax desde include/nativesharp/AST/TypeSyntax.h:66-79 hacia
  include/nativesharp/AST/TypeModifierSyntax.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0209 — Conservar el class TypeSyntax como tipo principal de include/nativesharp/AST/TypeSyntax.h:81-267 y
  convertir ese archivo en umbrella de sus tipos extraídos, sin alterar el cierre transitivo. Restricción de pruebas:
  ninguna.
- [x] NSC-IMPL-0210 — Extraer el enum OutputFailureKind desde include/nativesharp/Basic/ArtifactTransaction.h:12-21
  hacia include/nativesharp/Basic/OutputFailureKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0211 — Conservar el class ArtifactTransaction como tipo principal de
  include/nativesharp/Basic/ArtifactTransaction.h:31-55 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0212 — Extraer el enum CompilationEventKind desde include/nativesharp/Basic/CompilationEvent.h:9-19 hacia
  include/nativesharp/Basic/CompilationEventKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0213 — Conservar el struct CompilationEvent como tipo principal de
  include/nativesharp/Basic/CompilationEvent.h:21-29 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0214 — Extraer el enum TypeDeclarationKind desde
  include/nativesharp/Compilation/PartialTypeAggregator.h:13-20 hacia
  include/nativesharp/Compilation/TypeDeclarationKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0215 — Extraer el enum DeclaredAccessibility desde
  include/nativesharp/Compilation/PartialTypeAggregator.h:22-27 hacia
  include/nativesharp/Compilation/DeclaredAccessibility.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0216 — Extraer el struct TypePartDescriptor desde
  include/nativesharp/Compilation/PartialTypeAggregator.h:29-42 hacia
  include/nativesharp/Compilation/TypePartDescriptor.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0217 — Extraer el struct MergedTypeDescriptor desde
  include/nativesharp/Compilation/PartialTypeAggregator.h:44-54 hacia
  include/nativesharp/Compilation/MergedTypeDescriptor.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0218 — Conservar el class PartialTypeAggregator como tipo principal de
  include/nativesharp/Compilation/PartialTypeAggregator.h:56-71 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0219 — Extraer el enum ProjectOutputKind desde include/nativesharp/Compilation/Project.h:15-20 hacia
  include/nativesharp/Compilation/ProjectOutputKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0220 — Extraer el struct LibraryReference desde include/nativesharp/Compilation/Project.h:25-30 hacia
  include/nativesharp/Compilation/LibraryReference.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0221 — Conservar el class Project como tipo principal de include/nativesharp/Compilation/Project.h:32-72
  y convertir ese archivo en umbrella de sus tipos extraídos, sin alterar el cierre transitivo. Restricción de pruebas:
  ninguna.
- [x] NSC-IMPL-0222 — Extraer el struct PrecompiledProjectDescriptor desde
  include/nativesharp/Compilation/ProjectGraph.h:12-17 hacia
  include/nativesharp/Compilation/PrecompiledProjectDescriptor.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0223 — Extraer el struct SourceLibraryReferenceReplacement desde
  include/nativesharp/Compilation/ProjectGraph.h:19-24 hacia
  include/nativesharp/Compilation/SourceLibraryReferenceReplacement.h; conservar texto, comentarios, orden de
  declaración y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0224 — Conservar el class ProjectGraph como tipo principal de
  include/nativesharp/Compilation/ProjectGraph.h:26-60 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0225 — Extraer el struct ProjectLoadOptions desde include/nativesharp/Compilation/ProjectLoader.h:16-32
  hacia include/nativesharp/Compilation/ProjectLoadOptions.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0226 — Extraer el struct ProjectBuildSettings desde include/nativesharp/Compilation/ProjectLoader.h:37-45
  hacia include/nativesharp/Compilation/ProjectBuildSettings.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0227 — Extraer el struct ProjectLoaderStatistics desde
  include/nativesharp/Compilation/ProjectLoader.h:47-51 hacia include/nativesharp/Compilation/ProjectLoaderStatistics.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0228 — Conservar el class ProjectLoader como tipo principal de
  include/nativesharp/Compilation/ProjectLoader.h:55-72 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0229 — Extraer el struct ProjectSource desde include/nativesharp/Compilation/ProjectSourceSet.h:10-15
  hacia include/nativesharp/Compilation/ProjectSource.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0230 — Conservar el class ProjectSourceSet como tipo principal de
  include/nativesharp/Compilation/ProjectSourceSet.h:17-36 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0240 — Extraer el struct SimpleXmlElement desde include/nativesharp/Compilation/SimpleXmlParser.h:10-16
  hacia include/nativesharp/Compilation/SimpleXmlElement.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0241 — Conservar el class SimpleXmlParser como tipo principal de
  include/nativesharp/Compilation/SimpleXmlParser.h:18-42 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0242 — Extraer el struct TypeNameSegment desde include/nativesharp/Compilation/TypeIdentity.h:10-16 hacia
  include/nativesharp/Compilation/TypeNameSegment.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0243 — Conservar el class TypeIdentity como tipo principal de
  include/nativesharp/Compilation/TypeIdentity.h:94-128 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0244 — Extraer el struct DiagnosticLabel desde include/nativesharp/Diagnostics/Diagnostic.h:14-18 hacia
  include/nativesharp/Diagnostics/DiagnosticLabel.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0245 — Conservar el class Diagnostic como tipo principal de
  include/nativesharp/Diagnostics/Diagnostic.h:20-48 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0790 — Extraer el struct MirEffectInfo desde include/nativesharp/Passes/MirEffectClassifier.h:10-17 hacia
  include/nativesharp/Passes/MirEffectInfo.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0791 — Conservar el class MirEffectClassifier como tipo principal de
  include/nativesharp/Passes/MirEffectClassifier.h:19-24 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0814 — Extraer el enum Architecture desde include/nativesharp/Target/TargetInfo.h:7-11 hacia
  include/nativesharp/Target/Architecture.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0815 — Extraer el enum OperatingSystem desde include/nativesharp/Target/TargetInfo.h:13-16 hacia
  include/nativesharp/Target/OperatingSystem.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0816 — Extraer el enum ObjectFormat desde include/nativesharp/Target/TargetInfo.h:18-21 hacia
  include/nativesharp/Target/ObjectFormat.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0817 — Extraer el enum CpuFeatureLevel desde include/nativesharp/Target/TargetInfo.h:23-29 hacia
  include/nativesharp/Target/CpuFeatureLevel.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0818 — Conservar el class TargetInfo como tipo principal de include/nativesharp/Target/TargetInfo.h:31-70
  y convertir ese archivo en umbrella de sus tipos extraídos, sin alterar el cierre transitivo. Restricción de pruebas:
  ninguna.
- [x] NSC-IMPL-0930 — Descomponer ProjectLoader::load en src/Compilation/ProjectLoader.cpp:782 (194 líneas ejecutables;
  296 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-0954 — Descomponer tokenKindName en src/Lexer/TokenKind.cpp:5 (156 líneas ejecutables; 162 totales) hasta
  que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [x] NSC-IMPL-0999 — Descomponer parsePropertyGroup en src/Compilation/ProjectLoader.cpp:230 (114 líneas ejecutables;
  197 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1000 — Descomponer ArtifactTransaction::commit en src/Basic/ArtifactTransaction.cpp:272 (114 líneas
  ejecutables; 163 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1006 — Descomponer astNodeKindName en src/AST/AstNodeKind.cpp:5 (112 líneas ejecutables; 118 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [x] NSC-IMPL-1008 — Descomponer ProjectGraph::addPrecompiledProjects en src/Compilation/ProjectGraph.cpp:226 (109
  líneas ejecutables; 153 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1030 — Descomponer keywordKind en src/Lexer/Lexer.cpp:53 (100 líneas ejecutables; 105 totales) hasta que
  cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1034 — Descomponer Lexer::lexNumber en src/Lexer/Lexer.cpp:455 (97 líneas ejecutables; 158 totales) hasta
  que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [x] NSC-IMPL-1049 — Descomponer Lexer::lexOperatorOrPunctuation en src/Lexer/Lexer.cpp:1028 (90 líneas ejecutables;
  120 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1052 — Descomponer foldBinary en src/Passes/ConstantFolder.cpp:418 (89 líneas ejecutables; 145 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [x] NSC-IMPL-1057 — Descomponer PassManager::run en src/Passes/PassManager.cpp:52 (87 líneas ejecutables; 139 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [x] NSC-IMPL-1077 — Descomponer ProjectGraph::replaceLibraryReferencesWithSourceProjects en
  src/Compilation/ProjectGraph.cpp:105 (83 líneas ejecutables; 120 totales) hasta que cada función quede en ≤60 líneas
  ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1082 — Descomponer ConstantFolder::fold en src/Passes/ConstantFolder.cpp:573 (81 líneas ejecutables; 127
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1094 — Descomponer parseItemGroup en src/Compilation/ProjectLoader.cpp:473 (79 líneas ejecutables; 132
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1095 — Descomponer MirEffectClassifier::classify en src/Passes/MirEffectClassifier.cpp:46 (79 líneas
  ejecutables; 97 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1102 — Descomponer diagnosticIdName en include/nativesharp/Diagnostics/DiagnosticId.h:106 (78 líneas
  ejecutables; 83 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1103 — Descomponer sha256 en src/Basic/Sha256.cpp:15 (77 líneas ejecutables; 90 totales) hasta que cada
  función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1109 — Descomponer AstDumper::dumpNode en src/AST/AstDumper.cpp:15 (77 líneas ejecutables; 134 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [x] NSC-IMPL-1110 — Descomponer AstDumper::dumpType en src/AST/AstDumper.cpp:150 (77 líneas ejecutables; 113 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [x] NSC-IMPL-1174 — Descomponer Lexer::lexRawString en src/Lexer/Lexer.cpp:767 (63 líneas ejecutables; 101 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [x] NSC-IMPL-1185 — Descomponer PhiSimplificationPass::run en src/Passes/PhiSimplificationPass.cpp:89 (62 líneas
  ejecutables; 97 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1186 — Descomponer SimplifyControlFlowPass::run en src/Passes/SimplifyControlFlowPass.cpp:178 (62 líneas
  ejecutables; 113 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
### Commit REF-02 — Lenguaje, metadata, interoperabilidad y contratos ABI — 314/314

Ordena las superficies de lenguaje, metadata e interoperabilidad sin cambiar formatos, ordinales ni ABI.

- [x] NSC-IMPL-0083 — Extraer el struct HostContext desde src/Interop/CHeaderGenerator.cpp:40-48 (9) hacia
  src/Interop/Internal/HostContext.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0084 — Extraer el struct Requirement desde src/Interop/LibraryCapabilityAudit.cpp:10-15 (6) hacia
  src/Interop/Internal/Requirement.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0085 — Extraer el class ByteWriter desde src/Interop/LibraryMetadata.cpp:70-112 (43) hacia
  src/Interop/Internal/ByteWriter.h, y mover sus cuerpos a src/Interop/Internal/ByteWriter.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0086 — Extraer el class ByteReader desde src/Interop/LibraryMetadata.cpp:114-185 (72) hacia
  src/Interop/Internal/ByteReader.h, y mover sus cuerpos a src/Interop/Internal/ByteReader.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0087 — Extraer el struct TemplateSourceReplacement desde src/Interop/LibraryMetadata.cpp:577-582 (6)
  hacia src/Interop/Internal/TemplateSourceReplacement.h, sin .cpp adicional; conservar texto, comentarios, firmas,
  orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0104 — Extraer el struct ParsedBuiltinHole desde src/Language/ExpressionErgonomics.cpp:205-213 (9) hacia
  src/Language/Internal/ParsedBuiltinHole.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0105 — Extraer el class BuiltinStaticInterpolationHandler desde
  src/Language/ExpressionErgonomics.cpp:273-328 (56) hacia src/Language/Internal/BuiltinStaticInterpolationHandler.h, y
  mover sus cuerpos a src/Language/Internal/BuiltinStaticInterpolationHandler.cpp; conservar texto, comentarios, firmas,
  orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0131 — Mover fuera de línea función libre checkedLayoutAdd de src/Language/CheckedLayoutArithmetic.h:9
  (10 líneas) a un único .cpp, dejando la declaración intacta y conservando exactamente el cuerpo.
- [x] NSC-IMPL-0132 — Mover fuera de línea función libre checkedLayoutAlignUp de
  src/Language/CheckedLayoutArithmetic.h:20 (16 líneas) a un único .cpp, dejando la declaración intacta y conservando
  exactamente el cuerpo.
- [x] NSC-IMPL-0142 — Mover fuera de línea función libre futureTypeKey de
  include/nativesharp/Language/CoroutineSemantics.h:27 (6 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0143 — Mover fuera de línea función libre futureTypeKey de
  include/nativesharp/Language/CoroutineSemantics.h:36 (10 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0144 — Mover fuera de línea función libre futureInnerTypeKey de
  include/nativesharp/Language/CoroutineSemantics.h:47 (11 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0145 — Mover fuera de línea función libre futureErrorTypeKey de
  include/nativesharp/Language/CoroutineSemantics.h:60 (9 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0146 — Mover fuera de línea función libre iteratorTypeKey de
  include/nativesharp/Language/IterationSemantics.h:26 (6 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0147 — Mover fuera de línea función libre iteratorInnerTypeKey de
  include/nativesharp/Language/IterationSemantics.h:33 (8 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0148 — Mover fuera de línea función libre iteratorTypeKey de
  include/nativesharp/Language/IterationSemantics.h:43 (11 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0149 — Mover fuera de línea función libre iteratorErrorTypeKey de
  include/nativesharp/Language/IterationSemantics.h:55 (9 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0150 — Mover fuera de línea función libre asyncIteratorTypeKey de
  include/nativesharp/Language/IterationSemantics.h:67 (11 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0151 — Mover fuera de línea función libre asyncIteratorInnerTypeKey de
  include/nativesharp/Language/IterationSemantics.h:79 (8 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0152 — Mover fuera de línea función libre asyncIteratorErrorTypeKey de
  include/nativesharp/Language/IterationSemantics.h:88 (9 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0231 — Extraer el struct SdkHeaderMapping desde include/nativesharp/Compilation/SdkManifest.h:14-18 hacia
  include/nativesharp/Compilation/SdkHeaderMapping.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0232 — Extraer el struct SdkTypeMapping desde include/nativesharp/Compilation/SdkManifest.h:20-24 hacia
  include/nativesharp/Compilation/SdkTypeMapping.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0233 — Extraer el struct SdkFieldMapping desde include/nativesharp/Compilation/SdkManifest.h:26-31 hacia
  include/nativesharp/Compilation/SdkFieldMapping.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0234 — Extraer el struct SdkDefineMapping desde include/nativesharp/Compilation/SdkManifest.h:33-37 hacia
  include/nativesharp/Compilation/SdkDefineMapping.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0235 — Extraer el struct SdkTargetVariant desde include/nativesharp/Compilation/SdkManifest.h:39-44 hacia
  include/nativesharp/Compilation/SdkTargetVariant.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0236 — Extraer el struct SdkLibraryDependency desde include/nativesharp/Compilation/SdkManifest.h:46-50
  hacia include/nativesharp/Compilation/SdkLibraryDependency.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0237 — Conservar el struct SdkManifest como tipo principal de
  include/nativesharp/Compilation/SdkManifest.h:52-63 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0238 — Extraer el class SdkTargetSelector desde include/nativesharp/Compilation/SdkManifest.h:65-73 hacia
  include/nativesharp/Compilation/SdkTargetSelector.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0239 — Extraer el class SdkManifestLoader desde include/nativesharp/Compilation/SdkManifest.h:75-81 hacia
  include/nativesharp/Compilation/SdkManifestLoader.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0470 — Extraer el enum LibraryApiChangeImpact desde include/nativesharp/Interop/LibraryApiDiff.h:10-15
  hacia include/nativesharp/Interop/LibraryApiChangeImpact.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0471 — Extraer el struct LibraryApiChange desde include/nativesharp/Interop/LibraryApiDiff.h:17-23 hacia
  include/nativesharp/Interop/LibraryApiChange.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0472 — Extraer el struct LibraryApiDiffReport desde include/nativesharp/Interop/LibraryApiDiff.h:25-34
  hacia include/nativesharp/Interop/LibraryApiDiffReport.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0473 — Extraer el class LibraryApiDiffer desde include/nativesharp/Interop/LibraryApiDiff.h:36-51 hacia
  include/nativesharp/Interop/LibraryApiDiffer.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0474 — Extraer el enum LibraryMetadataTypeKind desde
  include/nativesharp/Interop/LibraryMetadata.h:193-200 hacia include/nativesharp/Interop/LibraryMetadataTypeKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0475 — Extraer el enum LibraryMetadataOwnershipKind desde
  include/nativesharp/Interop/LibraryMetadata.h:202-207 hacia
  include/nativesharp/Interop/LibraryMetadataOwnershipKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0476 — Extraer el enum LibraryMetadataDefaultValueKind desde
  include/nativesharp/Interop/LibraryMetadata.h:209-218 hacia
  include/nativesharp/Interop/LibraryMetadataDefaultValueKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0477 — Extraer el enum LibraryMetadataParameterPassKind desde
  include/nativesharp/Interop/LibraryMetadata.h:220-226 hacia
  include/nativesharp/Interop/LibraryMetadataParameterPassKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0478 — Extraer el struct LibraryMetadataField desde include/nativesharp/Interop/LibraryMetadata.h:228-240
  hacia include/nativesharp/Interop/LibraryMetadataField.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0479 — Extraer el struct LibraryMetadataVariantCase desde
  include/nativesharp/Interop/LibraryMetadata.h:242-247 hacia include/nativesharp/Interop/LibraryMetadataVariantCase.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0480 — Extraer el enum LibraryMetadataAssociatedItemKind desde
  include/nativesharp/Interop/LibraryMetadata.h:249-253 hacia
  include/nativesharp/Interop/LibraryMetadataAssociatedItemKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0481 — Extraer el struct LibraryMetadataAssociatedItem desde
  include/nativesharp/Interop/LibraryMetadata.h:255-265 hacia
  include/nativesharp/Interop/LibraryMetadataAssociatedItem.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0482 — Extraer el struct LibraryMetadataType desde include/nativesharp/Interop/LibraryMetadata.h:267-329
  hacia include/nativesharp/Interop/LibraryMetadataType.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0483 — Extraer el struct LibraryMetadataParameter desde
  include/nativesharp/Interop/LibraryMetadata.h:331-355 hacia include/nativesharp/Interop/LibraryMetadataParameter.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0484 — Extraer el enum LibraryMetadataEscapeClass desde
  include/nativesharp/Interop/LibraryMetadata.h:357-364 hacia include/nativesharp/Interop/LibraryMetadataEscapeClass.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0485 — Extraer el struct LibraryMetadataMember desde
  include/nativesharp/Interop/LibraryMetadata.h:366-399 hacia include/nativesharp/Interop/LibraryMetadataMember.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0486 — Extraer el struct LibraryMetadataNamespaceFunction desde
  include/nativesharp/Interop/LibraryMetadata.h:401-417 hacia
  include/nativesharp/Interop/LibraryMetadataNamespaceFunction.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0487 — Extraer el enum LibraryMetadataNamespaceStateInitializerKind desde
  include/nativesharp/Interop/LibraryMetadata.h:419-427 hacia
  include/nativesharp/Interop/LibraryMetadataNamespaceStateInitializerKind.h; conservar texto, comentarios, orden de
  declaración y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0488 — Extraer el struct LibraryMetadataNamespaceState desde
  include/nativesharp/Interop/LibraryMetadata.h:429-442 hacia
  include/nativesharp/Interop/LibraryMetadataNamespaceState.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0489 — Extraer el struct LibraryMetadataGenericTemplate desde
  include/nativesharp/Interop/LibraryMetadata.h:443-475 hacia
  include/nativesharp/Interop/LibraryMetadataGenericTemplate.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0490 — Extraer el struct LibraryMetadataGenericSpecialization desde
  include/nativesharp/Interop/LibraryMetadata.h:491-498 hacia
  include/nativesharp/Interop/LibraryMetadataGenericSpecialization.h; conservar texto, comentarios, orden de declaración
  y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0491 — Extraer el struct LibraryMetadataDispatchInterface desde
  include/nativesharp/Interop/LibraryMetadata.h:500-508 hacia
  include/nativesharp/Interop/LibraryMetadataDispatchInterface.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0492 — Extraer el struct LibraryMetadataDispatch desde
  include/nativesharp/Interop/LibraryMetadata.h:510-524 hacia include/nativesharp/Interop/LibraryMetadataDispatch.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0493 — Extraer el struct LibraryMetadataExport desde
  include/nativesharp/Interop/LibraryMetadata.h:526-533 hacia include/nativesharp/Interop/LibraryMetadataExport.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0494 — Extraer el struct LibraryMetadataBinaryDependency desde
  include/nativesharp/Interop/LibraryMetadata.h:535-542 hacia
  include/nativesharp/Interop/LibraryMetadataBinaryDependency.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0495 — Extraer el struct LibraryMetadataPlatformImport desde
  include/nativesharp/Interop/LibraryMetadata.h:544-548 hacia
  include/nativesharp/Interop/LibraryMetadataPlatformImport.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0496 — Extraer el struct LibraryMetadataTraitImpl desde
  include/nativesharp/Interop/LibraryMetadata.h:553-558 hacia include/nativesharp/Interop/LibraryMetadataTraitImpl.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0497 — Conservar el struct LibraryMetadata como tipo principal de
  include/nativesharp/Interop/LibraryMetadata.h:560-609 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0498 — Extraer el class LibraryMetadataCodec desde include/nativesharp/Interop/LibraryMetadata.h:616-655
  hacia include/nativesharp/Interop/LibraryMetadataCodec.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0530 — Extraer el enum Representation desde include/nativesharp/Language/AbiSemantics.h:11-11 hacia
  include/nativesharp/Language/Representation.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0531 — Extraer el struct RepresentationContract desde include/nativesharp/Language/AbiSemantics.h:12-18
  hacia include/nativesharp/Language/RepresentationContract.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0532 — Extraer el struct BitFieldContract desde include/nativesharp/Language/AbiSemantics.h:21-28 hacia
  include/nativesharp/Language/BitFieldContract.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0533 — Extraer el struct PointerContract desde include/nativesharp/Language/AbiSemantics.h:31-38 hacia
  include/nativesharp/Language/PointerContract.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0534 — Extraer el enum AbiCallingConvention desde include/nativesharp/Language/AbiSemantics.h:44-44 hacia
  include/nativesharp/Language/AbiCallingConvention.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0535 — Extraer el enum UnsafeOperation desde include/nativesharp/Language/AbiSemantics.h:47-47 hacia
  include/nativesharp/Language/UnsafeOperation.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0536 — Extraer el enum FfiOwnership desde include/nativesharp/Language/AbiSemantics.h:51-51 hacia
  include/nativesharp/Language/FfiOwnership.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0537 — Extraer el struct FfiParameterContract desde include/nativesharp/Language/AbiSemantics.h:52-59
  hacia include/nativesharp/Language/FfiParameterContract.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0538 — Extraer el struct SimdContract desde include/nativesharp/Language/AbiSemantics.h:66-66 hacia
  include/nativesharp/Language/SimdContract.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0539 — Extraer el enum SimdLaneKind desde include/nativesharp/Language/AbiSemantics.h:68-68 hacia
  include/nativesharp/Language/SimdLaneKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0540 — Extraer el struct PortableSimdType desde include/nativesharp/Language/AbiSemantics.h:69-76 hacia
  include/nativesharp/Language/PortableSimdType.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0541 — Extraer el struct TupleElement desde include/nativesharp/Language/AggregateSemantics.h:13-17 hacia
  include/nativesharp/Language/TupleElement.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0542 — Extraer el struct AggregateFieldLayout desde
  include/nativesharp/Language/AggregateSemantics.h:25-29 hacia include/nativesharp/Language/AggregateFieldLayout.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0543 — Extraer el struct AggregateLayout desde include/nativesharp/Language/AggregateSemantics.h:31-36
  hacia include/nativesharp/Language/AggregateLayout.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0544 — Extraer el enum WindowsX64AggregateReturn desde
  include/nativesharp/Language/AggregateSemantics.h:41-45 hacia
  include/nativesharp/Language/WindowsX64AggregateReturn.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0545 — Extraer el enum DeconstructionBindingMode desde
  include/nativesharp/Language/AggregateSemantics.h:49-55 hacia
  include/nativesharp/Language/DeconstructionBindingMode.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0546 — Extraer el struct DeconstructionBinding desde
  include/nativesharp/Language/AggregateSemantics.h:57-61 hacia include/nativesharp/Language/DeconstructionBinding.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0547 — Extraer el enum RecordDerive desde include/nativesharp/Language/AggregateSemantics.h:67-73 hacia
  include/nativesharp/Language/RecordDerive.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0548 — Extraer el class RecordDeriveSet desde include/nativesharp/Language/AggregateSemantics.h:75-93
  hacia include/nativesharp/Language/RecordDeriveSet.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0549 — Extraer el struct PromotedField desde include/nativesharp/Language/AggregateSemantics.h:95-101
  hacia include/nativesharp/Language/PromotedField.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0550 — Extraer el enum FunctionCarrier desde include/nativesharp/Language/CallableSemantics.h:12-12 hacia
  include/nativesharp/Language/FunctionCarrier.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0551 — Extraer el enum CallingConvention desde include/nativesharp/Language/CallableSemantics.h:13-13
  hacia include/nativesharp/Language/CallingConvention.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0552 — Extraer el struct FunctionType desde include/nativesharp/Language/CallableSemantics.h:15-28 hacia
  include/nativesharp/Language/FunctionType.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0553 — Extraer el struct AnonymousLambdaType desde include/nativesharp/Language/CallableSemantics.h:42-47
  hacia include/nativesharp/Language/AnonymousLambdaType.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0554 — Extraer el enum CaptureMode desde include/nativesharp/Language/CallableSemantics.h:54-54 hacia
  include/nativesharp/Language/CaptureMode.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0555 — Extraer el struct Capture desde include/nativesharp/Language/CallableSemantics.h:55-62 hacia
  include/nativesharp/Language/Capture.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0556 — Extraer el struct ClosureLayout desde include/nativesharp/Language/CallableSemantics.h:64-64 hacia
  include/nativesharp/Language/ClosureLayout.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0557 — Extraer el enum CallableTrait desde include/nativesharp/Language/CallableSemantics.h:68-68 hacia
  include/nativesharp/Language/CallableTrait.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0558 — Extraer el enum MethodValueKind desde include/nativesharp/Language/CallableSemantics.h:73-73 hacia
  include/nativesharp/Language/MethodValueKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0559 — Extraer el struct BoundMethodLayout desde include/nativesharp/Language/CallableSemantics.h:74-74
  hacia include/nativesharp/Language/BoundMethodLayout.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0560 — Extraer el struct BinaryModuleContract desde
  include/nativesharp/Language/CompatibilitySemantics.h:11-17 hacia include/nativesharp/Language/BinaryModuleContract.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0561 — Extraer el enum ModuleCompatibility desde
  include/nativesharp/Language/CompatibilitySemantics.h:18-18 hacia include/nativesharp/Language/ModuleCompatibility.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0562 — Extraer el struct CompilationUnitContract desde
  include/nativesharp/Language/CompatibilitySemantics.h:23-29 hacia
  include/nativesharp/Language/CompilationUnitContract.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0563 — Extraer el enum MigrationIssue desde include/nativesharp/Language/CompatibilitySemantics.h:37-46
  hacia include/nativesharp/Language/MigrationIssue.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0564 — Extraer el struct MigrationFix desde include/nativesharp/Language/CompatibilitySemantics.h:49-54
  hacia include/nativesharp/Language/MigrationFix.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0565 — Extraer el enum SourceCompatibility desde
  include/nativesharp/Language/CompatibilitySemantics.h:57-57 hacia include/nativesharp/Language/SourceCompatibility.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0566 — Extraer el enum AbiCompatibility desde include/nativesharp/Language/CompatibilitySemantics.h:58-58
  hacia include/nativesharp/Language/AbiCompatibility.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0567 — Extraer el struct CompatibilityClassification desde
  include/nativesharp/Language/CompatibilitySemantics.h:59-64 hacia
  include/nativesharp/Language/CompatibilityClassification.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0568 — Extraer el struct MigrationChapter desde
  include/nativesharp/Language/CompatibilitySemantics.h:71-78 hacia include/nativesharp/Language/MigrationChapter.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0569 — Extraer el enum FutureStart desde include/nativesharp/Language/CoroutineSemantics.h:19-19 hacia
  include/nativesharp/Language/FutureStart.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0570 — Extraer el enum FrameStorage desde include/nativesharp/Language/CoroutineSemantics.h:20-20 hacia
  include/nativesharp/Language/FrameStorage.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0571 — Extraer el struct AwaitSuspension desde include/nativesharp/Language/CoroutineSemantics.h:70-77
  hacia include/nativesharp/Language/AwaitSuspension.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0572 — Extraer el struct AsyncFunctionPlan desde include/nativesharp/Language/CoroutineSemantics.h:79-88
  hacia include/nativesharp/Language/AsyncFunctionPlan.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0573 — Extraer el enum PollState desde include/nativesharp/Language/CoroutineSemantics.h:101-101 hacia
  include/nativesharp/Language/PollState.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0574 — Extraer el enum FutureCompletion desde include/nativesharp/Language/CoroutineSemantics.h:102-102
  hacia include/nativesharp/Language/FutureCompletion.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0575 — Extraer el enum PollOutcome desde include/nativesharp/Language/CoroutineSemantics.h:103-103 hacia
  include/nativesharp/Language/PollOutcome.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0576 — Extraer el struct FutureProtocol desde include/nativesharp/Language/CoroutineSemantics.h:105-117
  hacia include/nativesharp/Language/FutureProtocol.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0577 — Extraer el struct AwaitContext desde include/nativesharp/Language/CoroutineSemantics.h:121-126
  hacia include/nativesharp/Language/AwaitContext.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0578 — Extraer el class FutureState desde include/nativesharp/Language/CoroutineSemantics.h:130-142 hacia
  include/nativesharp/Language/FutureState.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0579 — Extraer el enum CancellationReason desde include/nativesharp/Language/CoroutineSemantics.h:148-148
  hacia include/nativesharp/Language/CancellationReason.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0580 — Extraer el enum AsyncCleanupKind desde include/nativesharp/Language/CoroutineSemantics.h:149-149
  hacia include/nativesharp/Language/AsyncCleanupKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0581 — Extraer el struct ActiveAsyncDefer desde include/nativesharp/Language/CoroutineSemantics.h:151-155
  hacia include/nativesharp/Language/ActiveAsyncDefer.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0582 — Extraer el struct AsyncCleanupAction desde
  include/nativesharp/Language/CoroutineSemantics.h:157-162 hacia include/nativesharp/Language/AsyncCleanupAction.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0583 — Extraer el struct AsyncCancellationPlan desde
  include/nativesharp/Language/CoroutineSemantics.h:164-171 hacia include/nativesharp/Language/AsyncCancellationPlan.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0584 — Extraer el enum AsyncGeneratorState desde
  include/nativesharp/Language/CoroutineSemantics.h:181-181 hacia include/nativesharp/Language/AsyncGeneratorState.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0585 — Extraer el enum AsyncGeneratorSuspensionKind desde
  include/nativesharp/Language/CoroutineSemantics.h:186-186 hacia
  include/nativesharp/Language/AsyncGeneratorSuspensionKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0586 — Extraer el struct AsyncGeneratorSuspension desde
  include/nativesharp/Language/CoroutineSemantics.h:188-196 hacia
  include/nativesharp/Language/AsyncGeneratorSuspension.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0587 — Extraer el struct AsyncGeneratorPlan desde
  include/nativesharp/Language/CoroutineSemantics.h:198-207 hacia include/nativesharp/Language/AsyncGeneratorPlan.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
- [x] NSC-IMPL-0588 — Extraer el enum AsyncGeneratorPollOutcome desde
  include/nativesharp/Language/CoroutineSemantics.h:220-220 hacia
  include/nativesharp/Language/AsyncGeneratorPollOutcome.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0589 — Extraer el class AsyncGeneratorProtocolState desde
  include/nativesharp/Language/CoroutineSemantics.h:222-236 hacia
  include/nativesharp/Language/AsyncGeneratorProtocolState.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0590 — Extraer el struct ThreadSafetyFacts desde
  include/nativesharp/Language/CoroutineSemantics.h:238-245 hacia include/nativesharp/Language/ThreadSafetyFacts.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0591 — Extraer el enum ThreadSafetyMemberKind desde
  include/nativesharp/Language/CoroutineSemantics.h:249-257 hacia include/nativesharp/Language/ThreadSafetyMemberKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0592 — Extraer el struct ThreadSafetyMemberFacts desde
  include/nativesharp/Language/CoroutineSemantics.h:259-267 hacia
  include/nativesharp/Language/ThreadSafetyMemberFacts.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0593 — Extraer el struct ThreadSafetyTypeFacts desde
  include/nativesharp/Language/CoroutineSemantics.h:269-279 hacia include/nativesharp/Language/ThreadSafetyTypeFacts.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0594 — Extraer el struct ThreadSafetyDecision desde
  include/nativesharp/Language/CoroutineSemantics.h:281-288 hacia include/nativesharp/Language/ThreadSafetyDecision.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0595 — Extraer el struct ThreadSafetyMetadata desde
  include/nativesharp/Language/CoroutineSemantics.h:293-302 hacia include/nativesharp/Language/ThreadSafetyMetadata.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0596 — Extraer el struct TaskExecutorProtocol desde
  include/nativesharp/Language/CoroutineSemantics.h:313-321 hacia include/nativesharp/Language/TaskExecutorProtocol.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0597 — Extraer el struct ScopedTask desde include/nativesharp/Language/CoroutineSemantics.h:323-329 hacia
  include/nativesharp/Language/ScopedTask.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0598 — Extraer el struct TaskScopePlan desde include/nativesharp/Language/CoroutineSemantics.h:331-338
  hacia include/nativesharp/Language/TaskScopePlan.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0599 — Extraer el enum TaskScopeExitReason desde
  include/nativesharp/Language/CoroutineSemantics.h:347-347 hacia include/nativesharp/Language/TaskScopeExitReason.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0600 — Extraer el enum TaskScopeActionKind desde
  include/nativesharp/Language/CoroutineSemantics.h:348-348 hacia include/nativesharp/Language/TaskScopeActionKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0601 — Extraer el struct TaskScopeAction desde include/nativesharp/Language/CoroutineSemantics.h:350-354
  hacia include/nativesharp/Language/TaskScopeAction.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0602 — Extraer el enum ModuleUnloadPolicy desde include/nativesharp/Language/CrossModuleLifetime.h:15-19
  hacia include/nativesharp/Language/ModuleUnloadPolicy.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: vista #define private public (ver §4.3).
- [x] NSC-IMPL-0603 — Extraer el struct CrossModuleLifetimeContract desde
  include/nativesharp/Language/CrossModuleLifetime.h:21-32 hacia
  include/nativesharp/Language/CrossModuleLifetimeContract.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: vista #define private public (ver §4.3).
- [x] NSC-IMPL-0604 — Extraer el class ModuleLifetimeRegistry desde
  include/nativesharp/Language/CrossModuleLifetime.h:44-61 hacia include/nativesharp/Language/ModuleLifetimeRegistry.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: vista #define private public (ver §4.3).
- [x] NSC-IMPL-0605 — Extraer el struct ResultProtocol desde include/nativesharp/Language/ErrorSemantics.h:11-18 hacia
  include/nativesharp/Language/ResultProtocol.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0606 — Extraer el enum ThrowAbi desde include/nativesharp/Language/ErrorSemantics.h:22-22 hacia
  include/nativesharp/Language/ThrowAbi.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0607 — Extraer el struct ThrowContract desde include/nativesharp/Language/ErrorSemantics.h:23-29 hacia
  include/nativesharp/Language/ThrowContract.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0608 — Extraer el enum PanicProfile desde include/nativesharp/Language/ErrorSemantics.h:40-40 hacia
  include/nativesharp/Language/PanicProfile.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0609 — Extraer el struct PanicContract desde include/nativesharp/Language/ErrorSemantics.h:43-47 hacia
  include/nativesharp/Language/PanicContract.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0610 — Extraer el enum AssertionKind desde include/nativesharp/Language/ErrorSemantics.h:50-50 hacia
  include/nativesharp/Language/AssertionKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0611 — Extraer el enum ExpressionFeature desde include/nativesharp/Language/ExpressionErgonomics.h:12-21
  hacia include/nativesharp/Language/ExpressionFeature.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0612 — Extraer el struct ExpressionLoweringContract desde
  include/nativesharp/Language/ExpressionErgonomics.h:23-31 hacia
  include/nativesharp/Language/ExpressionLoweringContract.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0613 — Extraer el enum NullAwareOperandKind desde
  include/nativesharp/Language/ExpressionErgonomics.h:38-44 hacia include/nativesharp/Language/NullAwareOperandKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0614 — Extraer el struct RangeEndpoint desde include/nativesharp/Language/ExpressionErgonomics.h:49-53
  hacia include/nativesharp/Language/RangeEndpoint.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0615 — Extraer el struct ResolvedRange desde include/nativesharp/Language/ExpressionErgonomics.h:55-59
  hacia include/nativesharp/Language/ResolvedRange.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0616 — Extraer el enum InterpolationPartKind desde
  include/nativesharp/Language/ExpressionErgonomics.h:68-72 hacia include/nativesharp/Language/InterpolationPartKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0617 — Extraer el struct InterpolationPart desde
  include/nativesharp/Language/ExpressionErgonomics.h:74-84 hacia include/nativesharp/Language/InterpolationPart.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0618 — Extraer el class StaticInterpolationHandler desde
  include/nativesharp/Language/ExpressionErgonomics.h:95-101 hacia
  include/nativesharp/Language/StaticInterpolationHandler.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0619 — Extraer el struct ConstraintSet desde include/nativesharp/Language/GenericSemantics.h:12-16 hacia
  include/nativesharp/Language/ConstraintSet.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0620 — Extraer el enum ConstGenericKind desde include/nativesharp/Language/GenericSemantics.h:23-23 hacia
  include/nativesharp/Language/ConstGenericKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0621 — Extraer el struct ConstGenericValue desde include/nativesharp/Language/GenericSemantics.h:25-30
  hacia include/nativesharp/Language/ConstGenericValue.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0622 — Extraer el enum AliasKind desde include/nativesharp/Language/GenericSemantics.h:55-55 hacia
  include/nativesharp/Language/AliasKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0623 — Extraer el class InstantiationBudget desde include/nativesharp/Language/GenericSemantics.h:66-87
  hacia include/nativesharp/Language/InstantiationBudget.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0624 — Extraer el enum FeatureArea desde include/nativesharp/Language/IntegrationQuality.h:13-28 hacia
  include/nativesharp/Language/FeatureArea.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0625 — Extraer el enum InteractionRisk desde include/nativesharp/Language/IntegrationQuality.h:29-29
  hacia include/nativesharp/Language/InteractionRisk.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0626 — Extraer el struct InteractionCase desde include/nativesharp/Language/IntegrationQuality.h:31-40
  hacia include/nativesharp/Language/InteractionCase.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0627 — Extraer el struct WholeProgramWitness desde
  include/nativesharp/Language/IntegrationQuality.h:51-66 hacia include/nativesharp/Language/WholeProgramWitness.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0628 — Extraer el struct PerformanceBudget desde include/nativesharp/Language/IntegrationQuality.h:73-82
  hacia include/nativesharp/Language/PerformanceBudget.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0629 — Extraer el enum PerformanceCorpus desde include/nativesharp/Language/IntegrationQuality.h:85-93
  hacia include/nativesharp/Language/PerformanceCorpus.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0630 — Extraer el struct PerformanceBudgetProfile desde
  include/nativesharp/Language/IntegrationQuality.h:95-103 hacia
  include/nativesharp/Language/PerformanceBudgetProfile.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0631 — Extraer el struct PerformanceAssessment desde
  include/nativesharp/Language/IntegrationQuality.h:105-110 hacia include/nativesharp/Language/PerformanceAssessment.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0632 — Extraer el enum DiagnosticArea desde include/nativesharp/Language/IntegrationQuality.h:118-118
  hacia include/nativesharp/Language/DiagnosticArea.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0633 — Extraer el struct DiagnosticSpan desde include/nativesharp/Language/IntegrationQuality.h:120-127
  hacia include/nativesharp/Language/DiagnosticSpan.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0634 — Extraer el struct DiagnosticFixIt desde include/nativesharp/Language/IntegrationQuality.h:129-134
  hacia include/nativesharp/Language/DiagnosticFixIt.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0635 — Extraer el struct DiagnosticQuality desde
  include/nativesharp/Language/IntegrationQuality.h:136-148 hacia include/nativesharp/Language/DiagnosticQuality.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0636 — Extraer el enum DebugOptimization desde include/nativesharp/Language/IntegrationQuality.h:164-164
  hacia include/nativesharp/Language/DebugOptimization.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0637 — Extraer el enum DebugAvailability desde include/nativesharp/Language/IntegrationQuality.h:165-165
  hacia include/nativesharp/Language/DebugAvailability.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0638 — Extraer el enum DebugEntityKind desde include/nativesharp/Language/IntegrationQuality.h:166-176
  hacia include/nativesharp/Language/DebugEntityKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0639 — Extraer el struct SemanticDebugEntry desde
  include/nativesharp/Language/IntegrationQuality.h:178-191 hacia include/nativesharp/Language/SemanticDebugEntry.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0640 — Extraer el enum GeneratedCost desde include/nativesharp/Language/IntegrationQuality.h:197-197
  hacia include/nativesharp/Language/GeneratedCost.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0641 — Extraer el struct CostEvent desde include/nativesharp/Language/IntegrationQuality.h:198-203 hacia
  include/nativesharp/Language/CostEvent.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0642 — Extraer el enum CostOptimization desde include/nativesharp/Language/IntegrationQuality.h:206-206
  hacia include/nativesharp/Language/CostOptimization.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0643 — Extraer el struct CostAuditRecord desde include/nativesharp/Language/IntegrationQuality.h:207-215
  hacia include/nativesharp/Language/CostAuditRecord.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0644 — Extraer el enum IterationMode desde include/nativesharp/Language/IterationSemantics.h:12-12 hacia
  include/nativesharp/Language/IterationMode.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0645 — Extraer el struct IteratorProtocol desde include/nativesharp/Language/IterationSemantics.h:13-21
  hacia include/nativesharp/Language/IteratorProtocol.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0646 — Extraer el struct LazyIntegerRange desde include/nativesharp/Language/IterationSemantics.h:98-104
  hacia include/nativesharp/Language/LazyIntegerRange.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0647 — Extraer el enum BuilderStorage desde include/nativesharp/Language/IterationSemantics.h:108-108
  hacia include/nativesharp/Language/BuilderStorage.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0648 — Extraer el enum SourceMutation desde include/nativesharp/Language/IterationSemantics.h:111-111
  hacia include/nativesharp/Language/SourceMutation.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0649 — Extraer el struct GeneratorLocal desde include/nativesharp/Language/IterationSemantics.h:116-128
  hacia include/nativesharp/Language/GeneratorLocal.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0650 — Extraer el struct GeneratorFrame desde include/nativesharp/Language/IterationSemantics.h:129-129
  hacia include/nativesharp/Language/GeneratorFrame.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0651 — Extraer el enum GeneratorStorage desde include/nativesharp/Language/IterationSemantics.h:134-134
  hacia include/nativesharp/Language/GeneratorStorage.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0652 — Extraer el enum GeneratorYieldKind desde include/nativesharp/Language/IterationSemantics.h:135-135
  hacia include/nativesharp/Language/GeneratorYieldKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0653 — Extraer el struct GeneratorSuspension desde
  include/nativesharp/Language/IterationSemantics.h:137-148 hacia include/nativesharp/Language/GeneratorSuspension.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0654 — Extraer el struct GeneratorFrameSlot desde
  include/nativesharp/Language/IterationSemantics.h:150-161 hacia include/nativesharp/Language/GeneratorFrameSlot.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0655 — Extraer el struct GeneratorState desde include/nativesharp/Language/IterationSemantics.h:163-169
  hacia include/nativesharp/Language/GeneratorState.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0656 — Extraer el struct SynchronousGeneratorPlan desde
  include/nativesharp/Language/IterationSemantics.h:171-181 hacia
  include/nativesharp/Language/SynchronousGeneratorPlan.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0657 — Extraer el enum TraversalCategory desde include/nativesharp/Language/IterationSemantics.h:206-206
  hacia include/nativesharp/Language/TraversalCategory.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0658 — Extraer el struct EvaluationContract desde
  include/nativesharp/Language/IterationSemantics.h:207-216 hacia include/nativesharp/Language/EvaluationContract.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0659 — Extraer el enum FusionRejection desde include/nativesharp/Language/IterationSemantics.h:218-227
  hacia include/nativesharp/Language/FusionRejection.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0660 — Extraer el struct FusionRequest desde include/nativesharp/Language/IterationSemantics.h:229-239
  hacia include/nativesharp/Language/FusionRequest.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0661 — Extraer el struct FusionDecision desde include/nativesharp/Language/IterationSemantics.h:241-246
  hacia include/nativesharp/Language/FusionDecision.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0662 — Extraer el enum Language3Feature desde include/nativesharp/Language/Language3Surface.h:9-27 hacia
  include/nativesharp/Language/Language3Feature.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0663 — Extraer el enum Language3Disposition desde include/nativesharp/Language/Language3Surface.h:29-35
  hacia include/nativesharp/Language/Language3Disposition.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0664 — Extraer el enum StaticInitializationState desde
  include/nativesharp/Language/Language3Surface.h:37-43 hacia include/nativesharp/Language/StaticInitializationState.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0665 — Extraer el struct Language3SurfaceEntry desde
  include/nativesharp/Language/Language3Surface.h:71-77 hacia include/nativesharp/Language/Language3SurfaceEntry.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0666 — Extraer el enum AtomicOperation desde include/nativesharp/Language/MemoryModel.h:9-15 hacia
  include/nativesharp/Language/AtomicOperation.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0667 — Extraer el enum MemoryOrder desde include/nativesharp/Language/MemoryModel.h:19-26 hacia
  include/nativesharp/Language/MemoryOrder.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0668 — Extraer el enum DataRaceBehavior desde include/nativesharp/Language/MemoryModel.h:28-31 hacia
  include/nativesharp/Language/DataRaceBehavior.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0669 — Extraer el enum GeneratedDeclarationKind desde
  include/nativesharp/Language/MetaprogrammingGeneration.h:12-12 hacia
  include/nativesharp/Language/GeneratedDeclarationKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0670 — Extraer el struct GeneratedDeclaration desde
  include/nativesharp/Language/MetaprogrammingGeneration.h:19-31 hacia
  include/nativesharp/Language/GeneratedDeclaration.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0671 — Extraer el class MetaprogrammingAstBuilder desde
  include/nativesharp/Language/MetaprogrammingGeneration.h:36-74 hacia
  include/nativesharp/Language/MetaprogrammingAstBuilder.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0672 — Extraer el struct IntrospectionMember desde
  include/nativesharp/Language/MetaprogrammingSemantics.h:11-17 hacia
  include/nativesharp/Language/IntrospectionMember.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0673 — Extraer el enum AttributeRetention desde
  include/nativesharp/Language/MetaprogrammingSemantics.h:23-23 hacia include/nativesharp/Language/AttributeRetention.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0674 — Extraer el struct AttributeUse desde include/nativesharp/Language/MetaprogrammingSemantics.h:24-30
  hacia include/nativesharp/Language/AttributeUse.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0675 — Extraer el struct HygienicName desde include/nativesharp/Language/MetaprogrammingSemantics.h:37-37
  hacia include/nativesharp/Language/HygienicName.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0676 — Extraer el enum ComptimeCapability desde
  include/nativesharp/Language/MetaprogrammingSemantics.h:40-40 hacia include/nativesharp/Language/ComptimeCapability.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0677 — Extraer el struct ComptimeInput desde
  include/nativesharp/Language/MetaprogrammingSemantics.h:41-41 hacia include/nativesharp/Language/ComptimeInput.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0678 — Extraer el class ComptimeBudget desde
  include/nativesharp/Language/MetaprogrammingSemantics.h:56-65 hacia include/nativesharp/Language/ComptimeBudget.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0679 — Extraer el enum ModuleDependencyKind desde include/nativesharp/Language/ModuleSemantics.h:12-12
  hacia include/nativesharp/Language/ModuleDependencyKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0680 — Extraer el struct ModuleDependency desde include/nativesharp/Language/ModuleSemantics.h:13-13
  hacia include/nativesharp/Language/ModuleDependency.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0681 — Extraer el enum ImportKind desde include/nativesharp/Language/ModuleSemantics.h:16-16 hacia
  include/nativesharp/Language/ImportKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0682 — Extraer el struct Import desde include/nativesharp/Language/ModuleSemantics.h:17-17 hacia
  include/nativesharp/Language/Import.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante
  el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0683 — Extraer el enum Visibility desde include/nativesharp/Language/ModuleSemantics.h:20-20 hacia
  include/nativesharp/Language/Visibility.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0684 — Extraer el struct Availability desde include/nativesharp/Language/ModuleSemantics.h:30-33 hacia
  include/nativesharp/Language/Availability.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0685 — Extraer el enum AvailabilityStatus desde include/nativesharp/Language/ModuleSemantics.h:34-34
  hacia include/nativesharp/Language/AvailabilityStatus.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0686 — Extraer el struct PublicApiSymbol desde include/nativesharp/Language/ModuleSemantics.h:40-47 hacia
  include/nativesharp/Language/PublicApiSymbol.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0687 — Extraer el struct Decimal128Bits desde include/nativesharp/Language/NumericSemantics.h:23-27 hacia
  include/nativesharp/Language/Decimal128Bits.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0688 — Extraer el struct DecimalLiteralValue desde include/nativesharp/Language/NumericSemantics.h:62-68
  hacia include/nativesharp/Language/DecimalLiteralValue.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0689 — Extraer el enum DecimalArithmeticOperation desde
  include/nativesharp/Language/NumericSemantics.h:77-77 hacia include/nativesharp/Language/DecimalArithmeticOperation.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0690 — Extraer el struct IntegerType desde include/nativesharp/Language/NumericSemantics.h:88-88 hacia
  include/nativesharp/Language/IntegerType.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0691 — Extraer el enum IntegerLiteralBase desde include/nativesharp/Language/NumericSemantics.h:92-92
  hacia include/nativesharp/Language/IntegerLiteralBase.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0692 — Extraer el enum IntegerLiteralSuffix desde include/nativesharp/Language/NumericSemantics.h:93-98
  hacia include/nativesharp/Language/IntegerLiteralSuffix.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0693 — Extraer el struct UntypedIntegerLiteral desde
  include/nativesharp/Language/NumericSemantics.h:100-105 hacia include/nativesharp/Language/UntypedIntegerLiteral.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0694 — Extraer el struct ResolvedIntegerLiteral desde
  include/nativesharp/Language/NumericSemantics.h:107-113 hacia include/nativesharp/Language/ResolvedIntegerLiteral.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0695 — Extraer el enum OverflowPolicy desde include/nativesharp/Language/NumericSemantics.h:124-124 hacia
  include/nativesharp/Language/OverflowPolicy.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0696 — Extraer el enum IntegerArithmeticOperation desde
  include/nativesharp/Language/NumericSemantics.h:131-139 hacia
  include/nativesharp/Language/IntegerArithmeticOperation.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0697 — Extraer el enum NumericEvaluationDomain desde
  include/nativesharp/Language/NumericSemantics.h:140-140 hacia include/nativesharp/Language/NumericEvaluationDomain.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0698 — Extraer el struct IntegerArithmeticRequest desde
  include/nativesharp/Language/NumericSemantics.h:142-150 hacia include/nativesharp/Language/IntegerArithmeticRequest.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0699 — Extraer el struct IntegerArithmeticResult desde
  include/nativesharp/Language/NumericSemantics.h:152-157 hacia include/nativesharp/Language/IntegerArithmeticResult.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0700 — Extraer el enum NumericKind desde include/nativesharp/Language/NumericSemantics.h:166-166 hacia
  include/nativesharp/Language/NumericKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0701 — Extraer el struct NumericType desde include/nativesharp/Language/NumericSemantics.h:167-167 hacia
  include/nativesharp/Language/NumericType.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0702 — Extraer el enum NumericConversionKind desde
  include/nativesharp/Language/NumericSemantics.h:170-180 hacia include/nativesharp/Language/NumericConversionKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0703 — Extraer el enum NumericConversionUse desde include/nativesharp/Language/NumericSemantics.h:181-181
  hacia include/nativesharp/Language/NumericConversionUse.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0704 — Extraer el struct NumericConstant desde include/nativesharp/Language/NumericSemantics.h:183-188
  hacia include/nativesharp/Language/NumericConstant.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0705 — Extraer el struct NumericConversionDecision desde
  include/nativesharp/Language/NumericSemantics.h:190-197 hacia
  include/nativesharp/Language/NumericConversionDecision.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0706 — Extraer el enum FloatingPointMode desde include/nativesharp/Language/NumericSemantics.h:205-205
  hacia include/nativesharp/Language/FloatingPointMode.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0707 — Extraer el struct FloatingPointContract desde
  include/nativesharp/Language/NumericSemantics.h:206-215 hacia include/nativesharp/Language/FloatingPointContract.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0708 — Extraer el struct StrongNumericType desde include/nativesharp/Language/NumericSemantics.h:218-218
  hacia include/nativesharp/Language/StrongNumericType.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0709 — Extraer el enum StrongNumericOperator desde
  include/nativesharp/Language/NumericSemantics.h:222-222 hacia include/nativesharp/Language/StrongNumericOperator.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0710 — Extraer el struct StrongNumericDefinition desde
  include/nativesharp/Language/NumericSemantics.h:224-235 hacia include/nativesharp/Language/StrongNumericDefinition.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0711 — Extraer el struct StrongNumericPlan desde include/nativesharp/Language/NumericSemantics.h:237-250
  hacia include/nativesharp/Language/StrongNumericPlan.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0712 — Extraer el struct ConstantPointer desde include/nativesharp/Language/NumericSemantics.h:260-264
  hacia include/nativesharp/Language/ConstantPointer.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0713 — Extraer el struct ConstantAllocation desde include/nativesharp/Language/NumericSemantics.h:266-272
  hacia include/nativesharp/Language/ConstantAllocation.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0714 — Extraer el class ConstantMemory desde include/nativesharp/Language/NumericSemantics.h:274-293
  hacia include/nativesharp/Language/ConstantMemory.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0715 — Extraer el struct ConstantUnionState desde include/nativesharp/Language/NumericSemantics.h:295-299
  hacia include/nativesharp/Language/ConstantUnionState.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0716 — Extraer el enum ValueState desde include/nativesharp/Language/OwnershipSemantics.h:11-11 hacia
  include/nativesharp/Language/ValueState.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0717 — Extraer el struct MoveState desde include/nativesharp/Language/OwnershipSemantics.h:13-18 hacia
  include/nativesharp/Language/MoveState.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0718 — Extraer el struct BorrowPlace desde include/nativesharp/Language/OwnershipSemantics.h:25-29 hacia
  include/nativesharp/Language/BorrowPlace.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0719 — Extraer el enum CleanupKind desde include/nativesharp/Language/OwnershipSemantics.h:39-39 hacia
  include/nativesharp/Language/CleanupKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0720 — Extraer el struct CleanupAction desde include/nativesharp/Language/OwnershipSemantics.h:40-40
  hacia include/nativesharp/Language/CleanupAction.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0721 — Extraer el enum EscapeClass desde include/nativesharp/Language/OwnershipSemantics.h:45-45 hacia
  include/nativesharp/Language/EscapeClass.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0722 — Extraer el struct EscapeSummary desde include/nativesharp/Language/OwnershipSemantics.h:46-50
  hacia include/nativesharp/Language/EscapeSummary.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0723 — Extraer el struct FreezeManifest desde include/nativesharp/Language/ReleaseSemantics.h:11-17 hacia
  include/nativesharp/Language/FreezeManifest.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0724 — Extraer el struct SyntaxFreezeManifest desde include/nativesharp/Language/ReleaseSemantics.h:20-32
  hacia include/nativesharp/Language/SyntaxFreezeManifest.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0725 — Extraer el struct SemanticFreezeManifest desde
  include/nativesharp/Language/ReleaseSemantics.h:35-47 hacia include/nativesharp/Language/SemanticFreezeManifest.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0726 — Extraer el struct Win64AbiFreezeManifest desde
  include/nativesharp/Language/ReleaseSemantics.h:50-65 hacia include/nativesharp/Language/Win64AbiFreezeManifest.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0727 — Extraer el enum ConformanceTier desde include/nativesharp/Language/ReleaseSemantics.h:68-68 hacia
  include/nativesharp/Language/ConformanceTier.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0728 — Extraer el struct ConformanceCase desde include/nativesharp/Language/ReleaseSemantics.h:69-69
  hacia include/nativesharp/Language/ConformanceCase.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0729 — Extraer el struct CandidateConformanceManifest desde
  include/nativesharp/Language/ReleaseSemantics.h:72-81 hacia
  include/nativesharp/Language/CandidateConformanceManifest.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0730 — Extraer el struct BilingualClause desde include/nativesharp/Language/ReleaseSemantics.h:86-99
  hacia include/nativesharp/Language/BilingualClause.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0731 — Extraer el enum ReleaseCandidateStage desde
  include/nativesharp/Language/ReleaseSemantics.h:105-105 hacia include/nativesharp/Language/ReleaseCandidateStage.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0732 — Extraer el enum ChangeSeverity desde include/nativesharp/Language/ReleaseSemantics.h:106-106 hacia
  include/nativesharp/Language/ChangeSeverity.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0733 — Extraer el struct ReleaseCandidateEvidence desde
  include/nativesharp/Language/ReleaseSemantics.h:109-122 hacia include/nativesharp/Language/ReleaseCandidateEvidence.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0734 — Extraer el struct PromotionEvidence desde include/nativesharp/Language/ReleaseSemantics.h:127-138
  hacia include/nativesharp/Language/PromotionEvidence.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0735 — Extraer el enum PromotionStatus desde include/nativesharp/Language/ReleaseSemantics.h:139-139
  hacia include/nativesharp/Language/PromotionStatus.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0736 — Extraer el struct OfficialPromotionManifest desde
  include/nativesharp/Language/ReleaseSemantics.h:143-155 hacia
  include/nativesharp/Language/OfficialPromotionManifest.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0737 — Extraer el enum SmartPointerFamily desde
  include/nativesharp/Language/SmartPointerSemantics.h:14-21 hacia include/nativesharp/Language/SmartPointerFamily.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0738 — Extraer el enum SmartPointerThreadModel desde
  include/nativesharp/Language/SmartPointerSemantics.h:23-28 hacia
  include/nativesharp/Language/SmartPointerThreadModel.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0739 — Extraer el struct SmartPointerFamilyContract desde
  include/nativesharp/Language/SmartPointerSemantics.h:30-39 hacia
  include/nativesharp/Language/SmartPointerFamilyContract.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0740 — Extraer el enum SmartPointerOperation desde
  include/nativesharp/Language/SmartPointerSemantics.h:54-64 hacia include/nativesharp/Language/SmartPointerOperation.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0741 — Extraer el struct SmartPointerOperationContext desde
  include/nativesharp/Language/SmartPointerSemantics.h:66-74 hacia
  include/nativesharp/Language/SmartPointerOperationContext.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0742 — Extraer el struct VariantCase desde include/nativesharp/Language/SumTypeSemantics.h:12-17 hacia
  include/nativesharp/Language/VariantCase.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0743 — Extraer el struct VariantLayout desde include/nativesharp/Language/SumTypeSemantics.h:19-25 hacia
  include/nativesharp/Language/VariantLayout.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0744 — Extraer el enum PatternKind desde include/nativesharp/Language/SumTypeSemantics.h:30-40 hacia
  include/nativesharp/Language/PatternKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0745 — Extraer el enum PatternBindingMode desde include/nativesharp/Language/SumTypeSemantics.h:42-49
  hacia include/nativesharp/Language/PatternBindingMode.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0746 — Extraer el struct PatternBinding desde include/nativesharp/Language/SumTypeSemantics.h:51-56 hacia
  include/nativesharp/Language/PatternBinding.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0747 — Extraer el struct MatchArmCoverage desde include/nativesharp/Language/SumTypeSemantics.h:63-79
  hacia include/nativesharp/Language/MatchArmCoverage.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0748 — Extraer el struct ExhaustivenessResult desde include/nativesharp/Language/SumTypeSemantics.h:81-86
  hacia include/nativesharp/Language/ExhaustivenessResult.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0749 — Extraer el struct TraitMethod desde include/nativesharp/Language/TraitSemantics.h:10-16 hacia
  include/nativesharp/Language/TraitMethod.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0750 — Extraer el struct TraitContract desde include/nativesharp/Language/TraitSemantics.h:18-23 hacia
  include/nativesharp/Language/TraitContract.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0751 — Extraer el struct ImplCandidate desde include/nativesharp/Language/TraitSemantics.h:27-34 hacia
  include/nativesharp/Language/ImplCandidate.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0752 — Extraer el struct AssociatedProjection desde include/nativesharp/Language/TraitSemantics.h:38-42
  hacia include/nativesharp/Language/AssociatedProjection.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0753 — Extraer el enum DispatchKind desde include/nativesharp/Language/TraitSemantics.h:53-53 hacia
  include/nativesharp/Language/DispatchKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0754 — Extraer el struct TraitObjectLayout desde include/nativesharp/Language/TraitSemantics.h:54-61
  hacia include/nativesharp/Language/TraitObjectLayout.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0755 — Extraer el enum ExtensionResolution desde include/nativesharp/Language/TraitSemantics.h:65-65
  hacia include/nativesharp/Language/ExtensionResolution.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0756 — Extraer el struct OperatorTraitContract desde include/nativesharp/Language/TraitSemantics.h:73-79
  hacia include/nativesharp/Language/OperatorTraitContract.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0806 — Extraer el enum BuiltinLayoutKind desde include/nativesharp/Target/TargetDataLayout.h:10-36 hacia
  include/nativesharp/Target/BuiltinLayoutKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0807 — Extraer el struct TypeLayout desde include/nativesharp/Target/TargetDataLayout.h:38-42 hacia
  include/nativesharp/Target/TypeLayout.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0808 — Extraer el struct ArrayLayout desde include/nativesharp/Target/TargetDataLayout.h:44-50 hacia
  include/nativesharp/Target/ArrayLayout.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0809 — Extraer el struct StructFieldLayout desde include/nativesharp/Target/TargetDataLayout.h:52-61
  hacia include/nativesharp/Target/StructFieldLayout.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0810 — Extraer el struct StructLayout desde include/nativesharp/Target/TargetDataLayout.h:63-68 hacia
  include/nativesharp/Target/StructLayout.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0811 — Extraer el struct StructLayoutOptions desde include/nativesharp/Target/TargetDataLayout.h:84-89
  hacia include/nativesharp/Target/StructLayoutOptions.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0812 — Extraer el struct StructFieldRequest desde include/nativesharp/Target/TargetDataLayout.h:91-99
  hacia include/nativesharp/Target/StructFieldRequest.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0813 — Conservar el class TargetDataLayout como tipo principal de
  include/nativesharp/Target/TargetDataLayout.h:101-131 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0861 — Descomponer buildMetadataModel en src/Interop/LibraryMetadata.cpp:754 (920 líneas ejecutables;
  1305 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-0865 — Descomponer LibraryMetadataCodec::verifyCurrentCompiler en src/Interop/LibraryMetadata.cpp:4233
  (724 líneas ejecutables; 1058 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0867 — Descomponer decodePayload en src/Interop/LibraryMetadata.cpp:2672 (595 líneas ejecutables; 871
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-0891 — Descomponer encodePayload en src/Interop/LibraryMetadata.cpp:2081 (305 líneas ejecutables; 430
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-0901 — Descomponer LibraryMetadataCodec::writeModelImpl en src/Interop/LibraryMetadata.cpp:3664 (270
  líneas ejecutables; 454 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0951 — Descomponer LibraryAbiReporter::write en src/Interop/LibraryAbiReport.cpp:113 (160 líneas
  ejecutables; 197 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-0972 — Descomponer sourceSurface en src/Interop/LibraryApiDiff.cpp:160 (138 líneas ejecutables; 177
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-0975 — Descomponer emitStruct en src/Interop/CHeaderGenerator.cpp:426 (135 líneas ejecutables; 192
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-0976 — Descomponer abiSurface en src/Interop/LibraryApiDiff.cpp:338 (135 líneas ejecutables; 167 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [x] NSC-IMPL-1022 — Descomponer TargetDataLayout::layoutStruct en src/Target/TargetDataLayout.cpp:94 (103 líneas
  ejecutables; 155 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1027 — Descomponer CHeaderGenerator::writeHeader en src/Interop/CHeaderGenerator.cpp:797 (101 líneas
  ejecutables; 142 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1028 — Descomponer encodeAbiProjection en src/Interop/LibraryMetadata.cpp:2512 (101 líneas ejecutables;
  154 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1037 — Descomponer SdkManifestLoader::load en src/Compilation/SdkManifest.cpp:295 (96 líneas ejecutables;
  148 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1039 — Descomponer canonicalizeModel en src/Interop/LibraryMetadataMerger.cpp:270 (94 líneas ejecutables;
  108 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1044 — Descomponer materializeGenericTemplateSource en src/Interop/LibraryMetadata.cpp:621 (92 líneas
  ejecutables; 132 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1056 — Descomponer parseStaticInterpolation en src/Language/ExpressionErgonomics.cpp:78 (87 líneas
  ejecutables; 124 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1089 — Descomponer buildSynchronousGeneratorPlan en src/Language/IterationSemantics.cpp:93 (80 líneas
  ejecutables; 119 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1097 — Descomponer LibraryMetadataCodec::read en src/Interop/LibraryMetadata.cpp:4119 (79 líneas
  ejecutables; 113 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1118 — Descomponer parseDecimalLiteral en src/Language/NumericSemantics.cpp:150 (75 líneas ejecutables;
  102 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1138 — Descomponer evaluateIntegerArithmetic en src/Language/NumericSemantics.cpp:547 (71 líneas
  ejecutables; 97 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1219 — Convertir LibraryMetadataCodec en fachada y separar códec, construcción, verificación y payload
  sin cambiar NSMETA.

### Commit REF-03 — Modelos base del frontend, nombres, miembros, construcción y genéricos — 249/249

Modulariza los modelos del frontend y sus fachadas sin mover estado ni alterar resolución o especialización.

- [x] NSC-IMPL-0053 — Extraer el class IntegerExpression desde src/Frontend/ComptimeEngine.cpp:34-301 (268) hacia
  src/Frontend/Internal/IntegerExpression.h, y mover sus cuerpos a src/Frontend/Internal/IntegerExpression.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0058 — Extraer el struct Definition desde src/Frontend/MacroExpansionEngine.cpp:10-17 (8) hacia
  src/Frontend/Internal/Definition.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0059 — Extraer el class ProjectPhaseTimer desde src/Frontend/ProjectCompilation.cpp:76-109 (34) hacia
  src/Frontend/Internal/ProjectPhaseTimer.h, y mover sus cuerpos a src/Frontend/Internal/ProjectPhaseTimer.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0060 — Extraer el struct NamedConstructorArgumentMap desde
  src/Frontend/ProjectConstructionModel.cpp:15-24 (10) hacia src/Frontend/Internal/NamedConstructorArgumentMap.h, sin
  .cpp adicional; conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0061 — Extraer el struct PendingDiagnostic desde src/Frontend/ProjectGenericConstraintModel.cpp:275-281
  (7) hacia src/Frontend/Internal/PendingDiagnostic.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0062 — Extraer el class SpecializationWaveTimer desde src/Frontend/ProjectGenericModel.cpp:47-71 (25)
  hacia src/Frontend/Internal/SpecializationWaveTimer.h, y mover sus cuerpos a
  src/Frontend/Internal/SpecializationWaveTimer.cpp; conservar texto, comentarios, firmas, orden y conducta. Restricción
  de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0063 — Extraer el class EnumConstResolver desde src/Frontend/ProjectGenericModel.cpp:374-440 (67) hacia
  src/Frontend/Internal/EnumConstResolver.h, y mover sus cuerpos a src/Frontend/Internal/EnumConstResolver.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0064 — Extraer el class ByteReader desde src/Frontend/ProjectGenericModel.cpp:2194-2254 (61) hacia
  src/Frontend/Internal/ByteReader.h, y mover sus cuerpos a src/Frontend/Internal/ByteReader.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0065 — Extraer el struct InterfaceFingerprint desde src/Frontend/ProjectGenericModel.cpp:2901-2906 (6)
  hacia src/Frontend/Internal/InterfaceFingerprint.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0066 — Extraer el struct ImplementationFingerprint desde src/Frontend/ProjectGenericModel.cpp:2908-2912
  (5) hacia src/Frontend/Internal/ImplementationFingerprint.h, sin .cpp adicional; conservar texto, comentarios, firmas,
  orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0067 — Extraer el struct MirFunctionKey desde src/Frontend/ProjectGenericModel.cpp:2971-2978 (8) hacia
  src/Frontend/Internal/MirFunctionKey.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0068 — Extraer el struct MirCacheRecord desde src/Frontend/ProjectGenericModel.cpp:3004-3009 (6) hacia
  src/Frontend/Internal/MirCacheRecord.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0069 — Extraer el struct CloneDepthGuard desde src/Frontend/ProjectGenericModel.cpp:6748-6753 (6) hacia
  src/Frontend/Internal/CloneDepthGuard.h, y mover sus cuerpos a src/Frontend/Internal/CloneDepthGuard.cpp; conservar
  texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0070 — Extraer el struct EnumValue desde src/Frontend/ProjectMemberModel.cpp:3427-3431 (5) hacia
  src/Frontend/Internal/EnumValue.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0071 — Extraer el struct EnumSuccessor desde src/Frontend/ProjectMemberModel.cpp:3484-3488 (5) hacia
  src/Frontend/Internal/EnumSuccessor.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0072 — Extraer el class EnumInitializerResolver desde src/Frontend/ProjectMemberModel.cpp:3506-3567 (62)
  hacia src/Frontend/Internal/EnumInitializerResolver.h, y mover sus cuerpos a
  src/Frontend/Internal/EnumInitializerResolver.cpp; conservar texto, comentarios, firmas, orden y conducta. Restricción
  de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0075 — Extraer el struct ProjectNameLookup::LookupEnvironment desde
  src/Frontend/ProjectNameLookup.cpp:250-263 (14) hacia src/Frontend/Internal/LookupEnvironment.h, sin .cpp adicional;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0076 — Extraer el struct FunctionLookupEnvironment desde
  src/Frontend/ProjectNamespaceFunctionModel.cpp:263-267 (5) hacia src/Frontend/Internal/FunctionLookupEnvironment.h,
  sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite
  (A-1).
- [x] NSC-IMPL-0077 — Extraer el struct LocatedDeclaration desde src/Frontend/ProjectOpaqueResultModel.cpp:29-36 (8)
  hacia src/Frontend/Internal/LocatedDeclaration.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0078 — Extraer el struct CacheMetadata desde src/Frontend/TemplateParseCache.cpp:116-121 (6) hacia
  src/Frontend/Internal/CacheMetadata.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [x] NSC-IMPL-0153 — Mover fuera de línea función libre declaredEnumUnderlyingTypeKey de
  include/nativesharp/Frontend/EnumUnderlyingType.h:43 (30 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0154 — Mover fuera de línea función libre enumUnderlyingTypeKey de
  include/nativesharp/Frontend/EnumUnderlyingType.h:76 (5 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0155 — Mover fuera de línea método GenericTelemetry::recordSubPhase de
  include/nativesharp/Frontend/ProjectGenericModel.h:262 (6 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0156 — Mover fuera de línea método GenericTelemetry::noteResolutionQuery de
  include/nativesharp/Frontend/ProjectGenericModel.h:283 (6 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0157 — Mover fuera de línea método GenericTelemetry::noteTemplate de
  include/nativesharp/Frontend/ProjectGenericModel.h:290 (6 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0158 — Mover fuera de línea método GenericTelemetry::noteScopeCopy de
  include/nativesharp/Frontend/ProjectGenericModel.h:297 (6 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0159 — Mover fuera de línea método GenericTelemetry::noteSpecializationHit de
  include/nativesharp/Frontend/ProjectGenericModel.h:304 (4 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0160 — Mover fuera de línea método GenericTelemetry::noteMaterialization de
  include/nativesharp/Frontend/ProjectGenericModel.h:309 (13 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0161 — Mover fuera de línea método GenericTelemetry::noteDemand de
  include/nativesharp/Frontend/ProjectGenericModel.h:323 (6 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0162 — Mover fuera de línea método GenericTelemetry::noteNodesCloned de
  include/nativesharp/Frontend/ProjectGenericModel.h:330 (4 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0163 — Mover fuera de línea método GenericTelemetry::noteTypesCloned de
  include/nativesharp/Frontend/ProjectGenericModel.h:335 (4 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0164 — Mover fuera de línea método Scope::Scope de include/nativesharp/Frontend/ProjectGenericModel.h:342
  (27 líneas) a un único .cpp, dejando la declaración intacta y conservando exactamente el cuerpo.
- [x] NSC-IMPL-0165 — Mover fuera de línea método Scope::~Scope de
  include/nativesharp/Frontend/ProjectGenericModel.h:369 (7 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0166 — Mover fuera de línea método ValueBinding::ValueBinding de
  include/nativesharp/Frontend/ProjectGenericModel.h:603 (7 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0167 — Mover fuera de línea método ValueBinding::~ValueBinding de
  include/nativesharp/Frontend/ProjectGenericModel.h:611 (13 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0168 — Mover fuera de línea método GenericLookupScope::addValueType de
  include/nativesharp/Frontend/ProjectGenericModel.h:630 (5 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0169 — Mover fuera de línea método GenericLookupScope::findValueType de
  include/nativesharp/Frontend/ProjectGenericModel.h:635 (9 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0170 — Mover fuera de línea método SpecializationContext::save de
  include/nativesharp/Frontend/ProjectGenericModel.h:730 (13 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0171 — Mover fuera de línea método SpecializationContext::restore de
  include/nativesharp/Frontend/ProjectGenericModel.h:743 (13 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0172 — Mover fuera de línea método SpecializationPlanner::request de
  include/nativesharp/Frontend/ProjectGenericModel.h:1275 (5 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0173 — Mover fuera de línea método SpecializationPlanner::commit de
  include/nativesharp/Frontend/ProjectGenericModel.h:1280 (4 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0174 — Mover fuera de línea método SpecializationPlanner::rollback de
  include/nativesharp/Frontend/ProjectGenericModel.h:1284 (4 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0175 — Mover fuera de línea método ProjectIntegerValue::asSigned de
  include/nativesharp/Frontend/ProjectIntegerEvaluator.h:38 (4 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0176 — Mover fuera de línea método ProjectIntegerEvaluator::ProjectIntegerEvaluator de
  include/nativesharp/Frontend/ProjectIntegerEvaluator.h:101 (4 líneas) a un único .cpp, dejando la declaración intacta
  y conservando exactamente el cuerpo.
- [x] NSC-IMPL-0177 — Mover fuera de línea método TransparentStringHash::operator de
  include/nativesharp/Frontend/ProjectMemberModel.h:407 (4 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0178 — Mover fuera de línea método TransparentStringHash::operator de
  include/nativesharp/Frontend/ProjectMemberModel.h:412 (4 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [x] NSC-IMPL-0266 — Conservar el enum CompilationStage como tipo principal de
  include/nativesharp/Frontend/CompilationStage.h:11-20 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0267 — Extraer el enum StageInformation desde include/nativesharp/Frontend/CompilationStage.h:22-32 hacia
  include/nativesharp/Frontend/StageInformation.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0268 — Extraer el struct CompilationStageContract desde
  include/nativesharp/Frontend/CompilationStage.h:48-55 hacia include/nativesharp/Frontend/CompilationStageContract.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0269 — Extraer el class CompilationStageTracker desde
  include/nativesharp/Frontend/CompilationStage.h:61-71 hacia include/nativesharp/Frontend/CompilationStageTracker.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0270 — Extraer el struct ComptimeEvaluation desde include/nativesharp/Frontend/ComptimeEngine.h:19-26
  hacia include/nativesharp/Frontend/ComptimeEvaluation.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0271 — Extraer el struct ComptimeDependency desde include/nativesharp/Frontend/ComptimeEngine.h:28-33
  hacia include/nativesharp/Frontend/ComptimeDependency.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0272 — Extraer el enum ComptimeAttributeTarget desde include/nativesharp/Frontend/ComptimeEngine.h:35-35
  hacia include/nativesharp/Frontend/ComptimeAttributeTarget.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0273 — Extraer el enum ComptimeAttributeRetention desde
  include/nativesharp/Frontend/ComptimeEngine.h:36-36 hacia include/nativesharp/Frontend/ComptimeAttributeRetention.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0274 — Extraer el struct ComptimeAttributeDirective desde
  include/nativesharp/Frontend/ComptimeEngine.h:38-48 hacia include/nativesharp/Frontend/ComptimeAttributeDirective.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0275 — Extraer el enum ComptimeDiagnosticSeverity desde
  include/nativesharp/Frontend/ComptimeEngine.h:50-50 hacia include/nativesharp/Frontend/ComptimeDiagnosticSeverity.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0276 — Extraer el struct ComptimeAuthoredDiagnostic desde
  include/nativesharp/Frontend/ComptimeEngine.h:52-60 hacia include/nativesharp/Frontend/ComptimeAuthoredDiagnostic.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0277 — Extraer el class ComptimeCache desde include/nativesharp/Frontend/ComptimeEngine.h:62-78 hacia
  include/nativesharp/Frontend/ComptimeCache.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0278 — Extraer el struct ComptimeLimits desde include/nativesharp/Frontend/ComptimeEngine.h:80-88 hacia
  include/nativesharp/Frontend/ComptimeLimits.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0279 — Extraer el struct ComptimeExecutionContext desde
  include/nativesharp/Frontend/ComptimeEngine.h:90-95 hacia include/nativesharp/Frontend/ComptimeExecutionContext.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0280 — Extraer el struct ComptimeResult desde include/nativesharp/Frontend/ComptimeEngine.h:97-113 hacia
  include/nativesharp/Frontend/ComptimeResult.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0281 — Extraer el struct ProjectComptimeRecord desde
  include/nativesharp/Frontend/ComptimeEngine.h:115-131 hacia include/nativesharp/Frontend/ProjectComptimeRecord.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0282 — Conservar el class ComptimeEngine como tipo principal de
  include/nativesharp/Frontend/ComptimeEngine.h:137-150 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0283 — Extraer el enum MacroExpansionContext desde
  include/nativesharp/Frontend/MacroExpansionEngine.h:14-14 hacia include/nativesharp/Frontend/MacroExpansionContext.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0284 — Extraer el struct MacroExpansionRecord desde
  include/nativesharp/Frontend/MacroExpansionEngine.h:16-28 hacia include/nativesharp/Frontend/MacroExpansionRecord.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0285 — Extraer el struct MacroExpansionResult desde
  include/nativesharp/Frontend/MacroExpansionEngine.h:30-36 hacia include/nativesharp/Frontend/MacroExpansionResult.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0286 — Conservar el class MacroExpansionEngine como tipo principal de
  include/nativesharp/Frontend/MacroExpansionEngine.h:40-54 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0287 — Conservar el class NamespaceSymbol como tipo principal de
  include/nativesharp/Frontend/NamespaceSymbol.h:12-39 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0288 — Extraer el class GlobalNamespace desde include/nativesharp/Frontend/NamespaceSymbol.h:41-55 hacia
  include/nativesharp/Frontend/GlobalNamespace.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0289 — Extraer el enum PrecompiledMemberIdentityMatchKind desde
  include/nativesharp/Frontend/PrecompiledMemberIdentityIndex.h:12-17 hacia
  include/nativesharp/Frontend/PrecompiledMemberIdentityMatchKind.h; conservar texto, comentarios, orden de declaración
  y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0290 — Extraer el struct PrecompiledMemberIdentityMatch desde
  include/nativesharp/Frontend/PrecompiledMemberIdentityIndex.h:19-24 hacia
  include/nativesharp/Frontend/PrecompiledMemberIdentityMatch.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0291 — Conservar el class PrecompiledMemberIdentityIndex como tipo principal de
  include/nativesharp/Frontend/PrecompiledMemberIdentityIndex.h:28-54 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0292 — Extraer el struct PrecompiledSyntaxModule desde
  include/nativesharp/Frontend/ProjectCompilation.h:39-49 hacia include/nativesharp/Frontend/PrecompiledSyntaxModule.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0293 — Conservar el class ProjectCompilation como tipo principal de
  include/nativesharp/Frontend/ProjectCompilation.h:51-167 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0294 — Extraer el enum ConstructionResolutionKind desde
  include/nativesharp/Frontend/ProjectConstructionModel.h:30-38 hacia
  include/nativesharp/Frontend/ConstructionResolutionKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0295 — Extraer el struct ConstructorOwnershipTransfer desde
  include/nativesharp/Frontend/ProjectConstructionModel.h:43-48 hacia
  include/nativesharp/Frontend/ConstructorOwnershipTransfer.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0296 — Extraer el struct ConstructorCandidateBinding desde
  include/nativesharp/Frontend/ProjectConstructionModel.h:50-80 hacia
  include/nativesharp/Frontend/ConstructorCandidateBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0297 — Extraer el enum ConstructorInitializerKind desde
  include/nativesharp/Frontend/ProjectConstructionModel.h:83-88 hacia
  include/nativesharp/Frontend/ConstructorInitializerKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0298 — Extraer el enum ConstructorInitializerResolutionKind desde
  include/nativesharp/Frontend/ProjectConstructionModel.h:90-99 hacia
  include/nativesharp/Frontend/ConstructorInitializerResolutionKind.h; conservar texto, comentarios, orden de
  declaración y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0299 — Extraer el struct ConstructorInitializerBinding desde
  include/nativesharp/Frontend/ProjectConstructionModel.h:101-127 hacia
  include/nativesharp/Frontend/ConstructorInitializerBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0300 — Extraer el enum ImplicitDefaultConstructorResolutionKind desde
  include/nativesharp/Frontend/ProjectConstructionModel.h:129-137 hacia
  include/nativesharp/Frontend/ImplicitDefaultConstructorResolutionKind.h; conservar texto, comentarios, orden de
  declaración y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0301 — Extraer el struct ImplicitDefaultConstructorBinding desde
  include/nativesharp/Frontend/ProjectConstructionModel.h:139-147 hacia
  include/nativesharp/Frontend/ImplicitDefaultConstructorBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0302 — Extraer el struct ObjectConstructionBinding desde
  include/nativesharp/Frontend/ProjectConstructionModel.h:149-191 hacia
  include/nativesharp/Frontend/ObjectConstructionBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0303 — Conservar el class ProjectConstructionModel como tipo principal de
  include/nativesharp/Frontend/ProjectConstructionModel.h:193-290 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0304 — Extraer el enum ProjectAllocatorFailureProfile desde
  include/nativesharp/Frontend/ProjectConstructionSite.h:15-22 hacia
  include/nativesharp/Frontend/ProjectAllocatorFailureProfile.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0305 — Conservar el struct ProjectConstructionSite como tipo principal de
  include/nativesharp/Frontend/ProjectConstructionSite.h:27-48 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0306 — Extraer el struct ProjectFunctionCosts desde
  include/nativesharp/Frontend/ProjectCostExplorer.h:12-27 hacia include/nativesharp/Frontend/ProjectFunctionCosts.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0307 — Extraer el struct ProjectCostReport desde include/nativesharp/Frontend/ProjectCostExplorer.h:29-33
  hacia include/nativesharp/Frontend/ProjectCostReport.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0308 — Extraer el struct ClassDestructionPlan desde
  include/nativesharp/Frontend/ProjectDestructionModel.h:21-32 hacia
  include/nativesharp/Frontend/ClassDestructionPlan.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0309 — Conservar el class ProjectDestructionModel como tipo principal de
  include/nativesharp/Frontend/ProjectDestructionModel.h:34-59 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0310 — Extraer el enum GenericConstraintViolationKind desde
  include/nativesharp/Frontend/ProjectGenericConstraintModel.h:27-55 hacia
  include/nativesharp/Frontend/GenericConstraintViolationKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0311 — Extraer el struct GenericConstraintViolation desde
  include/nativesharp/Frontend/ProjectGenericConstraintModel.h:57-71 hacia
  include/nativesharp/Frontend/GenericConstraintViolation.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0312 — Conservar el class ProjectGenericConstraintModel como tipo principal de
  include/nativesharp/Frontend/ProjectGenericConstraintModel.h:86-203 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0313 — Extraer el enum GenericTemplateKind desde include/nativesharp/Frontend/ProjectGenericModel.h:36-44
  hacia include/nativesharp/Frontend/GenericTemplateKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0314 — Extraer el enum GenericTemplateAccessibility desde
  include/nativesharp/Frontend/ProjectGenericModel.h:46-52 hacia
  include/nativesharp/Frontend/GenericTemplateAccessibility.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0315 — Extraer el struct GenericTemplateExternalTypeReference desde
  include/nativesharp/Frontend/ProjectGenericModel.h:53-59 hacia
  include/nativesharp/Frontend/GenericTemplateExternalTypeReference.h; conservar texto, comentarios, orden de
  declaración y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0316 — Extraer el struct GenericTemplateRecord desde
  include/nativesharp/Frontend/ProjectGenericModel.h:61-114 hacia include/nativesharp/Frontend/GenericTemplateRecord.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0317 — Extraer el enum SpecializationState desde
  include/nativesharp/Frontend/ProjectGenericModel.h:119-126 hacia include/nativesharp/Frontend/SpecializationState.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0318 — Extraer el struct InstantiationContext desde
  include/nativesharp/Frontend/ProjectGenericModel.h:131-139 hacia include/nativesharp/Frontend/InstantiationContext.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0319 — Extraer el struct GenericSpecializationRecord desde
  include/nativesharp/Frontend/ProjectGenericModel.h:141-176 hacia
  include/nativesharp/Frontend/GenericSpecializationRecord.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0320 — Extraer el struct PrecompiledGenericSpecialization desde
  include/nativesharp/Frontend/ProjectGenericModel.h:181-193 hacia
  include/nativesharp/Frontend/PrecompiledGenericSpecialization.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0321 — Extraer el struct GenericInstantiationReport desde
  include/nativesharp/Frontend/ProjectGenericModel.h:195-202 hacia
  include/nativesharp/Frontend/GenericInstantiationReport.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0322 — Extraer el struct GenericTelemetry desde
  include/nativesharp/Frontend/ProjectGenericModel.h:212-389 hacia include/nativesharp/Frontend/GenericTelemetry.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0323 — Conservar el class ProjectGenericModel como tipo principal de
  include/nativesharp/Frontend/ProjectGenericModel.h:391-1380 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0324 — Extraer el struct ClassInheritanceInfo desde
  include/nativesharp/Frontend/ProjectInheritanceModel.h:21-28 hacia
  include/nativesharp/Frontend/ClassInheritanceInfo.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0325 — Conservar el class ProjectInheritanceModel como tipo principal de
  include/nativesharp/Frontend/ProjectInheritanceModel.h:30-83 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0326 — Extraer el struct ProjectIntegerValue desde
  include/nativesharp/Frontend/ProjectIntegerEvaluator.h:30-42 hacia include/nativesharp/Frontend/ProjectIntegerValue.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0327 — Extraer el class ProjectConstantMemberResolver desde
  include/nativesharp/Frontend/ProjectIntegerEvaluator.h:50-85 hacia
  include/nativesharp/Frontend/ProjectConstantMemberResolver.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0328 — Conservar el class ProjectIntegerEvaluator como tipo principal de
  include/nativesharp/Frontend/ProjectIntegerEvaluator.h:87-141 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0329 — Extraer el struct InterfaceTypeInfo desde
  include/nativesharp/Frontend/ProjectInterfaceModel.h:22-30 hacia include/nativesharp/Frontend/InterfaceTypeInfo.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0330 — Extraer el struct ClassInterfaceTablePlan desde
  include/nativesharp/Frontend/ProjectInterfaceModel.h:32-39 hacia
  include/nativesharp/Frontend/ClassInterfaceTablePlan.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0331 — Extraer el struct TraitOperatorWitness desde
  include/nativesharp/Frontend/ProjectInterfaceModel.h:44-51 hacia include/nativesharp/Frontend/TraitOperatorWitness.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0332 — Extraer el struct TraitObjectContract desde
  include/nativesharp/Frontend/ProjectInterfaceModel.h:58-67 hacia include/nativesharp/Frontend/TraitObjectContract.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0333 — Conservar el class ProjectInterfaceModel como tipo principal de
  include/nativesharp/Frontend/ProjectInterfaceModel.h:69-135 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0334 — Extraer el enum ClassMemberKind desde include/nativesharp/Frontend/ProjectMemberModel.h:40-47
  hacia include/nativesharp/Frontend/ClassMemberKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0335 — Extraer el enum ClassMemberAccessibility desde
  include/nativesharp/Frontend/ProjectMemberModel.h:49-55 hacia include/nativesharp/Frontend/ClassMemberAccessibility.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0336 — Extraer el enum ParameterPassKind desde include/nativesharp/Frontend/ProjectMemberModel.h:70-76
  hacia include/nativesharp/Frontend/ParameterPassKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0337 — Extraer el struct MethodParameterSymbol desde
  include/nativesharp/Frontend/ProjectMemberModel.h:81-120 hacia include/nativesharp/Frontend/MethodParameterSymbol.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0338 — Extraer el struct ClassMemberSymbol desde
  include/nativesharp/Frontend/ProjectMemberModel.h:122-294 hacia include/nativesharp/Frontend/ClassMemberSymbol.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0339 — Extraer el struct ModuleInitializationRecord desde
  include/nativesharp/Frontend/ProjectMemberModel.h:296-302 hacia
  include/nativesharp/Frontend/ModuleInitializationRecord.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0340 — Extraer el struct PrecompiledMemberIdentity desde
  include/nativesharp/Frontend/ProjectMemberModel.h:304-312 hacia
  include/nativesharp/Frontend/PrecompiledMemberIdentity.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0341 — Extraer el enum MemberBodyBindingKind desde
  include/nativesharp/Frontend/ProjectMemberModel.h:314-318 hacia include/nativesharp/Frontend/MemberBodyBindingKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0342 — Extraer el struct MemberBodyBinding desde
  include/nativesharp/Frontend/ProjectMemberModel.h:320-328 hacia include/nativesharp/Frontend/MemberBodyBinding.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0343 — Conservar el class ProjectMemberModel como tipo principal de
  include/nativesharp/Frontend/ProjectMemberModel.h:330-471 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0344 — Extraer el enum StaticMemberKind desde
  include/nativesharp/Frontend/ProjectMetaprogrammingModel.h:20-20 hacia
  include/nativesharp/Frontend/StaticMemberKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0345 — Extraer el struct StaticMemberInfo desde
  include/nativesharp/Frontend/ProjectMetaprogrammingModel.h:22-39 hacia
  include/nativesharp/Frontend/StaticMemberInfo.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0346 — Extraer el struct StaticTypeInfo desde
  include/nativesharp/Frontend/ProjectMetaprogrammingModel.h:41-54 hacia include/nativesharp/Frontend/StaticTypeInfo.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0347 — Extraer el struct StaticAttributeInfo desde
  include/nativesharp/Frontend/ProjectMetaprogrammingModel.h:56-63 hacia
  include/nativesharp/Frontend/StaticAttributeInfo.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0348 — Extraer el struct StaticAvailabilityInfo desde
  include/nativesharp/Frontend/ProjectMetaprogrammingModel.h:67-76 hacia
  include/nativesharp/Frontend/StaticAvailabilityInfo.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0349 — Extraer el struct StaticDeriveInfo desde
  include/nativesharp/Frontend/ProjectMetaprogrammingModel.h:78-85 hacia
  include/nativesharp/Frontend/StaticDeriveInfo.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0350 — Conservar el class ProjectMetaprogrammingModel como tipo principal de
  include/nativesharp/Frontend/ProjectMetaprogrammingModel.h:89-124 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0407 — Extraer el enum NamedTypeDeclarationKind desde
  include/nativesharp/Frontend/ProjectNameLookup.h:28-35 hacia include/nativesharp/Frontend/NamedTypeDeclarationKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0408 — Extraer el enum NamedTypeAccessibility desde
  include/nativesharp/Frontend/ProjectNameLookup.h:46-53 hacia include/nativesharp/Frontend/NamedTypeAccessibility.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0409 — Extraer el struct NamedTypeDeclarationPart desde
  include/nativesharp/Frontend/ProjectNameLookup.h:55-75 hacia include/nativesharp/Frontend/NamedTypeDeclarationPart.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0410 — Extraer el struct NamedTypeDeclaration desde
  include/nativesharp/Frontend/ProjectNameLookup.h:77-97 hacia include/nativesharp/Frontend/NamedTypeDeclaration.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0411 — Extraer el enum NamespaceFunctionAccessibility desde
  include/nativesharp/Frontend/ProjectNameLookup.h:99-103 hacia
  include/nativesharp/Frontend/NamespaceFunctionAccessibility.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0412 — Extraer el struct NamespaceFunctionDeclaration desde
  include/nativesharp/Frontend/ProjectNameLookup.h:105-119 hacia
  include/nativesharp/Frontend/NamespaceFunctionDeclaration.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0413 — Extraer el struct NamespaceStateDeclaration desde
  include/nativesharp/Frontend/ProjectNameLookup.h:121-135 hacia
  include/nativesharp/Frontend/NamespaceStateDeclaration.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0414 — Extraer el enum TypeNameBindingKind desde include/nativesharp/Frontend/ProjectNameLookup.h:136-142
  hacia include/nativesharp/Frontend/TypeNameBindingKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0415 — Extraer el struct TypeNameBinding desde include/nativesharp/Frontend/ProjectNameLookup.h:144-160
  hacia include/nativesharp/Frontend/TypeNameBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0416 — Extraer el struct ExpressionTypeNameBinding desde
  include/nativesharp/Frontend/ProjectNameLookup.h:162-173 hacia
  include/nativesharp/Frontend/ExpressionTypeNameBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0417 — Extraer el class ModuleDeclarationSpace desde
  include/nativesharp/Frontend/ProjectNameLookup.h:175-197 hacia include/nativesharp/Frontend/ModuleDeclarationSpace.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0418 — Conservar el class ProjectNameLookup como tipo principal de
  include/nativesharp/Frontend/ProjectNameLookup.h:199-364 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0419 — Extraer el struct NamespaceFunctionPart desde
  include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h:23-32 hacia
  include/nativesharp/Frontend/NamespaceFunctionPart.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0420 — Extraer el struct NamespaceFunctionSymbol desde
  include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h:34-76 hacia
  include/nativesharp/Frontend/NamespaceFunctionSymbol.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0421 — Extraer el struct PrecompiledNamespaceFunctionIdentity desde
  include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h:78-85 hacia
  include/nativesharp/Frontend/PrecompiledNamespaceFunctionIdentity.h; conservar texto, comentarios, orden de
  declaración y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0422 — Extraer el enum NamespaceStateInitializerKind desde
  include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h:87-95 hacia
  include/nativesharp/Frontend/NamespaceStateInitializerKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0423 — Extraer el struct NamespaceStateSymbol desde
  include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h:97-121 hacia
  include/nativesharp/Frontend/NamespaceStateSymbol.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0424 — Extraer el struct NamespaceFunctionReferenceBinding desde
  include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h:123-135 hacia
  include/nativesharp/Frontend/NamespaceFunctionReferenceBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0425 — Extraer el struct NamespaceStateReferenceBinding desde
  include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h:137-146 hacia
  include/nativesharp/Frontend/NamespaceStateReferenceBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0426 — Conservar el class ProjectNamespaceFunctionModel como tipo principal de
  include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h:148-195 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0427 — Extraer el struct ObjectFieldLayout desde
  include/nativesharp/Frontend/ProjectObjectLayoutModel.h:26-35 hacia include/nativesharp/Frontend/ObjectFieldLayout.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0428 — Extraer el struct StructValueLayout desde
  include/nativesharp/Frontend/ProjectObjectLayoutModel.h:38-53 hacia include/nativesharp/Frontend/StructValueLayout.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0429 — Extraer el struct ClassObjectLayout desde
  include/nativesharp/Frontend/ProjectObjectLayoutModel.h:55-67 hacia include/nativesharp/Frontend/ClassObjectLayout.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0430 — Extraer el enum ObjectInitializationStepKind desde
  include/nativesharp/Frontend/ProjectObjectLayoutModel.h:69-78 hacia
  include/nativesharp/Frontend/ObjectInitializationStepKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0431 — Extraer el struct ObjectInitializationStep desde
  include/nativesharp/Frontend/ProjectObjectLayoutModel.h:80-84 hacia
  include/nativesharp/Frontend/ObjectInitializationStep.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0432 — Extraer el struct ObjectAllocationPlan desde
  include/nativesharp/Frontend/ProjectObjectLayoutModel.h:86-101 hacia
  include/nativesharp/Frontend/ObjectAllocationPlan.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0433 — Conservar el class ProjectObjectLayoutModel como tipo principal de
  include/nativesharp/Frontend/ProjectObjectLayoutModel.h:103-182 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0458 — Extraer el enum ProjectConversionKind desde
  include/nativesharp/Frontend/ProjectTypeConversion.h:13-35 hacia include/nativesharp/Frontend/ProjectConversionKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [x] NSC-IMPL-0459 — Extraer el struct ProjectIntegerConstant desde
  include/nativesharp/Frontend/ProjectTypeConversion.h:37-46 hacia
  include/nativesharp/Frontend/ProjectIntegerConstant.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0460 — Extraer el struct ProjectImplicitConversion desde
  include/nativesharp/Frontend/ProjectTypeConversion.h:48-57 hacia
  include/nativesharp/Frontend/ProjectImplicitConversion.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0461 — Conservar el class ProjectTypeConversion como tipo principal de
  include/nativesharp/Frontend/ProjectTypeConversion.h:59-88 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0462 — Extraer el struct VirtualMethodInfo desde
  include/nativesharp/Frontend/ProjectVirtualDispatchModel.h:19-25 hacia
  include/nativesharp/Frontend/VirtualMethodInfo.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0463 — Extraer el struct ClassVirtualTable desde
  include/nativesharp/Frontend/ProjectVirtualDispatchModel.h:27-35 hacia
  include/nativesharp/Frontend/ClassVirtualTable.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0464 — Conservar el class ProjectVirtualDispatchModel como tipo principal de
  include/nativesharp/Frontend/ProjectVirtualDispatchModel.h:37-69 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0465 — Extraer el struct QualifiedNameSite desde include/nativesharp/Frontend/QualifiedNameLadder.h:41-54
  hacia include/nativesharp/Frontend/QualifiedNameSite.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0466 — Extraer el enum QualifiedNameTier desde include/nativesharp/Frontend/QualifiedNameLadder.h:58-71
  hacia include/nativesharp/Frontend/QualifiedNameTier.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0467 — Extraer el struct QualifiedNameRung desde include/nativesharp/Frontend/QualifiedNameLadder.h:76-80
  hacia include/nativesharp/Frontend/QualifiedNameRung.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0468 — Extraer el enum StageBoundary desde include/nativesharp/Frontend/StageBoundaryVerifier.h:15-23
  hacia include/nativesharp/Frontend/StageBoundary.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0469 — Conservar el class StageBoundaryVerifier como tipo principal de
  include/nativesharp/Frontend/StageBoundaryVerifier.h:29-48 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [x] NSC-IMPL-0859 — Descomponer ProjectMemberModel::collectTypeMembers en src/Frontend/ProjectMemberModel.cpp:1773
  (1025 líneas ejecutables; 1490 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0866 — Descomponer ProjectNamespaceFunctionModel::build en
  src/Frontend/ProjectNamespaceFunctionModel.cpp:327 (713 líneas ejecutables; 966 totales) hasta que cada función quede
  en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0887 — Descomponer ProjectConstructionModel::bindConstruction en
  src/Frontend/ProjectConstructionModel.cpp:949 (314 líneas ejecutables; 444 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0889 — Descomponer ProjectOpaqueResultModel::build en src/Frontend/ProjectOpaqueResultModel.cpp:529 (309
  líneas ejecutables; 386 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0894 — Descomponer ProjectConstructionModel::bindConstructorInitializer en
  src/Frontend/ProjectConstructionModel.cpp:483 (280 líneas ejecutables; 389 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0897 — Descomponer ProjectMetaprogrammingModel::build en src/Frontend/ProjectMetaprogrammingModel.cpp:48
  (278 líneas ejecutables; 381 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0903 — Descomponer ProjectGenericConstraintModel::build en
  src/Frontend/ProjectGenericConstraintModel.cpp:612 (266 líneas ejecutables; 384 totales) hasta que cada función quede
  en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0909 — Descomponer ProjectMemberModel::build en src/Frontend/ProjectMemberModel.cpp:540 (240 líneas
  ejecutables; 384 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-0910 — Descomponer ProjectCompilation::compileSyntaxCore en src/Frontend/ProjectCompilation.cpp:932 (233
  líneas ejecutables; 325 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0911 — Descomponer ProjectObjectLayoutModel::buildStructLayout en
  src/Frontend/ProjectObjectLayoutModel.cpp:605 (231 líneas ejecutables; 325 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0913 — Descomponer ProjectGenericModel::inferGenericCall en src/Frontend/ProjectGenericModel.cpp:5230
  (228 líneas ejecutables; 321 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0914 — Descomponer ProjectGenericModel::specializeGenericLambdaCall en
  src/Frontend/ProjectGenericModel.cpp:3365 (225 líneas ejecutables; 305 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0915 — Descomponer ProjectInterfaceModel::buildClassPlans en src/Frontend/ProjectInterfaceModel.cpp:708
  (225 líneas ejecutables; 314 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0916 — Descomponer ProjectNameLookup::bindTypeSyntax en src/Frontend/ProjectNameLookup.cpp:1583 (222
  líneas ejecutables; 353 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0918 — Descomponer ProjectVirtualDispatchModel::buildClass en
  src/Frontend/ProjectVirtualDispatchModel.cpp:92 (218 líneas ejecutables; 297 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0919 — Descomponer ProjectNameLookup::finalizePredeclarations en src/Frontend/ProjectNameLookup.cpp:764
  (217 líneas ejecutables; 328 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0920 — Descomponer ProjectGenericModel::collectAssociatedItems en
  src/Frontend/ProjectGenericModel.cpp:1313 (213 líneas ejecutables; 267 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0922 — Descomponer ProjectGenericModel::build en src/Frontend/ProjectGenericModel.cpp:738 (206 líneas
  ejecutables; 358 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-0923 — Descomponer ProjectInterfaceModel::visitInterface en src/Frontend/ProjectInterfaceModel.cpp:408
  (205 líneas ejecutables; 299 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0926 — Descomponer ComptimeEngine::execute en src/Frontend/ComptimeEngine.cpp:703 (198 líneas
  ejecutables; 261 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-0937 — Descomponer ProjectGenericModel::normalizeGenericArguments en
  src/Frontend/ProjectGenericModel.cpp:4839 (182 líneas ejecutables; 241 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0952 — Descomponer ProjectGenericModel::collectTemplates en src/Frontend/ProjectGenericModel.cpp:3152
  (158 líneas ejecutables; 212 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0966 — Descomponer ProjectGenericModel::inferExpressionType en src/Frontend/ProjectGenericModel.cpp:4514
  (140 líneas ejecutables; 182 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0969 — Descomponer ProjectGenericConstraintModel::validateTemplateClauses en
  src/Frontend/ProjectGenericConstraintModel.cpp:284 (138 líneas ejecutables; 193 totales) hasta que cada función quede
  en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0971 — Descomponer ProjectObjectLayoutModel::buildClassLayout en
  src/Frontend/ProjectObjectLayoutModel.cpp:940 (138 líneas ejecutables; 198 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0982 — Descomponer ProjectConversionClassifier::classifyImplicit en
  src/Frontend/ProjectConversionClassifier.cpp:28 (128 líneas ejecutables; 193 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0985 — Descomponer ProjectMemberModel::canonicalTypeKey en src/Frontend/ProjectMemberModel.cpp:1364 (126
  líneas ejecutables; 206 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0987 — Descomponer ProjectGenericModel::executeTypeDemandWaves en
  src/Frontend/ProjectGenericModel.cpp:4229 (124 líneas ejecutables; 167 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0992 — Descomponer ProjectGenericModel::ensureMethodSpecialization en
  src/Frontend/ProjectGenericModel.cpp:6524 (120 líneas ejecutables; 154 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0993 — Descomponer ProjectMemberModelDumper::dump en src/Frontend/ProjectMemberModelDumper.cpp:10 (120
  líneas ejecutables; 160 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0994 — Descomponer ProjectNameLookup::predeclareNode en src/Frontend/ProjectNameLookup.cpp:587 (120
  líneas ejecutables; 176 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-0998 — Descomponer ProjectGenericModel::mergeTraitImplementations en
  src/Frontend/ProjectGenericModel.cpp:1097 (116 líneas ejecutables; 150 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1003 — Descomponer ProjectCallableResultMerger::merge en src/Frontend/ProjectCallableResultMerger.cpp:195
  (113 líneas ejecutables; 133 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1004 — Descomponer ProjectGenericModel::processNodeOnce en src/Frontend/ProjectGenericModel.cpp:3764 (113
  líneas ejecutables; 174 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1010 — Descomponer ProjectGenericModel::inferTypeArguments en src/Frontend/ProjectGenericModel.cpp:4697
  (109 líneas ejecutables; 141 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1012 — Descomponer ProjectGenericModel::cloneType en src/Frontend/ProjectGenericModel.cpp:7016 (108
  líneas ejecutables; 183 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1016 — Descomponer ProjectInheritanceModel::resolveBaseSpecifications en
  src/Frontend/ProjectInheritanceModel.cpp:296 (104 líneas ejecutables; 176 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1017 — Descomponer ProjectMemberModel::collectAnonymousMemberPromotions en
  src/Frontend/ProjectMemberModel.cpp:3264 (104 líneas ejecutables; 143 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1023 — Descomponer collectTypeSeedsInScope en src/Frontend/ProjectCompilation.cpp:270 (102 líneas
  ejecutables; 181 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1024 — Descomponer MacroExpansionEngine::expand en src/Frontend/MacroExpansionEngine.cpp:157 (101 líneas
  ejecutables; 119 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1025 — Descomponer ProjectNameLookup::resolveTypeCandidates en src/Frontend/ProjectNameLookup.cpp:1968
  (101 líneas ejecutables; 180 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1031 — Descomponer ProjectNameLookup::build en src/Frontend/ProjectNameLookup.cpp:313 (100 líneas
  ejecutables; 134 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1032 — Descomponer ProjectNameLookup::bindNode en src/Frontend/ProjectNameLookup.cpp:1361 (100 líneas
  ejecutables; 194 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1042 — Descomponer ProjectGenericModel::foldValuePack en src/Frontend/ProjectGenericModel.cpp:7234 (92
  líneas ejecutables; 117 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1043 — Descomponer ProjectNameLookup::finalizeReexports en src/Frontend/ProjectNameLookup.cpp:1093 (92
  líneas ejecutables; 130 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1047 — Descomponer ProjectMemberModel::collectEnumMembers en src/Frontend/ProjectMemberModel.cpp:3579 (91
  líneas ejecutables; 143 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1053 — Descomponer deserializeTypeSyntaxTree en src/Frontend/ProjectGenericModel.cpp:2453 (89 líneas
  ejecutables; 99 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1065 — Descomponer ProjectGenericModel::expandTypeAliases en src/Frontend/ProjectGenericModel.cpp:1662
  (86 líneas ejecutables; 107 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1066 — Descomponer ProjectTypeConversion::classifyImplicit en src/Frontend/ProjectTypeConversion.cpp:332
  (86 líneas ejecutables; 146 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1073 — Descomponer ProjectGenericConstraintModel::validateControlledSpecializations en
  src/Frontend/ProjectGenericConstraintModel.cpp:478 (84 líneas ejecutables; 112 totales) hasta que cada función quede
  en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1078 — Descomponer ProjectGenericModel::processTypeSyntax en src/Frontend/ProjectGenericModel.cpp:4397
  (83 líneas ejecutables; 116 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1079 — Descomponer validateDelegateLifetimeContract en src/Frontend/ProjectMemberModel.cpp:183 (83 líneas
  ejecutables; 111 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1080 — Descomponer ProjectNameLookup::bindScope en src/Frontend/ProjectNameLookup.cpp:1233 (83 líneas
  ejecutables; 127 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1081 — Descomponer ProjectGenericModel::cloneTypeGeneric en src/Frontend/ProjectGenericModel.cpp:6898 (82
  líneas ejecutables; 117 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1090 — Descomponer FrontendCompilation::run en src/Frontend/FrontendCompilation.cpp:113 (80 líneas
  ejecutables; 136 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1091 — Descomponer ProjectGenericModel::cloneProjectionType en src/Frontend/ProjectGenericModel.cpp:6791
  (80 líneas ejecutables; 106 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1104 — Descomponer ProjectGenericModel::processNode en src/Frontend/ProjectGenericModel.cpp:3939 (77
  líneas ejecutables; 106 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1105 — Descomponer ProjectGenericModel::ensureTypeSpecialization en
  src/Frontend/ProjectGenericModel.cpp:6421 (77 líneas ejecutables; 102 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1106 — Descomponer ProjectGenericModel::cloneNodeGeneric en src/Frontend/ProjectGenericModel.cpp:7378 (77
  líneas ejecutables; 100 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1120 — Descomponer ProjectConstructionModel::bindImplicitDefaultConstructor en
  src/Frontend/ProjectConstructionModel.cpp:363 (75 líneas ejecutables; 119 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1127 — Descomponer ProjectIntegerEvaluator::evaluateBinary en
  src/Frontend/ProjectIntegerEvaluator.cpp:228 (74 líneas ejecutables; 139 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1129 — Descomponer ProjectGenericModel::expandAliasesInNode en src/Frontend/ProjectGenericModel.cpp:1770
  (73 líneas ejecutables; 102 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1130 — Descomponer ProjectGenericModel::cloneNode en src/Frontend/ProjectGenericModel.cpp:7560 (73 líneas
  ejecutables; 95 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1139 — Descomponer ProjectGenericModel::resolveDeclaredTypeIdentity en
  src/Frontend/ProjectGenericModel.cpp:7777 (71 líneas ejecutables; 104 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1146 — Descomponer resolveCapabilityCalls en src/Frontend/ComptimeEngine.cpp:560 (69 líneas ejecutables;
  83 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1147 — Descomponer deserializeTypeSyntaxScalars en src/Frontend/ProjectGenericModel.cpp:2357 (69 líneas
  ejecutables; 73 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1155 — Descomponer parseAuthoredDiagnostic en src/Frontend/ComptimeEngine.cpp:451 (68 líneas ejecutables;
  81 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1170 — Descomponer TemplateParseCache::prune en src/Frontend/TemplateParseCache.cpp:287 (64 líneas
  ejecutables; 85 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1178 — Descomponer ProjectDestructionModel::buildClass en src/Frontend/ProjectDestructionModel.cpp:78 (63
  líneas ejecutables; 96 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1179 — Descomponer constArgumentExpression en src/Frontend/ProjectGenericModel.cpp:451 (63 líneas
  ejecutables; 76 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [x] NSC-IMPL-1189 — Descomponer accumulateNodeCost en src/Frontend/ProjectCostExplorer.cpp:76 (62 líneas ejecutables;
  75 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [x] NSC-IMPL-1190 — Descomponer ProjectGenericModel::discoverTemplateDependencies en
  src/Frontend/ProjectGenericModel.cpp:7994 (62 líneas ejecutables; 89 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1191 — Descomponer ProjectGenericModel::collectTemplateDependencies en
  src/Frontend/ProjectGenericModel.cpp:8199 (62 líneas ejecutables; 79 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1198 — Descomponer ProjectGenericModel::inferLambdaTypeArguments en
  src/Frontend/ProjectGenericModel.cpp:5152 (61 líneas ejecutables; 77 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1199 — Descomponer ProjectGenericModel::typeSpelling en src/Frontend/ProjectGenericModel.cpp:7656 (61
  líneas ejecutables; 78 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [x] NSC-IMPL-1208 — Partir ProjectGenericModel.cpp según los rangos de la guía y dejar ProjectGenericModel como
  fachada sin mover sus 41 campos.
- [x] NSC-IMPL-1215 — Convertir ProjectMemberModel en fachada con colaboradores por familia, sin mover campos ni cambiar
  collectTypeMembers.
- [x] NSC-IMPL-1216 — Convertir ProjectNameLookup en fachada con colaboradores por familia, sin mover campos ni cambiar
  resolución u orden.
- [x] NSC-IMPL-1217 — Convertir ProjectConstructionModel en fachada con colaboradores por familia, sin mover campos ni
  cambiar binding.

Nota de cierre REF-03: por instrucción explícita del propietario, los colaboradores de NSC-IMPL-1215,
NSC-IMPL-1216 y NSC-IMPL-1217 se materializaron como unidades de traducción por familia de métodos de la clase
existente, sin introducir clases conceptuales nuevas, mover campos ni alterar firmas, algoritmos o flujo.

### Commit REF-04 — Análisis de cuerpos de métodos — 0/107

Aísla las familias de análisis de cuerpos conservando firmas, estado, orden y resultados diagnósticos.

- [ ] NSC-IMPL-0052 — Extraer el struct NamedArgumentMap desde src/Frontend/Bodies/CallAnalyzer.cpp:49-58 (10) hacia
  src/Frontend/Bodies/Internal/NamedArgumentMap.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0073 — Extraer el enum Stage37ConstantKind desde src/Frontend/ProjectMethodBodyModel.cpp:346-352 (7)
  hacia src/Frontend/Internal/Stage37ConstantKind.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0074 — Extraer el struct Stage37ConstantValue desde src/Frontend/ProjectMethodBodyModel.cpp:354-379 (26)
  hacia src/Frontend/Internal/Stage37ConstantValue.h, y mover sus cuerpos a
  src/Frontend/Internal/Stage37ConstantValue.cpp; conservar texto, comentarios, firmas, orden y conducta. Restricción de
  pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0135 — Mover fuera de línea función libre traceMutex de src/Frontend/Bodies/MethodBodyInternals.h:18 (5
  líneas) a un único .cpp, dejando la declaración intacta y conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0136 — Mover fuera de línea función libre resetMethodBodyTrace de
  src/Frontend/Bodies/MethodBodyInternals.h:24 (9 líneas) a un único .cpp, dejando la declaración intacta y conservando
  exactamente el cuerpo.
- [ ] NSC-IMPL-0137 — Mover fuera de línea función libre writeMethodBodyTrace de
  src/Frontend/Bodies/MethodBodyInternals.h:34 (11 líneas) a un único .cpp, dejando la declaración intacta y conservando
  exactamente el cuerpo.
- [ ] NSC-IMPL-0138 — Mover fuera de línea función libre pointerContract de src/Frontend/Bodies/MethodBodyInternals.h:46
  (14 líneas) a un único .cpp, dejando la declaración intacta y conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0179 — Mover fuera de línea método ExpressionAnalysisRequest::optionMask de
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1745 (8 líneas) a un único .cpp, dejando la declaración intacta
  y conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0180 — Mover fuera de línea método ExpressionAnalysisCacheKeyHash::operator de
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1765 (9 líneas) a un único .cpp, dejando la declaración intacta
  y conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0181 — Mover fuera de línea método ConstantMemberResolver::ConstantMemberResolver de
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1992 (7 líneas) a un único .cpp, dejando la declaración intacta
  y conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0351 — Extraer el enum MethodScopeKind desde include/nativesharp/Frontend/ProjectMethodBodyModel.h:55-66
  hacia include/nativesharp/Frontend/MethodScopeKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0352 — Extraer el struct MethodScope desde include/nativesharp/Frontend/ProjectMethodBodyModel.h:68-75
  hacia include/nativesharp/Frontend/MethodScope.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0353 — Extraer el struct MethodLocalSymbol desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:77-185 hacia include/nativesharp/Frontend/MethodLocalSymbol.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0354 — Extraer el enum MethodNameBindingKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:187-209 hacia
  include/nativesharp/Frontend/MethodNameBindingKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0355 — Extraer el struct MethodNameBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:211-226 hacia include/nativesharp/Frontend/MethodNameBinding.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0356 — Extraer el enum MethodShadowingKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:228-234 hacia
  include/nativesharp/Frontend/MethodShadowingKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0357 — Extraer el struct MethodShadowingEvent desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:236-243 hacia
  include/nativesharp/Frontend/MethodShadowingEvent.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0358 — Extraer el enum OwnershipArgumentFit desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:258-269 hacia
  include/nativesharp/Frontend/OwnershipArgumentFit.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0359 — Extraer el struct MethodAggregateInitializerEntryBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:295-308 hacia
  include/nativesharp/Frontend/MethodAggregateInitializerEntryBinding.h; conservar texto, comentarios, orden de
  declaración y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0360 — Extraer el struct MethodAggregateInitializerBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:310-316 hacia
  include/nativesharp/Frontend/MethodAggregateInitializerBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0361 — Extraer el struct MethodDeconstructionBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:322-329 hacia
  include/nativesharp/Frontend/MethodDeconstructionBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0362 — Extraer el struct MethodMatchExpressionBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:331-345 hacia
  include/nativesharp/Frontend/MethodMatchExpressionBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0363 — Extraer el struct MethodLoopExpressionBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:350-357 hacia
  include/nativesharp/Frontend/MethodLoopExpressionBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0364 — Extraer el struct MethodLoopBreakBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:359-365 hacia
  include/nativesharp/Frontend/MethodLoopBreakBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0365 — Extraer el enum MethodRuntimeTypeOperationKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:367-372 hacia
  include/nativesharp/Frontend/MethodRuntimeTypeOperationKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0366 — Extraer el struct MethodRuntimeTypeBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:374-384 hacia
  include/nativesharp/Frontend/MethodRuntimeTypeBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0367 — Extraer el enum MethodExceptionOperationKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:387-392 hacia
  include/nativesharp/Frontend/MethodExceptionOperationKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0368 — Extraer el struct MethodExceptionBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:394-410 hacia
  include/nativesharp/Frontend/MethodExceptionBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0369 — Extraer el enum MethodCleanupRegionKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:412-420 hacia
  include/nativesharp/Frontend/MethodCleanupRegionKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0370 — Extraer el struct MethodCleanupRegion desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:422-435 hacia
  include/nativesharp/Frontend/MethodCleanupRegion.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0371 — Extraer el struct MethodMoveBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:439-479 hacia include/nativesharp/Frontend/MethodMoveBinding.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0372 — Extraer el enum MethodOwnershipTransferKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:481-489 hacia
  include/nativesharp/Frontend/MethodOwnershipTransferKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0373 — Extraer el struct MethodOwnershipTransferBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:491-502 hacia
  include/nativesharp/Frontend/MethodOwnershipTransferBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0374 — Extraer el enum MethodBorrowKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:504-518 hacia include/nativesharp/Frontend/MethodBorrowKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0375 — Extraer el struct MethodBorrowBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:520-531 hacia
  include/nativesharp/Frontend/MethodBorrowBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0376 — Extraer el enum MethodLocalBorrowSourceKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:533-539 hacia
  include/nativesharp/Frontend/MethodLocalBorrowSourceKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0377 — Extraer el enum MethodBorrowState desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:541-546 hacia include/nativesharp/Frontend/MethodBorrowState.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0378 — Extraer el struct MethodLocalBorrowBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:548-566 hacia
  include/nativesharp/Frontend/MethodLocalBorrowBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0379 — Extraer el struct MethodBorrowedParameterReassignmentBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:568-575 hacia
  include/nativesharp/Frontend/MethodBorrowedParameterReassignmentBinding.h; conservar texto, comentarios, orden de
  declaración y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0380 — Extraer el struct MethodUsingBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:577-589 hacia include/nativesharp/Frontend/MethodUsingBinding.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0381 — Extraer el struct MethodSwitchSectionBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:594-599 hacia
  include/nativesharp/Frontend/MethodSwitchSectionBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0382 — Extraer el struct MethodSwitchBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:601-608 hacia
  include/nativesharp/Frontend/MethodSwitchBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0383 — Extraer el enum MethodStructuredExitKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:610-615 hacia
  include/nativesharp/Frontend/MethodStructuredExitKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0384 — Extraer el struct MethodStructuredExitBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:617-625 hacia
  include/nativesharp/Frontend/MethodStructuredExitBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0385 — Extraer el enum MethodCleanupContinuationKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:627-634 hacia
  include/nativesharp/Frontend/MethodCleanupContinuationKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0386 — Extraer el struct MethodCleanupContinuation desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:636-645 hacia
  include/nativesharp/Frontend/MethodCleanupContinuation.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0387 — Extraer el enum MethodCoreOperationKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:647-1173 hacia
  include/nativesharp/Frontend/MethodCoreOperationKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0388 — Extraer el struct MethodCoreBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1175-1234 hacia
  include/nativesharp/Frontend/MethodCoreBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0389 — Extraer el struct MethodVariableArrayIndexBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1236-1240 hacia
  include/nativesharp/Frontend/MethodVariableArrayIndexBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0390 — Extraer el enum MethodLambdaCaptureKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1244-1249 hacia
  include/nativesharp/Frontend/MethodLambdaCaptureKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0391 — Extraer el enum MethodLambdaCaptureSourceKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1251-1259 hacia
  include/nativesharp/Frontend/MethodLambdaCaptureSourceKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0392 — Extraer el struct MethodLambdaCapture desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1261-1277 hacia
  include/nativesharp/Frontend/MethodLambdaCapture.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0393 — Extraer el struct MethodLambdaValueBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1279-1289 hacia
  include/nativesharp/Frontend/MethodLambdaValueBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0394 — Extraer el struct MethodLambdaBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1291-1313 hacia
  include/nativesharp/Frontend/MethodLambdaBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0395 — Extraer el enum MethodDelegateOperationKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1315-1319 hacia
  include/nativesharp/Frontend/MethodDelegateOperationKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0396 — Extraer el struct MethodDelegateBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1321-1356 hacia
  include/nativesharp/Frontend/MethodDelegateBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0397 — Extraer el enum MethodEventOperationKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1358-1363 hacia
  include/nativesharp/Frontend/MethodEventOperationKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0398 — Extraer el struct MethodEventBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1365-1384 hacia
  include/nativesharp/Frontend/MethodEventBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0399 — Extraer el struct MethodOptionalCarrierBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1388-1397 hacia
  include/nativesharp/Frontend/MethodOptionalCarrierBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0400 — Extraer el struct MethodResultPropagationBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1402-1423 hacia
  include/nativesharp/Frontend/MethodResultPropagationBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0401 — Extraer el struct MethodRangeSliceBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1430-1452 hacia
  include/nativesharp/Frontend/MethodRangeSliceBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0402 — Extraer el enum MethodCallResolutionKind desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1454-1459 hacia
  include/nativesharp/Frontend/MethodCallResolutionKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0403 — Extraer el struct MethodCallCandidate desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1461-1483 hacia
  include/nativesharp/Frontend/MethodCallCandidate.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0404 — Extraer el struct MethodCallBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1485-1517 hacia
  include/nativesharp/Frontend/MethodCallBinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0405 — Extraer el struct NamespaceFunctionCallBinding desde
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1519-1542 hacia
  include/nativesharp/Frontend/NamespaceFunctionCallBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0406 — Conservar el class ProjectMethodBodyModel como tipo principal de
  include/nativesharp/Frontend/ProjectMethodBodyModel.h:1544-2729 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0853 — Descomponer ProjectMethodBodyModel::analyzeExpressionCore en
  src/Frontend/ProjectMethodBodyModel.cpp:1619 (3062 líneas ejecutables; 4311 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0855 — Descomponer ProjectMethodBodyModel::analyzeCall en src/Frontend/Bodies/CallAnalyzer.cpp:731 (2560
  líneas ejecutables; 3728 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control
  de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0857 — Descomponer ProjectMethodBodyModel::analyzeAssignment en
  src/Frontend/Bodies/AssignmentAnalyzer.cpp:58 (1341 líneas ejecutables; 1991 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0860 — Descomponer ProjectMethodBodyModel::analyzeBlock en src/Frontend/Bodies/StatementAnalyzer.cpp:120
  (1005 líneas ejecutables; 1438 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0862 — Descomponer ProjectMethodBodyModel::analyzeMember en
  src/Frontend/Bodies/MemberAccessAnalyzer.cpp:62 (842 líneas ejecutables; 1101 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0864 — Descomponer ProjectMethodBodyModel::analyzeVariable en
  src/Frontend/Bodies/StatementAnalyzer.cpp:3221 (760 líneas ejecutables; 1109 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0868 — Descomponer ProjectMethodBodyModel::analyzeReturn en
  src/Frontend/Bodies/StatementAnalyzer.cpp:4780 (569 líneas ejecutables; 792 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0870 — Descomponer ProjectMethodBodyModel::analyzeStatement en
  src/Frontend/Bodies/StatementAnalyzer.cpp:1559 (528 líneas ejecutables; 750 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0871 — Descomponer ProjectMethodBodyModel::analyzeIdentifier en
  src/Frontend/ProjectMethodBodyModel.cpp:5931 (521 líneas ejecutables; 693 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0876 — Descomponer ProjectMethodBodyModel::analyzeBinary en src/Frontend/ProjectMethodBodyModel.cpp:7019
  (468 líneas ejecutables; 719 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0877 — Descomponer ProjectMethodBodyModel::build en src/Frontend/Bodies/MethodBodyBuildCoordinator.cpp:28
  (409 líneas ejecutables; 584 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0879 — Descomponer ProjectMethodBodyModel::analyzeMatchExpression en
  src/Frontend/Bodies/PatternAnalyzer.cpp:157 (368 líneas ejecutables; 520 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0880 — Descomponer ProjectMethodBodyModel::bindAggregateInitializer en
  src/Frontend/Bodies/ConstructionAnalyzer.cpp:364 (361 líneas ejecutables; 482 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0882 — Descomponer ProjectMethodBodyModelDumper::dump en src/Frontend/ProjectMethodBodyModelDumper.cpp:23
  (347 líneas ejecutables; 508 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0883 — Descomponer ProjectMethodBodyModel::collectLambdaCaptures en
  src/Frontend/ProjectMethodBodyModel.cpp:8342 (345 líneas ejecutables; 449 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0884 — Descomponer ProjectMethodBodyModel::analyzeNamespaceFunctionCall en
  src/Frontend/Bodies/CallAnalyzer.cpp:297 (340 líneas ejecutables; 433 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0888 — Descomponer ProjectMethodBodyModel::bindLambdaToDelegate en
  src/Frontend/ProjectMethodBodyModel.cpp:8792 (309 líneas ejecutables; 418 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0896 — Descomponer ProjectMethodBodyModel::isCompileTimeConstantExpression en
  src/Frontend/ProjectMethodBodyModel.cpp:7819 (279 líneas ejecutables; 396 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0938 — Descomponer ProjectMethodBodyModel::analyzeUnary en src/Frontend/ProjectMethodBodyModel.cpp:6670
  (180 líneas ejecutables; 300 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0939 — Descomponer methodCoreOperationKindName en src/Frontend/ProjectMethodBodyModel.cpp:10443 (179
  líneas ejecutables; 184 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0941 — Descomponer ProjectMethodBodyModel::analyzeLoopExpression en
  src/Frontend/ProjectMethodBodyModel.cpp:1389 (172 líneas ejecutables; 228 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0942 — Descomponer ProjectMethodBodyModel::resolveSyntheticCall en
  src/Frontend/Bodies/CallAnalyzer.cpp:4460 (172 líneas ejecutables; 241 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0946 — Descomponer ProjectMethodBodyModel::storagePlaceKey en
  src/Frontend/Bodies/OwnershipAnalyzer.cpp:209 (165 líneas ejecutables; 251 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0955 — Descomponer ProjectMethodBodyModel::joinOwnershipFlow en
  src/Frontend/Bodies/BodyScopeAndFlow.cpp:301 (156 líneas ejecutables; 202 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0959 — Descomponer ProjectMethodBodyModel::finalizeEscapeSummaries en
  src/Frontend/Bodies/EscapeAnalyzer.cpp:96 (147 líneas ejecutables; 200 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0961 — Descomponer ProjectMethodBodyModel::analyzeFor en src/Frontend/Bodies/StatementAnalyzer.cpp:4331
  (145 líneas ejecutables; 211 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0970 — Descomponer ProjectMethodBodyModel::bindMethodGroupToDelegate en
  src/Frontend/ProjectMethodBodyModel.cpp:9211 (138 líneas ejecutables; 186 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0974 — Descomponer ProjectMethodBodyModel::analyzeTry en src/Frontend/Bodies/StatementAnalyzer.cpp:2932
  (137 líneas ejecutables; 200 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1014 — Descomponer ProjectMethodBodyModel::beginUsing en src/Frontend/Bodies/StatementAnalyzer.cpp:2511
  (106 líneas ejecutables; 150 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1015 — Descomponer ProjectMethodBodyModel::beginSwitch en src/Frontend/Bodies/StatementAnalyzer.cpp:5719
  (106 líneas ejecutables; 166 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1033 — Descomponer ProjectMethodBodyModel::analyzeLoopExit en
  src/Frontend/Bodies/StatementAnalyzer.cpp:5936 (99 líneas ejecutables; 148 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1036 — Descomponer ProjectMethodBodyModel::prepareNextCatch en
  src/Frontend/Bodies/StatementAnalyzer.cpp:2741 (97 líneas ejecutables; 136 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1048 — Descomponer ProjectMethodBodyModel::resolveResultCarrier en
  src/Frontend/ProjectMethodBodyModel.cpp:9654 (91 líneas ejecutables; 113 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1054 — Descomponer mapArgumentsToParameters en src/Frontend/Bodies/CallAnalyzer.cpp:60 (89 líneas
  ejecutables; 143 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1055 — Descomponer ProjectMethodBodyModel::bindDeconstructionSource en
  src/Frontend/Bodies/PatternAnalyzer.cpp:41 (88 líneas ejecutables; 115 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1114 — Descomponer ProjectMethodBodyModel::beginForeach en src/Frontend/Bodies/StatementAnalyzer.cpp:4576
  (76 líneas ejecutables; 151 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1121 — Descomponer ProjectMemberModel::scanMethodBody en src/Frontend/ProjectMemberModel.cpp:3723 (75
  líneas ejecutables; 115 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1137 — Descomponer ProjectMethodBodyModel::aggregateSupportsSynthesizedMember en
  src/Frontend/Bodies/ConstructionAnalyzer.cpp:151 (72 líneas ejecutables; 110 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1161 — Descomponer ProjectMethodBodyModel::restoreOwnershipFlow en
  src/Frontend/Bodies/BodyScopeAndFlow.cpp:187 (67 líneas ejecutables; 93 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1163 — Descomponer ProjectMethodBodyModel::placesOverlap en src/Frontend/Bodies/OwnershipAnalyzer.cpp:469
  (66 líneas ejecutables; 90 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control
  de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1206 — Convertir ProjectMethodBodyModel en fachada con colaboradores por familia, forwarders de una línea
  y todos sus campos en el dueño.

### Commit REF-05 — HIR, MIR, CFG y dominancia — 0/97

Modulariza las representaciones y máquinas HIR/MIR junto con sus soportes de CFG y dominancia.

- [ ] NSC-IMPL-0010 — Extraer el struct DfsFrame desde src/CFG/CfgTraversal.cpp:15-19 (5) hacia
  src/CFG/Internal/DfsFrame.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta. Restricción
  de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0018 — Extraer el struct TreeFrame desde src/Dominance/DominanceBuilder.cpp:24-29 (6) hacia
  src/Dominance/Internal/TreeFrame.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0054 — Extraer el enum StaticAssertConstantKind desde src/Frontend/HIR/HirStatementMachine.cpp:22-27 (6)
  hacia src/Frontend/HIR/Internal/StaticAssertConstantKind.h, sin .cpp adicional; conservar texto, comentarios, firmas,
  orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0055 — Extraer el struct StaticAssertConstant desde src/Frontend/HIR/HirStatementMachine.cpp:29-36 (8)
  hacia src/Frontend/HIR/Internal/StaticAssertConstant.h, sin .cpp adicional; conservar texto, comentarios, firmas,
  orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0056 — Extraer el class MirByteReader desde src/Frontend/MIR/MirBuildCoordinator.cpp:104-166 (63) hacia
  src/Frontend/MIR/Internal/MirByteReader.h, y mover sus cuerpos a src/Frontend/MIR/Internal/MirByteReader.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0057 — Extraer el class ProjectOopMirModel::MirNodeMachine desde
  src/Frontend/MIR/MirNodeMachine.cpp:29-149 (121) hacia src/Frontend/MIR/Internal/MirNodeMachine.h, y mover sus cuerpos
  a src/Frontend/MIR/Internal/MirNodeMachine.cpp; conservar texto, comentarios, firmas, orden y conducta. Restricción de
  pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0079 — Extraer el enum LoweringFrameKind desde src/HIR/HirBuilder.cpp:18-31 (14) hacia
  src/HIR/Internal/LoweringFrameKind.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0080 — Extraer el struct LoweringFrame desde src/HIR/HirBuilder.cpp:33-47 (15) hacia
  src/HIR/Internal/LoweringFrame.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0081 — Extraer el class IterativeHirLowering desde src/HIR/HirBuilder.cpp:50-1384 (1335) hacia
  src/HIR/Internal/IterativeHirLowering.h, y mover sus cuerpos a src/HIR/Internal/IterativeHirLowering.cpp; conservar
  texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0082 — Extraer el struct VerificationTask desde src/HIR/HirVerifier.cpp:96-112 (17) hacia
  src/HIR/Internal/VerificationTask.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0098 — Extraer el class IdentityWriter desde src/LLVM/ProjectOopMirCacheIdentity.cpp:17-74 (58) hacia
  src/LLVM/Internal/IdentityWriter.h, y mover sus cuerpos a src/LLVM/Internal/IdentityWriter.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0133 — Mover fuera de línea función libre makeNode de src/Frontend/HIR/HirLoweringSupport.h:42 (11
  líneas) a un único .cpp, dejando la declaración intacta y conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0134 — Mover fuera de línea función libre destroyHirTreeIteratively de
  src/Frontend/HIR/HirLoweringSupport.h:54 (4 líneas) a un único .cpp, dejando la declaración intacta y conservando
  exactamente el cuerpo.
- [ ] NSC-IMPL-0182 — Mover fuera de línea método ProjectOopHirModel::lastErrorRange de
  include/nativesharp/Frontend/ProjectOopHirModel.h:1202 (5 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0434 — Extraer el enum ProjectOopHirNodeKind desde
  include/nativesharp/Frontend/ProjectOopHirModel.h:30-540 hacia include/nativesharp/Frontend/ProjectOopHirNodeKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0435 — Extraer el enum ProjectCollectionSegmentKind desde
  include/nativesharp/Frontend/ProjectOopHirModel.h:542-546 hacia
  include/nativesharp/Frontend/ProjectCollectionSegmentKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0436 — Extraer el struct ProjectOopHirNode desde
  include/nativesharp/Frontend/ProjectOopHirModel.h:548-699 hacia include/nativesharp/Frontend/ProjectOopHirNode.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0437 — Extraer el struct ProjectOopHirFieldInitializer desde
  include/nativesharp/Frontend/ProjectOopHirModel.h:701-708 hacia
  include/nativesharp/Frontend/ProjectOopHirFieldInitializer.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0438 — Extraer el struct ProjectOopHirFunction desde
  include/nativesharp/Frontend/ProjectOopHirModel.h:710-782 hacia include/nativesharp/Frontend/ProjectOopHirFunction.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0439 — Conservar el class ProjectOopHirModel como tipo principal de
  include/nativesharp/Frontend/ProjectOopHirModel.h:784-1396 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0440 — Extraer el enum ProjectOopMirAbiPassing desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:50-54 hacia include/nativesharp/Frontend/ProjectOopMirAbiPassing.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0441 — Extraer el enum ProjectOopMirNodeKind desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:58-434 hacia include/nativesharp/Frontend/ProjectOopMirNodeKind.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0442 — Extraer el struct ProjectOopMirNode desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:436-575 hacia include/nativesharp/Frontend/ProjectOopMirNode.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0443 — Extraer el struct ProjectOopMirFieldInitializer desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:577-584 hacia
  include/nativesharp/Frontend/ProjectOopMirFieldInitializer.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0444 — Extraer el struct ProjectOopMirStructLayout desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:587-602 hacia
  include/nativesharp/Frontend/ProjectOopMirStructLayout.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0445 — Extraer el struct ProjectOopMirClassLayout desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:608-620 hacia
  include/nativesharp/Frontend/ProjectOopMirClassLayout.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0446 — Extraer el struct ProjectOopMirFunctionAbi desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:622-629 hacia
  include/nativesharp/Frontend/ProjectOopMirFunctionAbi.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0447 — Extraer el enum ProjectOopMirCleanupKind desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:631-638 hacia
  include/nativesharp/Frontend/ProjectOopMirCleanupKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0448 — Extraer el struct ProjectOopMirCleanupRegion desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:642-652 hacia
  include/nativesharp/Frontend/ProjectOopMirCleanupRegion.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0449 — Extraer el enum ProjectOopMirNonlocalExitKind desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:654-663 hacia
  include/nativesharp/Frontend/ProjectOopMirNonlocalExitKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0450 — Extraer el struct ProjectOopMirNonlocalExit desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:667-675 hacia
  include/nativesharp/Frontend/ProjectOopMirNonlocalExit.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0451 — Extraer el struct ProjectOopMirExceptionFlow desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:677-684 hacia
  include/nativesharp/Frontend/ProjectOopMirExceptionFlow.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0452 — Extraer el struct ProjectOopMirStaticField desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:686-714 hacia
  include/nativesharp/Frontend/ProjectOopMirStaticField.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0453 — Extraer el struct ProjectOopMirDispatchTable desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:717-816 hacia
  include/nativesharp/Frontend/ProjectOopMirDispatchTable.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0454 — Extraer el struct ProjectOopMirFunction desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:818-882 hacia include/nativesharp/Frontend/ProjectOopMirFunction.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0455 — Extraer el struct ProjectOopMirGenericSpecialization desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:887-893 hacia
  include/nativesharp/Frontend/ProjectOopMirGenericSpecialization.h; conservar texto, comentarios, orden de declaración
  y cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0456 — Extraer el struct ProjectOopMirInterfaceBinding desde
  include/nativesharp/Frontend/ProjectOopMirModel.h:898-908 hacia
  include/nativesharp/Frontend/ProjectOopMirInterfaceBinding.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0457 — Conservar el class ProjectOopMirModel como tipo principal de
  include/nativesharp/Frontend/ProjectOopMirModel.h:910-1099 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0786 — Extraer el struct MirPhiIncoming desde include/nativesharp/MIR/MirInstruction.h:23-27 hacia
  include/nativesharp/MIR/MirPhiIncoming.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0787 — Conservar el class MirInstruction como tipo principal de
  include/nativesharp/MIR/MirInstruction.h:29-96 y convertir ese archivo en umbrella de sus tipos extraídos, sin alterar
  el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0788 — Extraer el enum MirVerificationMode desde include/nativesharp/MIR/MirVerifier.h:13-17 hacia
  include/nativesharp/MIR/MirVerificationMode.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0789 — Conservar el class MirVerifier como tipo principal de include/nativesharp/MIR/MirVerifier.h:25-44
  y convertir ese archivo en umbrella de sus tipos extraídos, sin alterar el cierre transitivo. Restricción de pruebas:
  ninguna.
- [ ] NSC-IMPL-0824 — Extraer el struct HirCoreOperandCallPlan desde src/Frontend/HIR/HirLoweringSupport.h:15-19 hacia
  src/Frontend/HIR/HirCoreOperandCallPlan.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0825 — Extraer el struct HirAtomicCallPlan desde src/Frontend/HIR/HirLoweringSupport.h:24-28 hacia
  src/Frontend/HIR/HirAtomicCallPlan.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante
  el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0826 — Extraer el struct HirMonitorCallPlan desde src/Frontend/HIR/HirLoweringSupport.h:33-37 hacia
  src/Frontend/HIR/HirMonitorCallPlan.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante
  el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0827 — Extraer el struct HirTuplePatternFrame desde src/Frontend/HIR/HirLoweringSupport.h:59-64 hacia
  src/Frontend/HIR/HirTuplePatternFrame.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0852 — Descomponer ProjectOopHirModel::lowerExpressionWithStatementFrame en
  src/Frontend/HIR/HirExpressionMachine.cpp:284 (8181 líneas ejecutables; 10721 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0856 — Descomponer ProjectOopHirModel::lowerExpressionImpl en
  src/Frontend/HIR/HirExpressionMachine.cpp:11161 (1425 líneas ejecutables; 2121 totales) hasta que cada función quede
  en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0863 — Descomponer ProjectOopHirModel::buildImpl en src/Frontend/HIR/HirBuildCoordinator.cpp:607 (838
  líneas ejecutables; 1210 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control
  de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0874 — Descomponer ProjectOopHirModel::lowerCallExpression en src/Frontend/HIR/HirCallLowering.cpp:995
  (482 líneas ejecutables; 801 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0881 — Descomponer HirVerifier::verifyNode en src/HIR/HirVerifier.cpp:149 (353 líneas ejecutables; 562
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0885 — Descomponer MirVerifier::verifyInstruction en src/MIR/MirVerifier.cpp:318 (330 líneas ejecutables;
  442 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0886 — Descomponer lowerExpressionFrame en src/HIR/HirBuilder.cpp:688 (324 líneas ejecutables; 428
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0893 — Descomponer ProjectOopMirModel::build en src/Frontend/MIR/MirBuildCoordinator.cpp:1382 (294 líneas
  ejecutables; 441 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0895 — Descomponer projectOopMirNodeKindName en src/Frontend/MIR/MirBuildCoordinator.cpp:1096 (280 líneas
  ejecutables; 285 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0899 — Descomponer projectOopHirNodeKindName en src/Frontend/HIR/HirBuildCoordinator.cpp:307 (275 líneas
  ejecutables; 280 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0900 — Descomponer hirCoreOperandCallPlanImpl en src/Frontend/HIR/HirCallLowering.cpp:13 (270 líneas
  ejecutables; 419 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0904 — Descomponer ProjectOopMirModel::importLayouts en src/Frontend/MIR/MirLayoutImporter.cpp:74 (266
  líneas ejecutables; 337 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0906 — Descomponer evaluateStaticAssertConstant en src/Frontend/HIR/HirStatementMachine.cpp:77 (256
  líneas ejecutables; 311 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0912 — Descomponer run en src/HIR/HirBuilder.cpp:126 (230 líneas ejecutables; 328 totales) hasta que cada
  función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0924 — Descomponer restoreMirFunctionCore en src/Frontend/MIR/MirBuildCoordinator.cpp:748 (202 líneas
  ejecutables; 254 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0936 — Descomponer ProjectOopMirModelDumper::dump en src/Frontend/ProjectOopMirModelDumper.cpp:328 (187
  líneas ejecutables; 228 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0940 — Descomponer ProjectOopHirModel::splitAsyncGeneratorFunction en
  src/Frontend/HIR/HirAsyncLowering.cpp:532 (174 líneas ejecutables; 205 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0943 — Descomponer lowerStatementFrame en src/HIR/HirBuilder.cpp:455 (170 líneas ejecutables; 232
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0949 — Descomponer ProjectOopHirModel::finishAllocatorConstructionExpression en
  src/Frontend/HIR/HirAggregateLowering.cpp:578 (163 líneas ejecutables; 198 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0953 — Descomponer ProjectOopHirModel::appendAggregateElements en
  src/Frontend/HIR/HirAggregateLowering.cpp:40 (157 líneas ejecutables; 217 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0956 — Descomponer DominanceVerifier::verifyFunction en src/Dominance/DominanceVerifier.cpp:209 (154
  líneas ejecutables; 295 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0967 — Descomponer readMirNodeFields en src/Frontend/MIR/MirBuildCoordinator.cpp:337 (140 líneas
  ejecutables; 161 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0968 — Descomponer ProjectOopHirModel::rewriteAsyncPollReturns en
  src/Frontend/HIR/HirAsyncLowering.cpp:101 (139 líneas ejecutables; 206 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0973 — Descomponer lowerAddressFrame en src/HIR/HirBuilder.cpp:1117 (137 líneas ejecutables; 186 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [ ] NSC-IMPL-0980 — Descomponer dumpNodeLine en src/Frontend/ProjectOopHirModelDumper.cpp:13 (129 líneas ejecutables;
  144 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0990 — Descomponer appendMirNodeFields en src/Frontend/MIR/MirBuildCoordinator.cpp:169 (123 líneas
  ejecutables; 140 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0991 — Descomponer MirBuilder::lowerExpression en src/MIR/MirBuilder.cpp:422 (122 líneas ejecutables; 203
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1005 — Descomponer serializeMirFunctionCore en src/Frontend/MIR/MirBuildCoordinator.cpp:612 (113 líneas
  ejecutables; 135 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1011 — Descomponer ProjectOopHirModel::splitGeneratorFunction en
  src/Frontend/HIR/HirAsyncLowering.cpp:390 (109 líneas ejecutables; 141 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1021 — Descomponer writeNode en src/LLVM/ProjectOopMirCacheIdentity.cpp:76 (104 líneas ejecutables; 109
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1026 — Descomponer ProjectOopHirModel::lowerArrayConstructionExpression en
  src/Frontend/HIR/HirAggregateLowering.cpp:392 (101 líneas ejecutables; 185 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1084 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup07 en
  src/Frontend/MIR/MirNodeMachine.cpp:750 (81 líneas ejecutables; 86 totales) hasta que cada función quede en ≤60 líneas
  ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1085 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup08 en
  src/Frontend/MIR/MirNodeMachine.cpp:837 (81 líneas ejecutables; 86 totales) hasta que cada función quede en ≤60 líneas
  ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1086 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup09 en
  src/Frontend/MIR/MirNodeMachine.cpp:924 (81 líneas ejecutables; 86 totales) hasta que cada función quede en ≤60 líneas
  ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1096 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup11 en
  src/Frontend/MIR/MirNodeMachine.cpp:1115 (79 líneas ejecutables; 84 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1101 — Descomponer writeCommonModel en src/LLVM/ProjectOopMirCacheIdentity.cpp:282 (78 líneas
  ejecutables; 99 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1108 — Descomponer writeFunction en src/LLVM/ProjectOopMirCacheIdentity.cpp:197 (77 líneas ejecutables;
  84 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1113 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup10 en
  src/Frontend/MIR/MirNodeMachine.cpp:1011 (76 líneas ejecutables; 103 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1122 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup06 en
  src/Frontend/MIR/MirNodeMachine.cpp:660 (75 líneas ejecutables; 89 totales) hasta que cada función quede en ≤60 líneas
  ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1123 — Descomponer ProjectOopHirModel::lowerDestructionExpression en
  src/Frontend/HIR/HirExceptionAndCleanup.cpp:130 (75 líneas ejecutables; 124 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1140 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup01 en
  src/Frontend/MIR/MirNodeMachine.cpp:267 (71 líneas ejecutables; 84 totales) hasta que cada función quede en ≤60 líneas
  ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1148 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup03 en
  src/Frontend/MIR/MirNodeMachine.cpp:410 (69 líneas ejecutables; 80 totales) hasta que cada función quede en ≤60 líneas
  ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1160 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup05 en
  src/Frontend/MIR/MirNodeMachine.cpp:581 (67 líneas ejecutables; 78 totales) hasta que cada función quede en ≤60 líneas
  ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1162 — Descomponer ProjectOopHirModel::buildBuiltinBinaryExpression en
  src/Frontend/HIR/HirExpressionMachine.cpp:11037 (66 líneas ejecutables; 88 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1171 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup17 en
  src/Frontend/MIR/MirNodeMachine.cpp:1593 (64 líneas ejecutables; 86 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1180 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup04 en
  src/Frontend/MIR/MirNodeMachine.cpp:491 (63 líneas ejecutables; 89 totales) hasta que cada función quede en ≤60 líneas
  ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1192 — Descomponer ProjectOopMirModel::MirNodeMachine::lowerGroup14 en
  src/Frontend/MIR/MirNodeMachine.cpp:1366 (62 líneas ejecutables; 89 totales) hasta que cada función quede en ≤60
  líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1193 — Descomponer ProjectOopHirModel::rewriteGeneratorThrows en src/Frontend/HIR/HirAsyncLowering.cpp:31
  (62 líneas ejecutables; 69 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control
  de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1194 — Descomponer ProjectOopHirModel::splitAsyncFunction en src/Frontend/HIR/HirAsyncLowering.cpp:308
  (62 líneas ejecutables; 81 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control
  de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1207 — Convertir ProjectOopHirModel en fachada con colaboradores por archivo, conservando firmas
  privadas, bindings y todos sus campos.
- [ ] NSC-IMPL-1209 — Convertir ProjectOopMirModel en fachada con colaboradores para coordinación, lowering, layout,
  nodos, caché y nombres de kinds.

### Commit REF-06 — Parser, análisis semántico y servicio de lenguaje — 0/72

Ordena Parser, Sema y LanguageService conservando vistas privadas, shims, cursor y contratos de pruebas.

- [ ] NSC-IMPL-0106 — Extraer el struct IndexedReference desde src/LanguageService/LanguageService.cpp:27-32 (6) hacia
  src/LanguageService/Internal/IndexedReference.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0107 — Extraer el struct IndexedCall desde src/LanguageService/LanguageService.cpp:34-40 (7) hacia
  src/LanguageService/Internal/IndexedCall.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0108 — Extraer el struct SemanticIndex desde src/LanguageService/LanguageService.cpp:42-48 (7) hacia
  src/LanguageService/Internal/SemanticIndex.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0109 — Extraer el struct LineBraceSummary desde src/LanguageService/LanguageService.cpp:134-139 (6) hacia
  src/LanguageService/Internal/LineBraceSummary.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0110 — Extraer el class AnalysisSnapshot::Implementation desde
  src/LanguageService/LanguageService.cpp:578-590 (13) hacia src/LanguageService/Internal/Implementation.h, sin .cpp
  adicional; conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0111 — Extraer el class LanguageService::Implementation desde
  src/LanguageService/LanguageService.cpp:592-621 (30) hacia src/LanguageService/Internal/Implementation.h, y mover sus
  cuerpos a src/LanguageService/Internal/Implementation.cpp; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0112 — Extraer el struct DecodedCodePoint desde src/LanguageService/ProtocolTypes.cpp:11-15 (5) hacia
  src/LanguageService/Internal/DecodedCodePoint.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0117 — Extraer el class Parser::MatchExpressionPatternAnalyzer desde
  src/Parser/ExpressionParser.cpp:230-424 (195) hacia src/Parser/Internal/MatchExpressionPatternAnalyzer.h, y mover sus
  cuerpos a src/Parser/Internal/MatchExpressionPatternAnalyzer.cpp; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0118 — Extraer el class Parser::ExpressionIterationMachine desde src/Parser/ExpressionParser.cpp:670-711
  (42) hacia src/Parser/Internal/ExpressionIterationMachine.h, y mover sus cuerpos a
  src/Parser/Internal/ExpressionIterationMachine.cpp; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0119 — Extraer el class Parser::PostfixIterationMachine desde src/Parser/ExpressionParser.cpp:1492-1638
  (147) hacia src/Parser/Internal/PostfixIterationMachine.h, y mover sus cuerpos a
  src/Parser/Internal/PostfixIterationMachine.cpp; conservar texto, comentarios, firmas, orden y conducta. Restricción
  de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0120 — Extraer el enum BinaryRightHandSide desde src/Parser/ExpressionParser.cpp:1833-1833 (1) hacia
  src/Parser/Internal/BinaryRightHandSide.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0121 — Extraer el struct BinaryOperatorInfo desde src/Parser/ExpressionParser.cpp:1835-1841 (7) hacia
  src/Parser/Internal/BinaryOperatorInfo.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0122 — Extraer el class Parser::ParseContinuationCoordinator desde
  src/Parser/ParserContinuationCoordinator.cpp:25-671 (647) hacia src/Parser/Internal/ParseContinuationCoordinator.h,
  sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: shim + vista
  privada (A-1).
- [ ] NSC-IMPL-0123 — Extraer el class Parser::PatternIterationParser desde src/Parser/PatternParser.cpp:27-97 (71)
  hacia src/Parser/Internal/PatternIterationParser.h, y mover sus cuerpos a
  src/Parser/Internal/PatternIterationParser.cpp; conservar texto, comentarios, firmas, orden y conducta. Restricción de
  pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0124 — Extraer el class Parser::PatternAlternativeExpander desde src/Parser/PatternParser.cpp:99-127 (29)
  hacia src/Parser/Internal/PatternAlternativeExpander.h, y mover sus cuerpos a
  src/Parser/Internal/PatternAlternativeExpander.cpp; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0125 — Extraer el class Parser::PatternConditionBuilder desde src/Parser/PatternParser.cpp:129-212 (84)
  hacia src/Parser/Internal/PatternConditionBuilder.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0126 — Extraer el class Parser::PatternLoweringMachine desde src/Parser/PatternParser.cpp:214-328 (115)
  hacia src/Parser/Internal/PatternLoweringMachine.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0127 — Extraer el class Parser::PatternBindingEmitter desde src/Parser/PatternParser.cpp:330-496 (167)
  hacia src/Parser/Internal/PatternBindingEmitter.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0128 — Extraer el class Parser::StructuredControlParser desde src/Parser/StatementParser.cpp:1479-3111
  (1633) hacia src/Parser/Internal/StructuredControlParser.h, y mover sus cuerpos a
  src/Parser/Internal/StructuredControlParser.cpp; conservar texto, comentarios, firmas, orden y conducta. Restricción
  de pruebas: shim + vista privada (A-1).
- [ ] NSC-IMPL-0139 — Mover fuera de línea método KnownGenericTypeScope::KnownGenericTypeScope de
  src/Parser/KnownGenericTypeScope.h:18 (21 líneas) a un único .cpp, dejando la declaración intacta y conservando
  exactamente el cuerpo.
- [ ] NSC-IMPL-0140 — Mover fuera de línea método KnownGenericTypeScope::~KnownGenericTypeScope de
  src/Parser/KnownGenericTypeScope.h:40 (4 líneas) a un único .cpp, dejando la declaración intacta y conservando
  exactamente el cuerpo.
- [ ] NSC-IMPL-0757 — Extraer el struct LanguageServiceIdentity desde
  include/nativesharp/LanguageService/LanguageService.h:20-27 hacia
  include/nativesharp/LanguageService/LanguageServiceIdentity.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0758 — Extraer el struct DocumentSnapshot desde
  include/nativesharp/LanguageService/LanguageService.h:29-34 hacia
  include/nativesharp/LanguageService/DocumentSnapshot.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0759 — Extraer el class AnalysisSnapshot desde
  include/nativesharp/LanguageService/LanguageService.h:36-87 hacia
  include/nativesharp/LanguageService/AnalysisSnapshot.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0760 — Conservar el class LanguageService como tipo principal de
  include/nativesharp/LanguageService/LanguageService.h:91-131 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0761 — Extraer el struct TextPosition desde include/nativesharp/LanguageService/ProtocolTypes.h:16-22
  hacia include/nativesharp/LanguageService/TextPosition.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0762 — Extraer el struct TextRange desde include/nativesharp/LanguageService/ProtocolTypes.h:24-30 hacia
  include/nativesharp/LanguageService/TextRange.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0763 — Extraer el enum LanguageDiagnosticSeverity desde
  include/nativesharp/LanguageService/ProtocolTypes.h:32-37 hacia
  include/nativesharp/LanguageService/LanguageDiagnosticSeverity.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0764 — Extraer el struct LanguageDiagnostic desde
  include/nativesharp/LanguageService/ProtocolTypes.h:39-46 hacia
  include/nativesharp/LanguageService/LanguageDiagnostic.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0765 — Extraer el enum LanguageSymbolKind desde include/nativesharp/LanguageService/ProtocolTypes.h:48-63
  hacia include/nativesharp/LanguageService/LanguageSymbolKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0766 — Extraer el struct LanguageLocation desde include/nativesharp/LanguageService/ProtocolTypes.h:65-69
  hacia include/nativesharp/LanguageService/LanguageLocation.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0767 — Extraer el struct LanguageSymbol desde include/nativesharp/LanguageService/ProtocolTypes.h:71-80
  hacia include/nativesharp/LanguageService/LanguageSymbol.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0768 — Extraer el struct CompletionItem desde include/nativesharp/LanguageService/ProtocolTypes.h:82-87
  hacia include/nativesharp/LanguageService/CompletionItem.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0769 — Extraer el struct SignatureInformation desde
  include/nativesharp/LanguageService/ProtocolTypes.h:89-93 hacia
  include/nativesharp/LanguageService/SignatureInformation.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0770 — Extraer el struct SignatureHelp desde include/nativesharp/LanguageService/ProtocolTypes.h:95-100
  hacia include/nativesharp/LanguageService/SignatureHelp.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0771 — Extraer el struct LanguageTextEdit desde
  include/nativesharp/LanguageService/ProtocolTypes.h:102-106 hacia
  include/nativesharp/LanguageService/LanguageTextEdit.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0772 — Extraer el struct DocumentFormattingOptions desde
  include/nativesharp/LanguageService/ProtocolTypes.h:108-112 hacia
  include/nativesharp/LanguageService/DocumentFormattingOptions.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0773 — Extraer el enum LanguageCodeActionKind desde
  include/nativesharp/LanguageService/ProtocolTypes.h:114-118 hacia
  include/nativesharp/LanguageService/LanguageCodeActionKind.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0774 — Extraer el struct LanguageCodeAction desde
  include/nativesharp/LanguageService/ProtocolTypes.h:120-127 hacia
  include/nativesharp/LanguageService/LanguageCodeAction.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0775 — Extraer el struct RenameResult desde include/nativesharp/LanguageService/ProtocolTypes.h:129-134
  hacia include/nativesharp/LanguageService/RenameResult.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0776 — Extraer el struct LanguageSemanticToken desde
  include/nativesharp/LanguageService/ProtocolTypes.h:136-141 hacia
  include/nativesharp/LanguageService/LanguageSemanticToken.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0797 — Extraer el enum ConstantKind desde include/nativesharp/Sema/ConstantValue.h:8-17 hacia
  include/nativesharp/Sema/ConstantKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0798 — Conservar el class ConstantValue como tipo principal de
  include/nativesharp/Sema/ConstantValue.h:19-43 y convertir ese archivo en umbrella de sus tipos extraídos, sin alterar
  el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0799 — Extraer el enum ConversionRole desde include/nativesharp/Sema/ConversionInfo.h:14-19 hacia
  include/nativesharp/Sema/ConversionRole.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0800 — Conservar el struct ConversionInfo como tipo principal de
  include/nativesharp/Sema/ConversionInfo.h:21-29 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0801 — Extraer el enum Effect desde include/nativesharp/Sema/EffectContext.h:9-17 hacia
  include/nativesharp/Sema/Effect.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0802 — Extraer el class EffectSet desde include/nativesharp/Sema/EffectContext.h:19-42 hacia
  include/nativesharp/Sema/EffectSet.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante
  el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0803 — Conservar el class EffectContext como tipo principal de
  include/nativesharp/Sema/EffectContext.h:63-86 y convertir ese archivo en umbrella de sus tipos extraídos, sin alterar
  el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0804 — Extraer el enum ValueCategory desde include/nativesharp/Sema/ExpressionInfo.h:12-24 hacia
  include/nativesharp/Sema/ValueCategory.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0805 — Conservar el struct ExpressionInfo como tipo principal de
  include/nativesharp/Sema/ExpressionInfo.h:54-64 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0945 — Descomponer SemanticAnalyzer::evaluateConstantBinary en src/Sema/SemanticAnalyzer.cpp:3317 (165
  líneas ejecutables; 250 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0950 — Descomponer buildSemanticIndex en src/LanguageService/LanguageService.cpp:317 (162 líneas
  ejecutables; 208 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0963 — Descomponer SemanticAnalyzer::analyzeBinary en src/Sema/SemanticAnalyzer.cpp:1370 (144 líneas
  ejecutables; 225 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1018 — Descomponer Parser::tryParseGlobalValueTypePrefix en src/Parser/AttributeParser.cpp:315 (104
  líneas ejecutables; 148 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1050 — Descomponer SemanticAnalyzer::analyzeIndex en src/Sema/SemanticAnalyzer.cpp:1800 (90 líneas
  ejecutables; 136 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1051 — Descomponer Parser::lowerForeachRange en src/Parser/StatementParser.cpp:3652 (90 líneas
  ejecutables; 109 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1068 — Descomponer SemanticAnalyzer::analyzeFunction en src/Sema/SemanticAnalyzer.cpp:490 (85 líneas
  ejecutables; 139 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1074 — Descomponer Parser::tryDispatchValueGlobal en src/Parser/AttributeParser.cpp:602 (84 líneas
  ejecutables; 114 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1087 — Descomponer Parser::ParseContinuationCoordinator::execute en
  src/Parser/ParserContinuationCoordinator.cpp:731 (81 líneas ejecutables; 86 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1098 — Descomponer SemanticAnalyzer::analyzeStatement en src/Sema/SemanticAnalyzer.cpp:707 (78 líneas
  ejecutables; 101 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1115 — Descomponer SemanticAnalyzer::analyzeEnum en src/Sema/SemanticAnalyzer.cpp:355 (75 líneas
  ejecutables; 126 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1116 — Descomponer SemanticAnalyzer::analyzeUnary en src/Sema/SemanticAnalyzer.cpp:1252 (75 líneas
  ejecutables; 117 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1134 — Descomponer SemanticAnalyzer::analyzeLiteral en src/Sema/SemanticAnalyzer.cpp:1143 (72 líneas
  ejecutables; 108 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1164 — Descomponer SimpleXmlParser::parseElement en src/Compilation/SimpleXmlParser.cpp:44 (65 líneas
  ejecutables; 121 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1165 — Descomponer SemanticAnalyzer::analyzeStruct en src/Sema/SemanticAnalyzer.cpp:252 (65 líneas
  ejecutables; 102 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1175 — Descomponer SemanticAnalyzer::analyzeExpression en src/Sema/SemanticAnalyzer.cpp:1011 (63 líneas
  ejecutables; 71 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1181 — Descomponer Parser::parseOperatorTail en src/Parser/MemberParser.cpp:2528 (63 líneas ejecutables;
  79 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1195 — Descomponer Parser::PostfixIterationMachine::startPostfix en src/Parser/ExpressionParser.cpp:2847
  (62 líneas ejecutables; 77 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control
  de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1200 — Descomponer Parser::parseNameOfExpression en src/Parser/ExpressionParser.cpp:3646 (61 líneas
  ejecutables; 94 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1212 — Partir ParseContinuationCoordinator por familias de métodos sin alterar su ciclo, cursor, campos
  ni algoritmo recursivo.
- [ ] NSC-IMPL-1213 — Partir SemanticAnalyzer.cpp en familias con shims autorizados, conservando maskForWidth y todos
  los miembros congelados.
- [ ] NSC-IMPL-1211 — Cerrar Parser mediante R1 y R5 sin crear colaboradores para sus métodos y sin mover helpers
  congelados por los shims.

### Commit REF-07 — Backend LLVM — 0/150

Separa lowering, generación y soporte LLVM sin cambiar IR, objetos, depuración ni ABI.

- [ ] NSC-IMPL-0088 — Extraer el class CachePruneMeasurement desde src/LLVM/CompilationArtifactCache.cpp:60-83 (24)
  hacia src/LLVM/Internal/CachePruneMeasurement.h, y mover sus cuerpos a src/LLVM/Internal/CachePruneMeasurement.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0089 — Extraer el class CacheEntryLock desde src/LLVM/CompilationArtifactCache.cpp:127-181 (55) hacia
  src/LLVM/Internal/CacheEntryLock.h, y mover sus cuerpos a src/LLVM/Internal/CacheEntryLock.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0090 — Extraer el struct CacheMetadata desde src/LLVM/CompilationArtifactCache.cpp:285-290 (6) hacia
  src/LLVM/Internal/CacheMetadata.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0091 — Extraer el struct CachePruneEntry desde src/LLVM/CompilationArtifactCache.cpp:396-403 (8) hacia
  src/LLVM/Internal/CachePruneEntry.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0092 — Extraer el class LlvmLowerer::Implementation desde src/LLVM/LlvmLowerer.cpp:207-1943 (1737) hacia
  src/LLVM/Internal/Implementation.h, y mover sus cuerpos a src/LLVM/Internal/Implementation.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0093 — Extraer el class StandardErrorCapture desde src/LLVM/LlvmOptimizer.cpp:144-188 (45) hacia
  src/LLVM/Internal/StandardErrorCapture.h, y mover sus cuerpos a src/LLVM/Internal/StandardErrorCapture.cpp; conservar
  texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0094 — Extraer el struct PartitionCandidate desde src/LLVM/ProjectOopCodePartition.cpp:20-25 (6) hacia
  src/LLVM/Internal/PartitionCandidate.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0095 — Extraer el struct StaticLocalStorage desde src/LLVM/ProjectOopLlvmLowerer.cpp:1287-1293 (7) hacia
  src/LLVM/Internal/StaticLocalStorage.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0096 — Extraer el struct StaticLocalGuard desde src/LLVM/ProjectOopLlvmLowerer.cpp:1295-1301 (7) hacia
  src/LLVM/Internal/StaticLocalGuard.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0097 — Extraer el class OopLlvmImplementation desde src/LLVM/ProjectOopLlvmLowerer.cpp:1303-22851 (21549)
  hacia src/LLVM/Internal/OopLlvmImplementation.h, y mover sus cuerpos a src/LLVM/Internal/OopLlvmImplementation.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0099 — Extraer el class CodeUnitProgress desde src/LLVM/ProjectOopObjectGenerator.cpp:37-69 (33) hacia
  src/LLVM/Internal/CodeUnitProgress.h, y mover sus cuerpos a src/LLVM/Internal/CodeUnitProgress.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0100 — Extraer el class PhaseClock desde src/LLVM/ProjectOopObjectGenerator.cpp:101-114 (14) hacia
  src/LLVM/Internal/PhaseClock.h, y mover sus cuerpos a src/LLVM/Internal/PhaseClock.cpp; conservar texto, comentarios,
  firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0101 — Extraer el struct ColdLlvmPreambleTelemetry desde src/LLVM/ProjectOopObjectGenerator.cpp:116-129
  (14) hacia src/LLVM/Internal/ColdLlvmPreambleTelemetry.h, sin .cpp adicional; conservar texto, comentarios, firmas,
  orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0102 — Extraer el class ColdLlvmTrace desde src/LLVM/ProjectOopObjectGenerator.cpp:133-223 (91) hacia
  src/LLVM/Internal/ColdLlvmTrace.h, y mover sus cuerpos a src/LLVM/Internal/ColdLlvmTrace.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0103 — Extraer el class ColdLlvmTraceScope desde src/LLVM/ProjectOopObjectGenerator.cpp:225-252 (28)
  hacia src/LLVM/Internal/ColdLlvmTraceScope.h, y mover sus cuerpos a src/LLVM/Internal/ColdLlvmTraceScope.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0141 — Mover fuera de línea función libre correctedSignedDivisor de
  src/LLVM/SignedDivisionCorrection.h:66 (33 líneas) a un único .cpp, dejando la declaración intacta y conservando
  exactamente el cuerpo.
- [ ] NSC-IMPL-0183 — Mover fuera de línea función libre portablePath de
  include/nativesharp/LLVM/LlvmDebugPrefixMap.h:15 (12 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0184 — Mover fuera de línea función libre foldedWindowsPath de
  include/nativesharp/LLVM/LlvmDebugPrefixMap.h:28 (8 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0185 — Mover fuera de línea función libre pathHasPrefix de
  include/nativesharp/LLVM/LlvmDebugPrefixMap.h:37 (6 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0186 — Mover fuera de línea función libre remapDebugSourcePath de
  include/nativesharp/LLVM/LlvmDebugPrefixMap.h:44 (33 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0187 — Mover fuera de línea función libre isPointerLikeType de
  include/nativesharp/LLVM/LlvmTypeClassification.h:13 (7 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0188 — Mover fuera de línea función libre isDelegateReferenceType de
  include/nativesharp/LLVM/LlvmTypeClassification.h:21 (15 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0189 — Mover fuera de línea función libre isClassReferenceType de
  include/nativesharp/LLVM/LlvmTypeClassification.h:37 (13 líneas) a un único .cpp, dejando la declaración intacta y
  conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0190 — Mover fuera de línea función libre findStructLayout de
  include/nativesharp/LLVM/LlvmTypeLayout.h:14 (9 líneas) a un único .cpp, dejando la declaración intacta y conservando
  exactamente el cuerpo.
- [ ] NSC-IMPL-0191 — Mover fuera de línea función libre findClassLayout de include/nativesharp/LLVM/LlvmTypeLayout.h:24
  (9 líneas) a un único .cpp, dejando la declaración intacta y conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0192 — Mover fuera de línea función libre findEnumUnderlyingInfo de
  include/nativesharp/LLVM/LlvmTypeLayout.h:34 (14 líneas) a un único .cpp, dejando la declaración intacta y conservando
  exactamente el cuerpo.
- [ ] NSC-IMPL-0193 — Mover fuera de línea función libre isEnumTypeKey de include/nativesharp/LLVM/LlvmTypeLayout.h:49
  (5 líneas) a un único .cpp, dejando la declaración intacta y conservando exactamente el cuerpo.
- [ ] NSC-IMPL-0499 — Extraer el struct CoffObjectInfo desde include/nativesharp/LLVM/CoffObjectVerifier.h:9-14 hacia
  include/nativesharp/LLVM/CoffObjectInfo.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0500 — Conservar el class CoffObjectVerifier como tipo principal de
  include/nativesharp/LLVM/CoffObjectVerifier.h:16-26 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0501 — Extraer el enum CacheMissReason desde include/nativesharp/LLVM/CompilationArtifactCache.h:18-26
  hacia include/nativesharp/LLVM/CacheMissReason.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0502 — Extraer el struct CachePruneTelemetry desde
  include/nativesharp/LLVM/CompilationArtifactCache.h:33-41 hacia include/nativesharp/LLVM/CachePruneTelemetry.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0503 — Conservar el class CompilationArtifactCache como tipo principal de
  include/nativesharp/LLVM/CompilationArtifactCache.h:43-104 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0504 — Extraer el class CompilationArtifactCacheScope desde
  include/nativesharp/LLVM/CompilationArtifactCache.h:106-117 hacia
  include/nativesharp/LLVM/CompilationArtifactCacheScope.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0505 — Conservar el enum FloatingPointMode como tipo principal de
  include/nativesharp/LLVM/FloatingPointMode.h:8-14 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0506 — Extraer el enum FmaContractPolicy desde include/nativesharp/LLVM/FloatingPointMode.h:16-21 hacia
  include/nativesharp/LLVM/FmaContractPolicy.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0507 — Extraer el enum FloatingPointRoundingMode desde include/nativesharp/LLVM/FloatingPointMode.h:23-30
  hacia include/nativesharp/LLVM/FloatingPointRoundingMode.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0508 — Extraer el enum FloatingPointDenormalMode desde include/nativesharp/LLVM/FloatingPointMode.h:32-38
  hacia include/nativesharp/LLVM/FloatingPointDenormalMode.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0509 — Extraer el struct FloatingPointState desde include/nativesharp/LLVM/FloatingPointMode.h:54-62
  hacia include/nativesharp/LLVM/FloatingPointState.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0510 — Extraer el class FloatingPointModeScope desde include/nativesharp/LLVM/FloatingPointMode.h:70-86
  hacia include/nativesharp/LLVM/FloatingPointModeScope.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0511 — Extraer el enum NativeSanitizerKind desde include/nativesharp/LLVM/LlvmNativeSanitizer.h:11-14
  hacia include/nativesharp/LLVM/NativeSanitizerKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0512 — Conservar el class LlvmNativeSanitizer como tipo principal de
  include/nativesharp/LLVM/LlvmNativeSanitizer.h:20-28 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0513 — Conservar el struct LlvmOptimizationObservation como tipo principal de
  include/nativesharp/LLVM/LlvmOptimizationObservation.h:32-106 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0514 — Extraer el struct LlvmProcessTraceLog desde
  include/nativesharp/LLVM/LlvmOptimizationObservation.h:120-128 hacia include/nativesharp/LLVM/LlvmProcessTraceLog.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0515 — Extraer el class LlvmOptimizationBudgetTracker desde
  include/nativesharp/LLVM/LlvmOptimizationObservation.h:158-177 hacia
  include/nativesharp/LLVM/LlvmOptimizationBudgetTracker.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0516 — Extraer el class LlvmOptimizationBudgetScope desde
  include/nativesharp/LLVM/LlvmOptimizationObservation.h:181-192 hacia
  include/nativesharp/LLVM/LlvmOptimizationBudgetScope.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0517 — Extraer el enum AutomaticInliningCostModel desde include/nativesharp/LLVM/LlvmOptimizer.h:20-26
  hacia include/nativesharp/LLVM/AutomaticInliningCostModel.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0518 — Extraer el struct AutomaticInliningConfiguration desde
  include/nativesharp/LLVM/LlvmOptimizer.h:28-37 hacia include/nativesharp/LLVM/AutomaticInliningConfiguration.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0519 — Conservar el class LlvmOptimizer como tipo principal de
  include/nativesharp/LLVM/LlvmOptimizer.h:42-90 y convertir ese archivo en umbrella de sus tipos extraídos, sin alterar
  el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0520 — Extraer el enum LlvmCodeGenFileType desde include/nativesharp/LLVM/LlvmTargetMachine.h:22-26 hacia
  include/nativesharp/LLVM/LlvmCodeGenFileType.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0521 — Conservar el class LlvmTargetMachine como tipo principal de
  include/nativesharp/LLVM/LlvmTargetMachine.h:28-103 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0522 — Extraer el enum ProjectOopCodePartitionPolicy desde
  include/nativesharp/LLVM/ProjectOopCodePartition.h:18-23 hacia
  include/nativesharp/LLVM/ProjectOopCodePartitionPolicy.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0523 — Extraer el struct ProjectOopCodeUnit desde
  include/nativesharp/LLVM/ProjectOopCodePartition.h:25-35 hacia include/nativesharp/LLVM/ProjectOopCodeUnit.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0524 — Conservar el class ProjectOopCodePartition como tipo principal de
  include/nativesharp/LLVM/ProjectOopCodePartition.h:37-70 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0525 — Extraer el class ProjectOopLlvmLoweringRequest desde
  include/nativesharp/LLVM/ProjectOopLlvmLowerer.h:33-82 hacia include/nativesharp/LLVM/ProjectOopLlvmLoweringRequest.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0526 — Conservar el class ProjectOopLlvmLowerer como tipo principal de
  include/nativesharp/LLVM/ProjectOopLlvmLowerer.h:84-120 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0527 — Extraer el struct CodeUnitTelemetry desde
  include/nativesharp/LLVM/ProjectOopObjectGenerator.h:36-52 hacia include/nativesharp/LLVM/CodeUnitTelemetry.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0528 — Extraer el class ProjectOopObjectBundle desde
  include/nativesharp/LLVM/ProjectOopObjectGenerator.h:54-85 hacia include/nativesharp/LLVM/ProjectOopObjectBundle.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0529 — Conservar el class ProjectOopObjectGenerator como tipo principal de
  include/nativesharp/LLVM/ProjectOopObjectGenerator.h:87-117 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0828 — Extraer el enum LLVMTailCallKind desde src/LLVM/LlvmCApi.h:45-51 hacia
  src/LLVM/LLVMTailCallKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0829 — Extraer el enum LLVMModuleFlagBehavior desde src/LLVM/LlvmCApi.h:56-64 hacia
  src/LLVM/LLVMModuleFlagBehavior.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0830 — Extraer el enum LLVMDWARFSourceLanguage desde src/LLVM/LlvmCApi.h:66-70 hacia
  src/LLVM/LLVMDWARFSourceLanguage.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0831 — Extraer el enum LLVMDWARFEmissionKind desde src/LLVM/LlvmCApi.h:72-77 hacia
  src/LLVM/LLVMDWARFEmissionKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0832 — Extraer el enum LLVMChecksumKind desde src/LLVM/LlvmCApi.h:79-84 hacia
  src/LLVM/LLVMChecksumKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0833 — Extraer el enum LLVMDIFlags desde src/LLVM/LlvmCApi.h:86-95 hacia src/LLVM/LLVMDIFlags.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0834 — Extraer el enum LLVMIntPredicate desde src/LLVM/LlvmCApi.h:111-123 hacia
  src/LLVM/LLVMIntPredicate.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0835 — Extraer el enum LLVMRealPredicate desde src/LLVM/LlvmCApi.h:125-143 hacia
  src/LLVM/LLVMRealPredicate.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0836 — Extraer el enum LLVMDenormalModeKind desde src/LLVM/LlvmCApi.h:145-151 hacia
  src/LLVM/LLVMDenormalModeKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0837 — Extraer el enum LLVMAtomicRMWBinOp desde src/LLVM/LlvmCApi.h:161-165 hacia
  src/LLVM/LLVMAtomicRMWBinOp.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0838 — Extraer el enum LLVMAtomicOrdering desde src/LLVM/LlvmCApi.h:172-181 hacia
  src/LLVM/LLVMAtomicOrdering.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0839 — Extraer el enum LLVMCodeGenOptLevel desde src/LLVM/LlvmCApi.h:183-189 hacia
  src/LLVM/LLVMCodeGenOptLevel.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0840 — Extraer el enum LLVMRelocMode desde src/LLVM/LlvmCApi.h:191-200 hacia src/LLVM/LLVMRelocMode.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0841 — Extraer el enum LLVMCodeModel desde src/LLVM/LlvmCApi.h:202-211 hacia src/LLVM/LLVMCodeModel.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0842 — Extraer el enum LLVMCodeGenFileType desde src/LLVM/LlvmCApi.h:213-217 hacia
  src/LLVM/LLVMCodeGenFileType.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0843 — Extraer el enum LLVMVerifierFailureAction desde src/LLVM/LlvmCApi.h:219-224 hacia
  src/LLVM/LLVMVerifierFailureAction.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante
  el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0844 — Extraer el enum LLVMLinkage desde src/LLVM/LlvmCApi.h:229-241 hacia src/LLVM/LLVMLinkage.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0845 — Extraer el enum LLVMComdatSelectionKind desde src/LLVM/LlvmCApi.h:243-250 hacia
  src/LLVM/LLVMComdatSelectionKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0846 — Extraer el enum LLVMInlineAsmDialect desde src/LLVM/LlvmCApi.h:252-256 hacia
  src/LLVM/LLVMInlineAsmDialect.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0847 — Extraer el enum LLVMFastMathFlags desde src/LLVM/LlvmCApi.h:781-794 hacia
  src/LLVM/LLVMFastMathFlags.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0854 — Descomponer lowerExpressionIterative en src/LLVM/ProjectOopLlvmLowerer.cpp:10657 (2967 líneas
  ejecutables; 3949 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0869 — Descomponer lowerExpression en src/LLVM/ProjectOopLlvmLowerer.cpp:14607 (565 líneas ejecutables;
  803 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0873 — Descomponer lowerStatement en src/LLVM/ProjectOopLlvmLowerer.cpp:8413 (502 líneas ejecutables; 698
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0875 — Descomponer declareRuntime en src/LLVM/ProjectOopLlvmLowerer.cpp:3051 (481 líneas ejecutables; 646
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0878 — Descomponer ProjectOopObjectGenerator::generate en src/LLVM/ProjectOopObjectGenerator.cpp:390 (376
  líneas ejecutables; 506 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0890 — Descomponer lowerAwaitExpression en src/LLVM/ProjectOopLlvmLowerer.cpp:10231 (307 líneas
  ejecutables; 422 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0902 — Descomponer convert en src/LLVM/ProjectOopLlvmLowerer.cpp:21426 (267 líneas ejecutables; 334
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0907 — Descomponer applyBinaryOperation en src/LLVM/ProjectOopLlvmLowerer.cpp:18237 (256 líneas
  ejecutables; 317 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0921 — Descomponer lowerMatchExpression en src/LLVM/ProjectOopLlvmLowerer.cpp:18609 (209 líneas
  ejecutables; 298 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0925 — Descomponer ProjectOopCodePartition::build en src/LLVM/ProjectOopCodePartition.cpp:127 (200 líneas
  ejecutables; 288 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0927 — Descomponer lowerTryRegion en src/LLVM/ProjectOopLlvmLowerer.cpp:9154 (197 líneas ejecutables; 357
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0929 — Descomponer lowerInlineAssembly en src/LLVM/ProjectOopLlvmLowerer.cpp:7926 (196 líneas
  ejecutables; 270 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0931 — Descomponer lowerInstruction en src/LLVM/LlvmLowerer.cpp:631 (194 líneas ejecutables; 307 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [ ] NSC-IMPL-0932 — Descomponer defineFunction en src/LLVM/ProjectOopLlvmLowerer.cpp:5915 (194 líneas ejecutables; 263
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0933 — Descomponer debugType en src/LLVM/ProjectOopLlvmLowerer.cpp:2424 (191 líneas ejecutables; 241
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0944 — Descomponer LlvmOptimizer::optimizeImpl en src/LLVM/LlvmOptimizer.cpp:252 (167 líneas ejecutables;
  278 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0947 — Descomponer defineTypeInitializers en src/LLVM/ProjectOopLlvmLowerer.cpp:3979 (165 líneas
  ejecutables; 259 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0958 — Descomponer callbackGuardThunk en src/LLVM/ProjectOopLlvmLowerer.cpp:5109 (148 líneas ejecutables;
  211 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0962 — Descomponer lowerPlacementConstruction en src/LLVM/ProjectOopLlvmLowerer.cpp:20919 (145 líneas
  ejecutables; 192 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0978 — Descomponer defineDestructionEntries en src/LLVM/ProjectOopLlvmLowerer.cpp:5729 (131 líneas
  ejecutables; 185 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0979 — Descomponer lowerStructuredExit en src/LLVM/ProjectOopLlvmLowerer.cpp:7483 (130 líneas
  ejecutables; 222 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0983 — Descomponer defineRttiHelpers en src/LLVM/ProjectOopLlvmLowerer.cpp:4649 (127 líneas ejecutables;
  155 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0995 — Descomponer closureContextDestroy en src/LLVM/ProjectOopLlvmLowerer.cpp:15632 (118 líneas
  ejecutables; 147 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0996 — Descomponer lowerArrayAllocate en src/LLVM/ProjectOopLlvmLowerer.cpp:17787 (118 líneas
  ejecutables; 148 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1019 — Descomponer runCurrentExecutableWithCapturedStandardError en src/LLVM/LlvmProcessTrace.cpp:94 (104
  líneas ejecutables; 141 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1020 — Descomponer emitDispatchMetadata en src/LLVM/ProjectOopLlvmLowerer.cpp:4493 (104 líneas
  ejecutables; 155 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1029 — Descomponer emitVariantDropAddress en src/LLVM/ProjectOopLlvmLowerer.cpp:6931 (101 líneas
  ejecutables; 124 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1038 — Descomponer lowerResultPropagation en src/LLVM/ProjectOopLlvmLowerer.cpp:19113 (96 líneas
  ejecutables; 113 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1040 — Descomponer emitDelegateCall en src/LLVM/ProjectOopLlvmLowerer.cpp:16141 (94 líneas ejecutables;
  118 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1041 — Descomponer lowerConstruction en src/LLVM/ProjectOopLlvmLowerer.cpp:20769 (93 líneas ejecutables;
  149 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1045 — Descomponer initializeAggregateIntoSlot en src/LLVM/ProjectOopLlvmLowerer.cpp:20613 (92 líneas
  ejecutables; 128 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1060 — Descomponer emitInterfaceDispose en src/LLVM/ProjectOopLlvmLowerer.cpp:6634 (87 líneas
  ejecutables; 105 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1061 — Descomponer lowerLeafExpression en src/LLVM/ProjectOopLlvmLowerer.cpp:9831 (87 líneas ejecutables;
  139 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1062 — Descomponer lowerStringEquals en src/LLVM/ProjectOopLlvmLowerer.cpp:17180 (87 líneas ejecutables;
  103 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1063 — Descomponer lowerVirtualCall en src/LLVM/ProjectOopLlvmLowerer.cpp:20095 (87 líneas ejecutables;
  120 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1064 — Descomponer lowerInterfaceCall en src/LLVM/ProjectOopLlvmLowerer.cpp:20299 (87 líneas ejecutables;
  119 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1067 — Descomponer declareFunctions en src/LLVM/ProjectOopLlvmLowerer.cpp:4239 (86 líneas ejecutables;
  149 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1070 — Descomponer emitValueDropAddress en src/LLVM/ProjectOopLlvmLowerer.cpp:7056 (85 líneas
  ejecutables; 112 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1075 — Descomponer defineTypedCatchFilters en src/LLVM/ProjectOopLlvmLowerer.cpp:4820 (84 líneas
  ejecutables; 118 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1076 — Descomponer defineRuntimeBoundImport en src/LLVM/ProjectOopLlvmLowerer.cpp:5366 (84 líneas
  ejecutables; 129 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1092 — Descomponer defineCAbiWrappers en src/LLVM/ProjectOopLlvmLowerer.cpp:5530 (80 líneas ejecutables;
  135 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1093 — Descomponer lowerRangeArrayAllocate en src/LLVM/ProjectOopLlvmLowerer.cpp:17939 (80 líneas
  ejecutables; 97 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1100 — Descomponer projectOopCoordinatorFunctionDeclarations en
  src/LLVM/ProjectOopFunctionReachability.cpp:181 (78 líneas ejecutables; 97 totales) hasta que cada función quede en
  ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1107 — Descomponer emitExternCDelegateCall en src/LLVM/ProjectOopLlvmLowerer.cpp:16260 (77 líneas
  ejecutables; 100 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1124 — Descomponer createDebugSubprogram en src/LLVM/ProjectOopLlvmLowerer.cpp:1708 (75 líneas
  ejecutables; 86 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1125 — Descomponer lowerExceptionExpression en src/LLVM/ProjectOopLlvmLowerer.cpp:10064 (75 líneas
  ejecutables; 109 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1128 — Descomponer emitExceptionalCleanupChain en src/LLVM/ProjectOopLlvmLowerer.cpp:6424 (74 líneas
  ejecutables; 126 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1131 — Descomponer debugClassPointee en src/LLVM/ProjectOopLlvmLowerer.cpp:2023 (73 líneas ejecutables;
  91 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1132 — Descomponer collectLocalAllocas en src/LLVM/ProjectOopLlvmLowerer.cpp:6220 (73 líneas ejecutables;
  98 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1133 — Descomponer storeFromParts en src/LLVM/ProjectOopLlvmLowerer.cpp:17640 (73 líneas ejecutables; 97
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1141 — Descomponer emitInterfaceMapGlobals en src/LLVM/ProjectOopLlvmLowerer.cpp:882 (71 líneas
  ejecutables; 94 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1142 — Descomponer loadInterfaceTarget en src/LLVM/ProjectOopLlvmLowerer.cpp:20216 (71 líneas
  ejecutables; 82 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1149 — Descomponer projectOopReachableDefinitionMask en src/LLVM/ProjectOopFunctionReachability.cpp:13
  (69 líneas ejecutables; 90 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control
  de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1150 — Descomponer emitLocalDebugInfo en src/LLVM/ProjectOopLlvmLowerer.cpp:668 (69 líneas ejecutables;
  76 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1151 — Descomponer declareStaticFields en src/LLVM/ProjectOopLlvmLowerer.cpp:3714 (69 líneas ejecutables;
  105 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1152 — Descomponer beginStaticLocalInitialization en src/LLVM/ProjectOopLlvmLowerer.cpp:8197 (69 líneas
  ejecutables; 84 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1153 — Descomponer lowerEnumName en src/LLVM/ProjectOopLlvmLowerer.cpp:16876 (69 líneas ejecutables; 111
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1154 — Descomponer lowerCall en src/LLVM/ProjectOopLlvmLowerer.cpp:19782 (69 líneas ejecutables; 99
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1156 — Descomponer lowerLambdaCreate en src/LLVM/ProjectOopLlvmLowerer.cpp:15978 (68 líneas ejecutables;
  93 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1157 — Descomponer lowerOptionalCoalescing en src/LLVM/ProjectOopLlvmLowerer.cpp:18998 (68 líneas
  ejecutables; 85 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1168 — Descomponer emitParameterDebugInfo en src/LLVM/ProjectOopLlvmLowerer.cpp:587 (65 líneas
  ejecutables; 78 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1172 — Descomponer emitNativeArrayDestroy en src/LLVM/ProjectOopLlvmLowerer.cpp:6802 (64 líneas
  ejecutables; 80 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1173 — Descomponer emitRaiiDestroy en src/LLVM/ProjectOopLlvmLowerer.cpp:7195 (64 líneas ejecutables; 108
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1182 — Descomponer debugPlacementType en src/LLVM/ProjectOopLlvmLowerer.cpp:2250 (63 líneas ejecutables;
  79 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1183 — Descomponer lowerExceptionalTransfer en src/LLVM/ProjectOopLlvmLowerer.cpp:7733 (63 líneas
  ejecutables; 110 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1201 — Descomponer CompilationArtifactCache::restoreObject en src/LLVM/CompilationArtifactCache.cpp:551
  (61 líneas ejecutables; 81 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control
  de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1202 — Descomponer emitDescriptorInitializer en src/LLVM/ProjectOopLlvmLowerer.cpp:1008 (61 líneas
  ejecutables; 82 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1203 — Descomponer ensureRuntimeDebugTrace en src/LLVM/ProjectOopLlvmLowerer.cpp:2834 (61 líneas
  ejecutables; 80 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1204 — Descomponer declareTypeInitializers en src/LLVM/ProjectOopLlvmLowerer.cpp:3872 (61 líneas
  ejecutables; 106 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1205 — Descomponer compoundStoreFromParts en src/LLVM/ProjectOopLlvmLowerer.cpp:17489 (61 líneas
  ejecutables; 81 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1210 — Cerrar OopLlvmImplementation como fachada interna y familias de lowering, sin mover estado ni
  alterar la fachada pública.
- [ ] NSC-IMPL-1214 — Partir LlvmLowerer::Implementation por expresiones, sentencias y tipos, conservando el pipeline
  heredado y todos sus campos.

### Commit REF-08 — Driver, comandos de proyecto y superficie pública — 0/84

Divide Driver y ProjectCommands conservando CLI, ayuda, orden de fases y superficies generadas.

- [ ] NSC-IMPL-0019 — Extraer el struct InvocationCapabilities desde src/Driver/CompilerInvocation.cpp:168-173 (6) hacia
  src/Driver/Internal/InvocationCapabilities.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0020 — Extraer el struct OptionResult desde src/Driver/CompilerOptionParser.cpp:16-20 (5) hacia
  src/Driver/Internal/OptionResult.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0021 — Extraer el struct OptionContext desde src/Driver/CompilerOptionParser.cpp:22-30 (9) hacia
  src/Driver/Internal/OptionContext.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0022 — Extraer el enum MillisecondsParseResult desde src/Driver/CompilerOptionParser.cpp:56-61 (6) hacia
  src/Driver/Internal/MillisecondsParseResult.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0023 — Extraer el struct InputIdentity desde src/Driver/DiagnosticBundle.cpp:18-22 (5) hacia
  src/Driver/Internal/InputIdentity.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0024 — Extraer el struct DriverExecutionScope desde src/Driver/Driver.cpp:146-241 (96) hacia
  src/Driver/Internal/DriverExecutionScope.h, y mover sus cuerpos a src/Driver/Internal/DriverExecutionScope.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0025 — Extraer el class DriverPhaseTimer desde src/Driver/Driver.cpp:261-293 (33) hacia
  src/Driver/Internal/DriverPhaseTimer.h, y mover sus cuerpos a src/Driver/Internal/DriverPhaseTimer.cpp; conservar
  texto, comentarios, firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0026 — Extraer el struct LayoutRow desde src/Driver/Driver.cpp:366-370 (5) hacia
  src/Driver/Internal/LayoutRow.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0027 — Extraer el struct PrecompiledExportContract desde src/Driver/Driver.cpp:398-404 (7) hacia
  src/Driver/Internal/PrecompiledExportContract.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0028 — Extraer el struct LoadedPrecompiledMetadata desde src/Driver/Driver.cpp:406-411 (6) hacia
  src/Driver/Internal/LoadedPrecompiledMetadata.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0029 — Extraer el struct ManifestEntry desde src/Driver/InstallationManifest.cpp:19-23 (5) hacia
  src/Driver/Internal/ManifestEntry.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0030 — Extraer el struct RequiredFile desde src/Driver/InstallationVerifier.cpp:18-21 (4) hacia
  src/Driver/Internal/RequiredFile.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0031 — Extraer el struct Token desde src/Driver/MigrationAnalyzer.cpp:19-27 (9) hacia
  src/Driver/Internal/Token.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta. Restricción
  de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0032 — Extraer el struct ManifestEntry desde src/Driver/PackageLifecycle.cpp:21-26 (6) hacia
  src/Driver/Internal/ManifestEntry.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0033 — Extraer el struct PlanEntry desde src/Driver/PackageLifecycle.cpp:28-32 (5) hacia
  src/Driver/Internal/PlanEntry.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0034 — Extraer el class ProgressJsonForwardingBuffer desde src/Driver/ProjectCommands.cpp:44-108 (65)
  hacia src/Driver/Internal/ProgressJsonForwardingBuffer.h, y mover sus cuerpos a
  src/Driver/Internal/ProgressJsonForwardingBuffer.cpp; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0035 — Extraer el struct Json desde src/Driver/ProjectCommands.cpp:160-170 (11) hacia
  src/Driver/Internal/Json.h, y mover sus cuerpos a src/Driver/Internal/Json.cpp; conservar texto, comentarios, firmas,
  orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0036 — Extraer el class JsonParser desde src/Driver/ProjectCommands.cpp:172-332 (161) hacia
  src/Driver/Internal/JsonParser.h, y mover sus cuerpos a src/Driver/Internal/JsonParser.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0037 — Extraer el struct PackageRequest desde src/Driver/ProjectCommands.cpp:392-392 (1) hacia
  src/Driver/Internal/PackageRequest.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0038 — Extraer el struct BuildPreferences desde src/Driver/ProjectCommands.cpp:393-401 (9) hacia
  src/Driver/Internal/BuildPreferences.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0039 — Extraer el struct Sidecar desde src/Driver/ProjectCommands.cpp:402-409 (8) hacia
  src/Driver/Internal/Sidecar.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta. Restricción
  de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0040 — Extraer el struct CommandOutput desde src/Driver/ProjectCommands.cpp:532-563 (32) hacia
  src/Driver/Internal/CommandOutput.h, y mover sus cuerpos a src/Driver/Internal/CommandOutput.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0041 — Extraer el struct CommonArguments desde src/Driver/ProjectCommands.cpp:565-569 (5) hacia
  src/Driver/Internal/CommonArguments.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0042 — Extraer el struct Artifact desde src/Driver/ProjectCommands.cpp:640-647 (8) hacia
  src/Driver/Internal/Artifact.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0043 — Extraer el struct RestoredPackage desde src/Driver/ProjectCommands.cpp:648-648 (1) hacia
  src/Driver/Internal/RestoredPackage.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0044 — Extraer el struct RestorePlan desde src/Driver/ProjectCommands.cpp:649-649 (1) hacia
  src/Driver/Internal/RestorePlan.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0045 — Extraer el struct RestoreOptions desde src/Driver/ProjectCommands.cpp:888-895 (8) hacia
  src/Driver/Internal/RestoreOptions.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0046 — Extraer el class TemporaryFile desde src/Driver/ProjectCommands.cpp:1030-1037 (8) hacia
  src/Driver/Internal/TemporaryFile.h, y mover sus cuerpos a src/Driver/Internal/TemporaryFile.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0047 — Extraer el class EnvironmentOverride desde src/Driver/ProjectCommands.cpp:1146-1181 (36) hacia
  src/Driver/Internal/EnvironmentOverride.h, y mover sus cuerpos a src/Driver/Internal/EnvironmentOverride.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0048 — Extraer el struct BuildOptions desde src/Driver/ProjectCommands.cpp:1183-1198 (16) hacia
  src/Driver/Internal/BuildOptions.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0049 — Extraer el struct BuildResult desde src/Driver/ProjectCommands.cpp:1342-1342 (1) hacia
  src/Driver/Internal/BuildResult.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0050 — Extraer el struct DependencyProvenance desde src/Driver/ProjectCommands.cpp:1344-1350 (7) hacia
  src/Driver/Internal/DependencyProvenance.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y
  conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0051 — Extraer el struct ExplainTopic desde src/Driver/ProjectCommands.cpp:1806-1811 (6) hacia
  src/Driver/Internal/ExplainTopic.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0246 — Conservar el class CompilationJobPolicy como tipo principal de
  include/nativesharp/Driver/CompilationJobPolicy.h:10-29 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0247 — Extraer el class CompilationJobPolicyScope desde
  include/nativesharp/Driver/CompilationJobPolicy.h:31-42 hacia include/nativesharp/Driver/CompilationJobPolicyScope.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0248 — Conservar el enum CompilationProfile como tipo principal de
  include/nativesharp/Driver/CompilationProfile.h:11-15 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0249 — Extraer el struct CompilationProfileSettings desde
  include/nativesharp/Driver/CompilationProfile.h:17-23 hacia include/nativesharp/Driver/CompilationProfileSettings.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0250 — Extraer el enum CompilerLanguage desde include/nativesharp/Driver/CompilerConsole.h:11-15 hacia
  include/nativesharp/Driver/CompilerLanguage.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0251 — Extraer el struct CompilerConsoleArguments desde
  include/nativesharp/Driver/CompilerConsole.h:17-22 hacia include/nativesharp/Driver/CompilerConsoleArguments.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0252 — Extraer el class CompilerLanguageScope desde include/nativesharp/Driver/CompilerConsole.h:30-41
  hacia include/nativesharp/Driver/CompilerLanguageScope.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0253 — Extraer el enum DeprecationKind desde include/nativesharp/Driver/DeprecationPolicy.h:12-12 hacia
  include/nativesharp/Driver/DeprecationKind.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0254 — Extraer el enum DeprecationState desde include/nativesharp/Driver/DeprecationPolicy.h:13-13 hacia
  include/nativesharp/Driver/DeprecationState.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0255 — Extraer el struct DeprecationRelease desde include/nativesharp/Driver/DeprecationPolicy.h:15-19
  hacia include/nativesharp/Driver/DeprecationRelease.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0256 — Extraer el struct DeprecatedFeature desde include/nativesharp/Driver/DeprecationPolicy.h:21-28
  hacia include/nativesharp/Driver/DeprecatedFeature.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0257 — Extraer el struct DeprecationDecision desde include/nativesharp/Driver/DeprecationPolicy.h:30-34
  hacia include/nativesharp/Driver/DeprecationDecision.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0258 — Extraer el struct ExecutableMigrationStep desde
  include/nativesharp/Driver/ExecutableMigrationGuide.h:11-17 hacia
  include/nativesharp/Driver/ExecutableMigrationStep.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0259 — Extraer el struct ExecutableMigrationChapter desde
  include/nativesharp/Driver/ExecutableMigrationGuide.h:19-24 hacia
  include/nativesharp/Driver/ExecutableMigrationChapter.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0260 — Conservar el struct ExecutableMigrationGuide como tipo principal de
  include/nativesharp/Driver/ExecutableMigrationGuide.h:26-30 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0261 — Extraer el struct MigrationTextEdit desde include/nativesharp/Driver/MigrationAnalyzer.h:14-20
  hacia include/nativesharp/Driver/MigrationTextEdit.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0262 — Extraer el struct MigrationFinding desde include/nativesharp/Driver/MigrationAnalyzer.h:22-32
  hacia include/nativesharp/Driver/MigrationFinding.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0263 — Extraer el struct MigrationAnalysis desde include/nativesharp/Driver/MigrationAnalyzer.h:34-39
  hacia include/nativesharp/Driver/MigrationAnalysis.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0264 — Extraer el struct MigrationCompatibilityEntry desde
  include/nativesharp/Driver/MigrationCompatibilityMatrix.h:10-18 hacia
  include/nativesharp/Driver/MigrationCompatibilityEntry.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0265 — Conservar el struct MigrationCompatibilityMatrix como tipo principal de
  include/nativesharp/Driver/MigrationCompatibilityMatrix.h:20-31 y convertir ese archivo en umbrella de sus tipos
  extraídos, sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0819 — Extraer el struct ObjectPublication desde src/Driver/ArtifactPublisher.h:11-15 hacia
  src/Driver/ObjectPublication.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0820 — Extraer el struct ExecutablePublication desde src/Driver/ArtifactPublisher.h:17-25 hacia
  src/Driver/ExecutablePublication.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante el
  umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0821 — Extraer el struct StaticLibraryPublication desde src/Driver/ArtifactPublisher.h:27-35 hacia
  src/Driver/StaticLibraryPublication.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante
  el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0822 — Extraer el struct DynamicLibraryPublication desde src/Driver/ArtifactPublisher.h:37-49 hacia
  src/Driver/DynamicLibraryPublication.h; conservar texto, comentarios, orden de declaración y cierre transitivo
  mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0823 — Conservar el class ArtifactPublisher como tipo principal de src/Driver/ArtifactPublisher.h:52-100
  y convertir ese archivo en umbrella de sus tipos extraídos, sin alterar el cierre transitivo. Restricción de pruebas:
  ninguna.
- [ ] NSC-IMPL-0858 — Descomponer PublicSurfaceEmitter::build en src/Driver/PublicSurfaceEmitter.cpp:28 (1305 líneas
  ejecutables; 1620 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0892 — Descomponer Driver::printHelp en src/Driver/Driver.cpp:2892 (299 líneas ejecutables; 304 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [ ] NSC-IMPL-0898 — Descomponer Driver::runProject en src/Driver/Driver.cpp:3500 (275 líneas ejecutables; 414 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [ ] NSC-IMPL-0905 — Descomponer Driver::runProjectOopDynamicLibraryGeneration en src/Driver/Driver.cpp:4474 (258
  líneas ejecutables; 351 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0917 — Descomponer configurePrecompiledModules en src/Driver/Driver.cpp:1672 (220 líneas ejecutables; 301
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0928 — Descomponer appendValidatedLibraryReferences en src/Driver/Driver.cpp:2002 (196 líneas
  ejecutables; 295 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0948 — Descomponer Driver::runProjectOopExecutableGeneration en src/Driver/Driver.cpp:4081 (163 líneas
  ejecutables; 223 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0957 — Descomponer executeBuild en src/Driver/ProjectCommands.cpp:1431 (150 líneas ejecutables; 179
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0965 — Descomponer Driver::runFrontend en src/Driver/Driver.cpp:5279 (140 líneas ejecutables; 240
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0977 — Descomponer Driver::runProjectOopStaticLibraryGeneration en src/Driver/Driver.cpp:4305 (131 líneas
  ejecutables; 168 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0981 — Descomponer applyInstallationRemovalPlan en src/Driver/PackageLifecycle.cpp:545 (128 líneas
  ejecutables; 218 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1001 — Descomponer Driver::verifyLibraryArtifacts en src/Driver/Driver.cpp:5116 (114 líneas ejecutables;
  162 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1069 — Descomponer renderCompilationTimeTraceJson en src/Driver/CompilationTelemetry.cpp:139 (85 líneas
  ejecutables; 103 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1071 — Descomponer Driver::runProjectOopCodeGeneration en src/Driver/Driver.cpp:3915 (84 líneas
  ejecutables; 121 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1088 — Descomponer analyzeMigrationSource en src/Driver/MigrationAnalyzer.cpp:228 (80 líneas ejecutables;
  117 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1111 — Descomponer publishContentDirectory en src/Driver/ProjectCommands.cpp:1039 (76 líneas ejecutables;
  106 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1117 — Descomponer tokenize en src/Driver/MigrationAnalyzer.cpp:41 (75 líneas ejecutables; 112 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [ ] NSC-IMPL-1126 — Descomponer verifyInstallation en src/Driver/InstallationVerifier.cpp:52 (74 líneas ejecutables;
  103 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1135 — Descomponer Driver::runCodeGeneration en src/Driver/Driver.cpp:3230 (72 líneas ejecutables; 119
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1158 — Descomponer sourceTypeForMetadataKey en src/Driver/Driver.cpp:447 (67 líneas ejecutables; 101
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1166 — Descomponer localizeCompilerMessage en src/Driver/CompilerConsole.cpp:108 (65 líneas ejecutables;
  74 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1169 — Descomponer writeTextAtomically en src/Driver/PackageLifecycle.cpp:167 (64 líneas ejecutables; 94
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1176 — Descomponer verifyToolchainContract en src/Driver/ToolchainContract.cpp:122 (63 líneas
  ejecutables; 84 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1184 — Descomponer readPlan en src/Driver/PackageLifecycle.cpp:354 (62 líneas ejecutables; 92 totales)
  hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni
  resultados.
- [ ] NSC-IMPL-1196 — Descomponer writeDiagnosticBundle en src/Driver/DiagnosticBundle.cpp:50 (61 líneas ejecutables; 81
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1218 — Partir Driver.cpp según los rangos medidos, conservando Driver como fachada y la salida de ayuda
  byte-idéntica.

### Commit REF-09 — Linker, runtime y ejecutable — 0/55

Ordena linker, runtime y entrada ejecutable sin cambiar artefactos, símbolos, mensajes ni enlazado.

- [ ] NSC-IMPL-0113 — Extraer el struct SectionView desde src/Linker/PeExecutableVerifier.cpp:58-64 (7) hacia
  src/Linker/Internal/SectionView.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0114 — Extraer el class TemporaryDirectory desde src/Linker/WindowsExecutableBuilder.cpp:76-106 (31)
  hacia src/Linker/Internal/TemporaryDirectory.h, y mover sus cuerpos a src/Linker/Internal/TemporaryDirectory.cpp;
  conservar texto, comentarios, firmas, orden y conducta. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0115 — Extraer el class TemporaryDirectory desde src/Linker/WindowsLibraryBuilder.cpp:29-53 (25) hacia
  src/Linker/Internal/TemporaryDirectory.h, y mover sus cuerpos a src/Linker/Internal/TemporaryDirectory.cpp; conservar
  texto, comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0116 — Extraer el class TemporaryFiles desde src/Linker/WindowsRuntimeLibrary.cpp:30-52 (23) hacia
  src/Linker/Internal/TemporaryFiles.h, y mover sus cuerpos a src/Linker/Internal/TemporaryFiles.cpp; conservar texto,
  comentarios, firmas, orden y conducta. Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0129 — Extraer el struct ActualSymbol desde src/Runtime/RuntimeAbiContract.cpp:16-20 (5) hacia
  src/Runtime/Internal/ActualSymbol.h, sin .cpp adicional; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: lista del suite (A-1).
- [ ] NSC-IMPL-0130 — Extraer el class TeeBuffer desde src/nsc/main.cpp:36-61 (26) hacia src/nsc/Internal/TeeBuffer.h, y
  mover sus cuerpos a src/nsc/Internal/TeeBuffer.cpp; conservar texto, comentarios, firmas, orden y conducta.
  Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0777 — Extraer el struct CoffArchiveInfo desde include/nativesharp/Linker/CoffArchiveVerifier.h:10-14
  hacia include/nativesharp/Linker/CoffArchiveInfo.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0778 — Conservar el class CoffArchiveVerifier como tipo principal de
  include/nativesharp/Linker/CoffArchiveVerifier.h:16-26 y convertir ese archivo en umbrella de sus tipos extraídos, sin
  alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0779 — Extraer el enum WindowsExecutableSubsystem desde include/nativesharp/Linker/LldLinker.h:11-15
  hacia include/nativesharp/Linker/WindowsExecutableSubsystem.h; conservar texto, comentarios, orden de declaración y
  cierre transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0780 — Conservar el class LldLinker como tipo principal de include/nativesharp/Linker/LldLinker.h:17-74 y
  convertir ese archivo en umbrella de sus tipos extraídos, sin alterar el cierre transitivo. Restricción de pruebas:
  ninguna.
- [ ] NSC-IMPL-0781 — Extraer el struct PdbInfo desde include/nativesharp/Linker/PdbVerifier.h:10-21 hacia
  include/nativesharp/Linker/PdbInfo.h; conservar texto, comentarios, orden de declaración y cierre transitivo mediante
  el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0782 — Conservar el class PdbVerifier como tipo principal de
  include/nativesharp/Linker/PdbVerifier.h:23-33 y convertir ese archivo en umbrella de sus tipos extraídos, sin alterar
  el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0783 — Extraer el struct PeImportedSymbol desde include/nativesharp/Linker/PeExecutableVerifier.h:11-15
  hacia include/nativesharp/Linker/PeImportedSymbol.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0784 — Extraer el struct PeExecutableInfo desde include/nativesharp/Linker/PeExecutableVerifier.h:17-40
  hacia include/nativesharp/Linker/PeExecutableInfo.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0785 — Conservar el class PeExecutableVerifier como tipo principal de
  include/nativesharp/Linker/PeExecutableVerifier.h:42-55 y convertir ese archivo en umbrella de sus tipos extraídos,
  sin alterar el cierre transitivo. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0792 — Extraer el enum RuntimeAbiSymbolKind desde include/nativesharp/Runtime/RuntimeAbiContract.h:20-24
  hacia include/nativesharp/Runtime/RuntimeAbiSymbolKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0793 — Extraer el enum RuntimeAbiImageKind desde include/nativesharp/Runtime/RuntimeAbiContract.h:26-31
  hacia include/nativesharp/Runtime/RuntimeAbiImageKind.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0794 — Extraer el struct RuntimeAbiSymbol desde include/nativesharp/Runtime/RuntimeAbiContract.h:33-40
  hacia include/nativesharp/Runtime/RuntimeAbiSymbol.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0795 — Extraer el enum RuntimeCapabilityStatus desde
  include/nativesharp/Runtime/RuntimeCapabilityReport.h:9-14 hacia
  include/nativesharp/Runtime/RuntimeCapabilityStatus.h; conservar texto, comentarios, orden de declaración y cierre
  transitivo mediante el umbrella original. Restricción de pruebas: ninguna.
- [ ] NSC-IMPL-0796 — Extraer el struct RuntimeCapability desde
  include/nativesharp/Runtime/RuntimeCapabilityReport.h:16-23 hacia include/nativesharp/Runtime/RuntimeCapability.h;
  conservar texto, comentarios, orden de declaración y cierre transitivo mediante el umbrella original. Restricción de
  pruebas: ninguna.
- [ ] NSC-IMPL-0872 — Descomponer WindowsRuntimeBuilder::buildInternal en src/Runtime/WindowsRuntimeBuilder.cpp:5840
  (509 líneas ejecutables; 703 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0908 — Descomponer PeExecutableVerifier::verifyImage en src/Linker/PeExecutableVerifier.cpp:129 (241
  líneas ejecutables; 374 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0934 — Descomponer runCompiler en src/nsc/main.cpp:272 (190 líneas ejecutables; 237 totales) hasta que
  cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0935 — Descomponer buildConsoleReadLine en src/Runtime/WindowsRuntimeBuilder.cpp:415 (187 líneas
  ejecutables; 208 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0960 — Descomponer buildEnvironmentGetCommandLineArgs en src/Runtime/WindowsRuntimeBuilder.cpp:2508 (145
  líneas ejecutables; 188 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0964 — Descomponer buildPuts en src/Runtime/WindowsRuntimeBuilder.cpp:5233 (142 líneas ejecutables; 158
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0984 — Descomponer WindowsLibraryBuilder::buildStaticObjects en src/Linker/WindowsLibraryBuilder.cpp:495
  (126 líneas ejecutables; 166 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0986 — Descomponer WindowsLibraryBuilder::buildDynamic en src/Linker/WindowsLibraryBuilder.cpp:294 (124
  líneas ejecutables; 200 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0988 — Descomponer buildProcessRun en src/Runtime/WindowsRuntimeBuilder.cpp:1940 (123 líneas ejecutables;
  173 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-0989 — Descomponer createWindowsImportLibraries en src/Linker/WindowsRuntimeImports.cpp:21 (123 líneas
  ejecutables; 247 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-0997 — Descomponer PdbVerifier::verify en src/Linker/PdbVerifier.cpp:56 (117 líneas ejecutables; 171
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1002 — Descomponer LldLinker::runResponseFile en src/Linker/LldLinker.cpp:398 (114 líneas ejecutables;
  151 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1007 — Descomponer buildDirectoryGetEntries en src/Runtime/WindowsRuntimeBuilder.cpp:3009 (110 líneas
  ejecutables; 131 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1009 — Descomponer WindowsExecutableBuilder::build en src/Linker/WindowsExecutableBuilder.cpp:280 (109
  líneas ejecutables; 190 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1013 — Descomponer WindowsExecutableBuilder::buildObjects en src/Linker/WindowsExecutableBuilder.cpp:471
  (107 líneas ejecutables; 154 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1035 — Descomponer WindowsLibraryBuilder::buildDynamicObjects en src/Linker/WindowsLibraryBuilder.cpp:662
  (97 líneas ejecutables; 137 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden,
  control de flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1046 — Descomponer buildHalfCompilerRtSupport en src/Runtime/WindowsRuntimeBuilder.cpp:4952 (91 líneas
  ejecutables; 96 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1058 — Descomponer buildEhFilter en src/Runtime/WindowsRuntimeBuilder.cpp:5618 (87 líneas ejecutables;
  106 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1059 — Descomponer LldLinker::linkDynamicLibrary en src/Linker/LldLinker.cpp:729 (87 líneas ejecutables;
  130 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1072 — Descomponer buildPathGetFullPath en src/Runtime/WindowsRuntimeBuilder.cpp:2307 (84 líneas
  ejecutables; 110 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1083 — Descomponer buildDirectoryGetEntryCount en src/Runtime/WindowsRuntimeBuilder.cpp:2900 (81 líneas
  ejecutables; 98 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1099 — Descomponer buildFileReadAllText en src/Runtime/WindowsRuntimeBuilder.cpp:1294 (78 líneas
  ejecutables; 110 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1112 — Descomponer buildConsoleWrite en src/Runtime/WindowsRuntimeBuilder.cpp:321 (76 líneas ejecutables;
  93 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1119 — Descomponer buildFileReadAllBytes en src/Runtime/WindowsRuntimeBuilder.cpp:1520 (75 líneas
  ejecutables; 93 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1136 — Descomponer CoffArchiveVerifier::verify en src/Linker/CoffArchiveVerifier.cpp:101 (72 líneas
  ejecutables; 117 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1143 — Descomponer WindowsLibraryBuilder::buildStatic en src/Linker/WindowsLibraryBuilder.cpp:170 (70
  líneas ejecutables; 123 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de
  flujo, estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1144 — Descomponer buildFileWriteAllBytes en src/Runtime/WindowsRuntimeBuilder.cpp:1722 (69 líneas
  ejecutables; 85 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1145 — Descomponer writeWindowsRuntimeLibrary en src/Linker/WindowsRuntimeLibrary.cpp:140 (69 líneas
  ejecutables; 102 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1159 — Descomponer buildStart en src/Runtime/WindowsRuntimeBuilder.cpp:221 (67 líneas ejecutables; 81
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1167 — Descomponer buildFileReadBytes en src/Runtime/WindowsRuntimeBuilder.cpp:1629 (65 líneas
  ejecutables; 79 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1177 — Descomponer buildThrow en src/Runtime/WindowsRuntimeBuilder.cpp:5543 (63 líneas ejecutables; 74
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1187 — Descomponer LldLinker::createStaticLibrary en src/Linker/LldLinker.cpp:635 (62 líneas ejecutables;
  93 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1188 — Descomponer LldLinker::linkExecutable en src/Linker/LldLinker.cpp:870 (62 líneas ejecutables; 89
  totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo, estado,
  algoritmo ni resultados.
- [ ] NSC-IMPL-1197 — Descomponer buildFileAppendAllText en src/Runtime/WindowsRuntimeBuilder.cpp:2195 (61 líneas
  ejecutables; 83 totales) hasta que cada función quede en ≤60 líneas ejecutables, sin cambiar orden, control de flujo,
  estado, algoritmo ni resultados.
- [ ] NSC-IMPL-1220 — Convertir WindowsRuntimeBuilder en fachada y separar sus familias sin cambiar runtime, ABI,
  objetos ni enlazado.

### Commit REF-10 — Utilidades compartidas, includes y cierre transversal — 0/9

Consolida duplicados permitidos, reduce includes internos y ejecuta el cierre estructural del refactor.

- [ ] NSC-IMPL-0848 — Unificar MirByteReader y ByteReader en CachePayloadReader con la unión exacta de métodos y migrar
  ambos llamadores.
- [ ] NSC-IMPL-0849 — Unificar los escritores binarios MIR y genéricos en CachePayloadWriter, preservando bytes
  little-endian idénticos.
- [ ] NSC-IMPL-0850 — Unificar las tres copias del almacén de caché en CacheEntryStore parametrizado, preservando cada
  política y mensaje.
- [ ] NSC-IMPL-0851 — Unificar los directorios temporales de los builders de EXE y LIB en ScopedTemporaryDirectory,
  preservando prefijos, mensajes y keep.
- [ ] NSC-IMPL-1221 — Inventariar después de R1–R6 los includes directos y transitivos de todos los .cpp y headers
  internos del árbol resultante.
- [ ] NSC-IMPL-1222 — Reemplazar en cada .cpp los umbrellas por headers directos de tipo cuando el cierre y la
  compilación permanezcan idénticos.
- [ ] NSC-IMPL-1223 — Reducir includes de headers internos sin quitar ningún include de headers públicos ni alterar su
  cierre transitivo.
- [ ] NSC-IMPL-1224 — Verificar que cada archivo nuevo o movido incluya directamente todas sus dependencias y no dependa
  de orden incidental.
- [ ] NSC-IMPL-1225 — Comparar el .ninja_log limpio posterior con R0 y documentar el efecto informativo de R7 sin usarlo
  como gate funcional.

## Implementación de optimizaciones — 0/2

Este bloque permanece detenido hasta terminar y validar los diez commits del refactor. Cada optimización tendrá
su propio commit posterior y su medición antes/después.

- [ ] NSC-IMPL-1226 — Cachear el objeto runtime y las import libraries con llaves completas, verificación COFF y bytes
  idénticos en frío y caliente.
- [ ] NSC-IMPL-1227 — Optimizar restore con prefiltro conservador de tamaño y fecha antes de SHA-256 y reescribir el
  lock solo cuando cambie.
