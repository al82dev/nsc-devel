# 03 — Plan de modularización y tratamiento método por método

## 1. Regla de extracción

La modularización será horizontal por responsabilidad, no un reparto arbitrario de líneas. En cada
extracción se conserva la fachada pública original, se mueve una familia coherente y se prueba antes
de continuar. Ningún commit mezcla movimiento masivo con cambio algorítmico.

Secuencia obligatoria por familia:

1. congelar caracterización y dumps del comportamiento actual;
2. declarar inputs, outputs y estado leído/escrito;
3. extraer tipos de contexto/resultado sin cambiar conducta;
4. mover helpers puros;
5. mover un dispatcher o una familia de casos;
6. validar equivalencia textual/semántica;
7. sólo en una tanda posterior cambiar el algoritmo.

## 2. `ProjectGenericModel.cpp` — 5,812 líneas

### 2.1 Fachada conservada

`ProjectGenericModel` conserva provisionalmente los métodos públicos `build`, `templates`,
`specializations`, lookups de resultado e `instantiationReport`. Internamente se convierte en
coordinador de servicios.

### 2.2 Archivos propuestos

```text
Frontend/Generics/GenericBuildCoordinator.{h,cpp}
Frontend/Generics/GenericTemplateCatalog.{h,cpp}
Frontend/Generics/GenericTemplateCollector.cpp
Frontend/Generics/GenericTemplateIndex.cpp
Frontend/Generics/GenericVisibility.cpp
Frontend/Generics/GenericScopeStore.{h,cpp}
Frontend/Generics/GenericDependencyScanner.cpp
Frontend/Generics/GenericDemandCollector.cpp
Frontend/Generics/GenericResolver.cpp
Frontend/Generics/GenericInference.cpp
Frontend/Generics/GenericArgumentNormalizer.cpp
Frontend/Generics/GenericSpecializationPlanner.cpp
Frontend/Generics/GenericSpecializationWorker.cpp
Frontend/Generics/GenericAstCloner.cpp
Frontend/Generics/GenericTypeCloner.cpp
Frontend/Generics/GenericAssociatedItems.cpp
Frontend/Generics/GenericTelemetry.cpp
```

### 2.3 Tratamiento de funciones

| Función/familia actual | Cambio concreto | Razón |
|---|---|---|
| `build` | Reducir a validar precondiciones, solicitar snapshot de catálogo, ejecutar coordinador y publicar `GenericBuildResult`. | Hoy mezcla al menos quince pasos y controla estado mutable. |
| `buildVisibility` | Mover a `GenericVisibility`; DFS iterativo con colores `Unseen/Active/Done` y bitsets/sets ordenados por proyecto. | El lambda recursivo actual no escala y `projectVisible` hace búsqueda lineal. |
| `projectVisible` | Consulta `O(1)` en matriz sparse/bitset del snapshot. | Se llama dentro de resoluciones frecuentes. |
| `expandAliasesInNode` | Visitor iterativo separado; produce rewrites, no muta mientras recorre. | El recorrido actual es recursivo y mezcla scope/mutación. |
| `expandTypeAliases` | Pila explícita de tipos y set de alias activos; resultado transaccional. | El vector de expansión simula ciclo pero usa call stack. |
| `cloneAliasType` | Reusar `GenericTypeCloner` con política `AliasExpansion`. | Evita dos clonadores semánticamente cercanos. |
| `mergeTraitImplementations` | Índice `canonical template key -> TemplateId`; adjuntar extensiones por id. | El `find_if` por impl es `O(I×T)`. |
| `collectAssociatedItems` / `registerAssociatedItems` | Servicio `GenericAssociatedItemIndex` construido una vez. | Aísla defaults/proyecciones y permite snapshot inmutable. |
| `collectTemplates` | Collector iterativo por unidad; buffers locales y merge estable. | Elimina recursión y habilita paralelismo sin cambiar ordinals. |
| `discoverTemplateDependencies` | Scheduler por template; cada tarea usa scope snapshot. | Hoy vuelve a caminar unidades completas serialmente. |
| `collectTemplateDependencies` | Frame iterativo que no entra a roots genéricas anidadas. | Preserva la regla actual sin stack C++. |
| `collectTypeDependency` | Resolver indexado y agregar `TemplateId`, no strings duplicadas. | Evita dos barridos y reduce memoria. |
| `collectCallDependency` | Consumir un `GenericMethodResolution` único. | Elimina 3–5 resoluciones del mismo sitio. |
| `stateForSyntax` | Sustituir `find_if` por `bySyntax`. | `O(T)` por consulta pasa a `O(1)` promedio. |
| `processNode` | Retirar. Se divide en `GenericDemandCollector` iterativo y `RewriteApplier`. | Es el monolito que mezcla traversal, scope, inferencia y materialización. |
| `collectBodyTypeSpecializationDemand` | Emite una demanda canónica deduplicada al planner. | Hoy crea Span y ReadOnlySpan aunque el cache colapse después. |
| `materializeBodyTypeSpecializationDemands` | Delegar a ondas del planner. | Mantiene orden estable y permite workers. |
| `processTypeSyntax` | `GenericTypeDemandVisitor`; devuelve `TypeResolution` y `SpecializationId`. | Elimina recursión y búsqueda reversa por nombre. |
| `processGenericCall` | `GenericCallDemandAnalyzer`; resolución única, normalization, demanda y rewrite. | Reduce consultas y responsabilidades. |
| `templatesNamedBy` | Método puro de `GenericResolver` sobre índices. | Conserva ladder pero examina sólo cubetas. |
| `collectMethodTemplates` | Vista del resultado de resolución; no segunda búsqueda. | Evita recalcular. |
| `resolveMethodTemplate` | Política sobre el mismo resultado; nunca recorre catálogo. | Centraliza ambigüedad/fallback. |
| `resolveTypeTemplate` | `GenericTypeResolver` indexado con resultado triestado. | Elimina barridos completos. |
| `inferGenericCall` | Orquestador pequeño: evidence collector + unifier + ranker. | 322 líneas y recursión/inferencia mezcladas. |
| `inferExpressionType` | Evaluador iterativo con memo por `(node, ScopeId)`. | Ocho llamadas recursivas y recomputación. |
| `inferTypeArguments` | Worklist de pares pattern/actual. | Unificación iterativa y testeable. |
| `normalizeGenericArguments` | Servicio puro; salida `NormalizedArguments`. | Actualmente modifica argumentos y diagnostica en el mismo paso. |
| `ensureTypeSpecialization` | Wrapper de `SpecializationPlanner::request(TypeDemand)`. | Deja de clonar/publicar inmediatamente. |
| `ensureMethodSpecialization` | Mismo planner con `MethodDemand` y overload group. | Elimina duplicación con el tipo. |
| `cloneType` | `GenericTypeCloner::cloneIterative`. | Cero recursión; telemetría por nodos. |
| `cloneNode` | `GenericAstCloner::cloneIterative`. | Cero recursión y arena privada por task. |
| `cloneExpandedTypeArgument` | Expansor de packs no recursivo; primero descubre nombres, luego genera items. | El lambda `collect` actual es recursivo y copia mapas por item. |
| `typeSpelling` / `typeSpecializationIdentity` | `CanonicalTypeIdentity` con interning. | Evita reconstruir strings repetidamente. |
| `sanitize` / hash/name generation | `SpecializationNaming` puro y congelado con goldens. | Los nombres físicos son contrato y no deben divergir entre workers. |

### 2.4 Estado que debe desaparecer de la clase

`currentInstantiation*`, `currentOverloadGroup_`, `activePackSubstitutions_`, errores temporales de
pack/proyección y sets activos pasan al contexto de demanda. `templateStates_` se vuelve catálogo
inmutable; `specializations_` se publica al final. Esto es requisito previo al paralelismo.

## 3. `ProjectMethodBodyModel.cpp` — 27,087 líneas

El archivo no es el cuello observado actual, pero es la mayor deuda estructural y ya participa en
la ruta total. Tiene 269 funciones.

### 3.1 Archivos propuestos

```text
Frontend/Bodies/MethodBodyBuildCoordinator.cpp
Frontend/Bodies/BodyAnalysisContext.{h,cpp}
Frontend/Bodies/BodyScopeAndFlow.cpp
Frontend/Bodies/StatementAnalyzer.cpp
Frontend/Bodies/ExpressionAnalyzer.cpp
Frontend/Bodies/CallAnalyzer.cpp
Frontend/Bodies/AssignmentAnalyzer.cpp
Frontend/Bodies/MemberAccessAnalyzer.cpp
Frontend/Bodies/ConstructionAnalyzer.cpp
Frontend/Bodies/PatternAnalyzer.cpp
Frontend/Bodies/LambdaAnalyzer.cpp
Frontend/Bodies/OwnershipAnalyzer.cpp
Frontend/Bodies/EscapeAnalyzer.cpp
Frontend/Bodies/CleanupAnalyzer.cpp
Frontend/Bodies/BodyDiagnosticBuilder.cpp
```

### 3.2 Métodos gigantes

- `analyzeExpressionCore` — 4,303 líneas, 474 ramas. Sustituir el switch monolítico por un
  `ExpressionWorkMachine` iterativo y handlers por clase. El handler devuelve `Continue`,
  `PushChildren`, `Complete` o `Fail`, más hechos semánticos; no recurre.
- `analyzeCall` — 3,728 líneas, 536 ramas. Separar exactamente: clasificación de receptor,
  recolección de candidatos, mapeo de argumentos, inferencia/conversión, ranking, ownership/escape,
  binding de retorno y diagnóstico. `CallResolutionResult` debe conservar todas las razones de
  descarte para reproducir el diagnóstico actual.
- `analyzeAssignment` — 1,991 líneas. Separar destino/place, operador, conversión, movimiento,
  préstamos, compound assignment y writeback.
- `analyzeVariable` — 1,109 líneas. Separar lookup, captura, estado definite-assignment,
  move/borrow y producción de place/value.
- `analyzeMember` — 1,101 líneas. Consumir índices de miembros; separar receiver classification,
  promoción anónima, accesibilidad, evento/propiedad/campo/método y binding.
- `analyzeReturn` — 792 líneas. Separar validación de contexto, conversión, ownership, async/error
  carrier y structured exit.
- `analyzeStatement` — 712 líneas y recursivo. Máquina iterativa de statement frames; cada scope
  tiene estado de flujo persistente y merge explícito.
- `build` — 561 líneas. Conservar como coordinator: construir índices, enumerar callables,
  despachar tasks, merge determinista y publicar modelos.
- `bindAggregateInitializer` — 482 líneas, recursivo. Worklist de elementos/fields con path de
  inicialización explícito.

### 3.3 Invariantes

- la resolución de overloads produce exactamente la misma candidata y diagnóstico;
- el orden de diagnostics no depende del worker;
- place/value, move-only, borrowed, escape y cleanup no se simplifican;
- el modelo final conserva ordinals de locals, calls, bindings y cleanup regions;
- los workers nunca escriben directamente los vectores globales.

## 4. `ProjectOopHirModel.cpp` — 10,678 líneas

### 4.1 Partición

```text
Frontend/HIR/HirBuildCoordinator.cpp
Frontend/HIR/HirFunctionLowerer.cpp
Frontend/HIR/HirStatementMachine.cpp
Frontend/HIR/HirExpressionMachine.cpp
Frontend/HIR/HirCallLowering.cpp
Frontend/HIR/HirAggregateLowering.cpp
Frontend/HIR/HirAsyncLowering.cpp
Frontend/HIR/HirExceptionAndCleanup.cpp
Frontend/HIR/HirConversion.cpp
Frontend/HIR/HirBindingIndex.cpp
```

### 4.2 Funciones

- `buildImpl` — 1,209 líneas. Dejar sólo precondiciones, construcción de índices de facts y
  scheduler de funciones. La rama `memberFilter` usa el mismo worker callable sin duplicar lógica.
- `lowerStatement` — 1,026 líneas y recursión directa. Convertir a frames con continuaciones de
  loop/switch/try/cleanup.
- `lowerExpressionImpl` — 5,167 líneas, 798 ramas. Dividir por literal/name, member/index, call,
  unary/binary, assignment, construction, aggregate, lambda/delegate, pattern/match, async/result y
  metaprogramming. Dispatcher sin lógica semántica propia.
- `lowerBlock`, `lowerReceiver`, `lowerDefaultArgument`, field initialization y deconstruction:
  handlers separados que comparten `HirLoweringContext` inmutable + `HirFunctionBuilder` privado.

La paralelización existente por miembro se conserva. La extracción debe reducir el tamaño por
translation unit para acelerar también la construcción del propio compilador.

## 5. `ProjectOopMirModel.cpp` — 4,218 líneas

### 5.1 Partición

```text
Frontend/MIR/MirBuildCoordinator.cpp
Frontend/MIR/MirLayoutImporter.cpp
Frontend/MIR/MirFunctionLowerer.cpp
Frontend/MIR/MirNodeMachine.cpp
Frontend/MIR/MirConstantEvaluator.cpp
Frontend/MIR/MirVerifier.cpp
```

### 5.2 Funciones

- `build` — 1,387 líneas. Separar importación de layouts, descubrimiento de tuples, creación de
  funciones y merge. Los lambdas recursivos `collectTypeKey` y `collectNode` pasan a una worklist.
- `lowerNode` — 1,641 líneas, recursivo. Máquina de nodos con stack de resultados.
- evaluadores constantes recursivos: bytecode/evaluador iterativo con límite de pasos explícito.

El MIR resultante debe ser inmutable y serializable para habilitar C4.

## 6. `ProjectOopLlvmLowerer.cpp` — 20,247 líneas

### 6.1 Partición

```text
LLVM/Lowering/LlvmModuleLowerer.cpp
LLVM/Lowering/LlvmLoweringContext.{h,cpp}
LLVM/Lowering/LlvmRuntimeDeclarations.cpp
LLVM/Lowering/LlvmTypeLowering.cpp
LLVM/Lowering/LlvmLayoutLowering.cpp
LLVM/Lowering/LlvmFunctionLowerer.cpp
LLVM/Lowering/LlvmStatementMachine.cpp
LLVM/Lowering/LlvmExpressionMachine.cpp
LLVM/Lowering/LlvmCallEmitter.cpp
LLVM/Lowering/LlvmDispatchEmitter.cpp
LLVM/Lowering/LlvmAggregateEmitter.cpp
LLVM/Lowering/LlvmConstructionEmitter.cpp
LLVM/Lowering/LlvmCleanupEmitter.cpp
LLVM/Lowering/LlvmExceptionEmitter.cpp
LLVM/Lowering/LlvmConversionEmitter.cpp
LLVM/Lowering/LlvmDebugEmitter.cpp
LLVM/Lowering/LlvmMetadataEmitter.cpp
```

### 6.2 Funciones

- `lowerExpression` — 3,877 líneas. Reemplazar por dispatcher de `MirNodeKind` y frames. Los 190
  self-calls desaparecen; subexpresiones empujan frames y reciben valores desde `ValueStack`.
- `lowerStatement` — 698 líneas. Usar la misma máquina con destinos de control explícitos.
- `declareRuntime` — 593 líneas. Extraer tabla declarativa de símbolos con firma/atributos y un
  verificador; no repetir bloques manuales.
- `emitDispatchMetadata` — 373 líneas. Builder aislado, ordenado por identidad canónica.
- `convert` — 334 líneas, recursivo. Grafo de conversiones planificado primero y emitido después;
  worklist para agregados/tuples.
- helpers de tipo/layout: mover a `LlvmTypeLowering`; memo por type key dentro de un módulo.

No se paraleliza dentro de una función LLVM. La granularidad segura es función o partición con
contexto privado, y la publicación del módulo conserva orden.

## 7. `Parser.cpp` — 16,186 líneas

El parser se modulariza después de estabilizar genéricos/LLVM, porque no domina el perfil aportado.

```text
Parser/Parser.cpp                 fachada y entry points
Parser/ParserContext.cpp          cursor, diagnostics, recovery
Parser/DeclarationParser.cpp
Parser/MemberParser.cpp
Parser/TypeParser.cpp
Parser/StatementParser.cpp
Parser/ExpressionParser.cpp
Parser/PatternParser.cpp
Parser/GenericSyntaxParser.cpp
Parser/AttributeParser.cpp
```

- `parseClassMember` (1,464) se divide por kind de miembro después de prefijos comunes.
- `parseGlobalDeclaration` (935) se vuelve dispatcher de declaraciones.
- `parseType` (667, recursivo) usa frames para function/tuple/generic/modifiers.
- `parseUnaryExpression` (564, recursivo) y precedencias migran a Pratt iterativo.
- pattern parsing usa operator/operand stacks y frames de delimitadores.

`Parser` conserva su API y la misma recuperación. Se comparan AST dumps y diagnósticos byte a byte.

## 8. `Driver.cpp` — 8,359 líneas

### 8.1 Partición

```text
Driver/CommandLineParser.cpp
Driver/CompilerInvocation.cpp
Driver/ProjectActionDispatcher.cpp
Driver/ProjectBuildCoordinator.cpp
Driver/PrecompiledModuleLoader.cpp
Driver/PublicSurfaceEmitter.cpp
Driver/ArtifactPublisher.cpp
Driver/BuildReportWriter.cpp
```

- `Driver::run` — 1,811 líneas. Parsear a `CompilerInvocation`, validar combinaciones y despachar.
  La tabla de opciones conserva spelling y texto de error existente.
- `buildPrecompiledPublicSource` — 1,358 líneas. Separar selección de superficie, ordenamiento y
  rendering. Golden de salida antes de moverlo.
- `runProject` — 414 líneas. Convertir la cascada de booleans en tabla `Action -> RequiredStage +
  Renderer` sin cambiar acciones.
- generadores EXE/LIB/DLL — extraer preparación común, generar objetos, runtime y publicación;
  mantener las diferencias explícitas en strategies pequeñas.
- `configurePrecompiledModules` — devolver snapshot y estadísticas, sin modificar globals.

## 9. `ProjectMemberModel.cpp`, `ProjectNameLookup.cpp` y construcción

### `ProjectMemberModel`

- `collectTypeMembers` (1,489): separar fields, properties/events, constructors/destructors,
  methods/operators y promoted anonymous members; buffers por tipo.
- `build` (384): coordinator e índices.
- `canonicalTypeKey` (206, recursión intensa): parser/normalizador iterativo con interning.

### `ProjectNameLookup`

- `bindTypeSyntax` (353, recursivo): resolver iterativamente los componentes del tipo.
- `finalizePredeclarations` (328): dividir resolución de padres, aliases y consistencia.
- `predeclareNode`/`bindNode`: visitors iterativos y scope ids.

### `ProjectConstructionModel`

- `bindConstruction` (444): separar selección, mapeo de argumentos, temporales y ownership.
- `bindConstructorInitializer` (389): reutilizar el mismo motor de selección/mapeo.

## 10. Coordinación y objetos de resultado

Cada modelo grande debe adoptar el patrón:

```text
Inputs (const snapshot)
  -> Analyzer/Lowerer local
  -> Result { records, indexes, rewrites, diagnostics, metrics }
  -> Verifier
  -> Committer
```

Esto reemplaza el patrón actual de “construir la mitad del modelo y dejar flags/campos temporales”.
Un resultado fallido no es observable por etapas posteriores.

## 11. Límites de tamaño y dependencia

Objetivos de mantenimiento, no gates semánticos absolutos:

- `.cpp` nuevo: preferentemente menos de 2,000 líneas;
- método coordinador: menos de 150 líneas;
- handler normal: menos de 250 líneas;
- ningún método nuevo mayor de 500 líneas sin justificación escrita;
- dependencias dirigidas: coordinator conoce servicios; servicios no incluyen coordinator;
- headers públicos no exponen detalles de worklist ni caché;
- `CMakeLists.txt` enumera explícitamente los nuevos translation units.

No se trocea un método sólo para satisfacer números. Si dos handlers comparten estado implícito, se
extrae primero el contexto y se documenta el invariante.

## 12. Orden recomendado de modularización

1. `ProjectGenericModel`: coincide con el cuello y desbloquea índices/cache/parallelism.
2. `ProjectOopObjectGenerator` + telemetría LLVM: pequeño, medible y frontera de caché existente.
3. `ProjectOopLlvmLowerer`: segunda fase dominante.
4. `ProjectOopHirModel` y `ProjectOopMirModel`: habilitan cache por callable.
5. `ProjectMethodBodyModel`: deuda mayor, pero ya tiene paralelismo y no domina el perfil actual.
6. `Driver`: después de estabilizar requests/results para no diseñar abstractions prematuras.
7. `Parser`, name lookup, members y construcción: una familia por tanda, guiadas por perfil.

Cambiar este orden requiere evidencia de perfil que muestre otro cuello dominante.
