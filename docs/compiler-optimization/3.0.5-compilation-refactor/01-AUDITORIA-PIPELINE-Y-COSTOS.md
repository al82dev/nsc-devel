# 01 — Auditoría del pipeline actual y explicación de costos

## 1. Método de auditoría

La auditoría tomó como universo de producción `src`, `include`, `driver` y `lib`: 527 archivos
C/C++ y 203,765 líneas físicas. Para las rutas críticas se combinó lectura manual con un inventario
AST generado desde la base real de compilación de Ninja y libclang 23.1.0. El apéndice A conserva
los resultados por función; este capítulo interpreta los que afectan al tiempo de compilación.

No se usaron los tiempos del baseline que continúa ejecutándose en otra máquina. El único perfil
de pared considerado es el entregado por el usuario: `generics ≈ 122 s`, `LLVM ≈ 53 s`,
`link ≈ 9 s`, `artifact-cache-hits = 0`.

## 2. Mapa del pipeline ejecutado

`Driver::run` interpreta opciones y despacha a una acción de proyecto. Para EXE/LIB/DLL, las rutas
`runProjectOopExecutableGeneration`, `runProjectOopStaticLibraryGeneration` y
`runProjectOopDynamicLibraryGeneration` construyen un `ProjectCompilation`, cargan módulos
precompilados y llaman `compileProjectOopMir`. La dependencia entre métodos fuerza este orden:

1. `compileSyntaxCore`: grafo, lectura, comptime, unidades virtuales precompiladas, lex y parse.
2. `compileProjectGenerics`: catálogo, dependencias, demandas y monomorfización.
3. `compileProjectNames`: resolución de nombres.
4. `compileProjectMembers`: miembros, aliases físicos y funciones de namespace.
5. `compileProjectInheritance`.
6. `compileProjectInterfaces`.
7. `buildGenericConstraintModel`.
8. `compileProjectMethodBodies`.
9. `compileProjectVirtualDispatch`.
10. `compileProjectDestruction`.
11. `compileProjectConstructions`.
12. `compileProjectObjectLayouts` y metaprogramación.
13. `compileProjectOopHir`.
14. `compileProjectOopMir`.
15. `ProjectOopObjectGenerator::generate`: partición, lowerer LLVM, optimización y objetos.
16. Builder/linker de EXE, LIB o DLL.

Esto prueba una propiedad esencial: un hit de objeto en la etapa 15 llega demasiado tarde para
evitar las etapas 1 a 14.

## 3. Qué se reutiliza hoy y qué no

### 3.1 Reutilización existente

- `ProjectLoader` comparte descriptores de proyecto durante una sesión del driver.
- Lectura, comptime, lex y parse de unidades se distribuyen con `CompilationWorkerPool`.
- Names, method bodies, HIR, MIR y LLVM reciben o usan el presupuesto `-j` en rutas ya
  paralelizadas.
- `ProjectGenericModel::specializationByKey_` evita duplicar una misma especialización durante una
  sola construcción.
- `ProjectOopObjectGenerator` restaura objetos por contenido cuando existe un
  `CompilationArtifactCache` habilitado y la configuración es elegible.
- La caché actual también cubre rutas específicas del runtime y del archivo estático final.

### 3.2 Reutilización ausente

- No se persisten tokens ni AST de las plantillas públicas importadas.
- No se persiste el catálogo `TemplateState` ni sus índices.
- No se persiste el cierre de dependencias genéricas.
- No se persisten especializaciones semánticas del frontend.
- No se persiste HIR o MIR por callable.
- El `ProjectGenericModel` es nuevo y de un solo uso en cada compilación.
- El caché de descriptor vive sólo durante el proceso.

### 3.3 Condiciones de la caché de objetos

**HECHO.** La caché se habilita mediante `--artifact-cache=<directorio-absoluto>`; un límite con
`--artifact-cache-max-mb` no es válido sin esa opción. La CLI la restringe a emisiones OOP de
proyecto EXE/LIB/DLL.

**HECHO.** `ProjectOopObjectGenerator::generate` tiene dos caminos:

- antes de construir LLVM, una clave basada en identidad MIR para configuraciones sin información
  de depuración y sin ThinLTO;
- después de producir texto LLVM, una clave de objeto para los demás casos cacheables.

En ambos casos el frontend ya se ejecutó. Un hit pre-LLVM significa “no volver a bajar MIR a
objeto”, no “no compilar el módulo”.

**INFERENCIA.** Los cero hits observados pueden tener causas diferentes. Sin contadores por razón
no es correcto elegir una: caché no configurada; directorio vacío; clave distinta por MIR, target,
política, optimización, depuración o versión; objeto corrupto puesto en cuarentena; o configuración
no elegible. El diseño exige imprimir esta distinción.

## 4. Anatomía de los 122 segundos de `generics`

### 4.1 `ProjectCompilation::compileProjectGenerics` (27 líneas)

Responsabilidad actual:

1. obliga a terminar `compileSyntaxCore`;
2. crea un `ProjectGenericModel` nuevo;
3. concatena `genericSpecializations` de todos los módulos precompilados;
4. llama `ProjectGenericModel::build`.

Problemas:

- no recibe `compilationJobs_`;
- reconstruye el vector importado en cada ejecución;
- no recibe una sesión ni un snapshot cacheable;
- el timer `generics` sólo mide la caja completa, sin subfases.

Acción indicada: conservarlo como fachada pequeña, pero entregarle un `GenericBuildRequest` con
catálogos inmutables, worker budget, política de caché y telemetría; devolver un
`GenericBuildResult` transaccional.

### 4.2 `compileSyntaxCore` (325 líneas)

La función ya paraleliza lectura, comptime, lex y parse. El costo relevante para genéricos aparece
al materializar módulos precompilados:

- `module.sourceText` se registra como API pública precompilada;
- `module.templateSourceText` se registra como una unidad **no precompilada**;
- el comentario del código confirma que esa unidad alimenta la pasada genérica ordinaria del
  consumidor;
- todas las unidades entran después a lex y parse.

Consecuencia: cada invocación independiente vuelve a tokenizar y parsear texto de plantillas cuyo
contenido no cambió. El paralelismo reduce pared, pero no elimina CPU, asignaciones ni memoria.

Acción indicada: introducir un `DecodedTemplateUnitCache` privado, por hash del texto y esquema
interno, que restaure un AST inmutable relocatable o una representación compacta equivalente. No
debe alterar NSMETA ni hacer que el AST cacheado conserve punteros de otro proceso.

### 4.3 `ProjectGenericModel::build` (247 líneas)

El método es un coordinador serial y de estado mutable. Su secuencia incluye:

1. `buildVisibility`.
2. expansión de aliases sobre cada unidad.
3. `mergeTraitImplementations`.
4. `ProjectOpaqueResultModel::build`.
5. recorrido para recolectar retornos de callables ordinarios.
6. `collectAssociatedItems`.
7. `collectTemplates`.
8. clasificación de familias de especialización controlada.
9. `discoverTemplateDependencies` para cada unidad.
10. sort/unique de dependencias, no resueltas y referencias externas de cada plantilla.
11. detección de claves duplicadas.
12. copia de records públicos.
13. siembra de especializaciones precompiladas.
14. `processNode` sobre cada unidad.
15. `materializeBodyTypeSpecializationDemands`.

El problema no es que esas obligaciones sean innecesarias; es que se ejecutan como recorridos
globales separados, muchos vuelven a resolver las mismas identidades y todo sucede antes de poder
reusar un resultado persistente.

Hotspots confirmados dentro de `build`:

- la siembra importada usa `find_if(templateStates_)` por cada especialización importada:
  complejidad `O(P × T)`;
- la asociación de implementaciones de traits busca la plantilla con un `find_if` por impl:
  `O(I × T)`;
- `templateRecords_` duplica cada record, incluidas cadenas y vectores;
- la normalización `sort + unique` se repite por plantilla después de construir todas las aristas.

### 4.4 `collectTemplates` (176 líneas, recursivo)

Recorre el AST, construye `GenericTemplateRecord`, calcula claves canónicas, captura constraints,
dueños, accesibilidad y punteros de sintaxis. Para tipos anidados propaga el dueño por valor.

Riesgos/costos:

- el recorrido recursivo escala con profundidad del AST;
- el record acumula varias cadenas y vectores por plantilla;
- el catálogo queda sólo como vector, sin índices de consulta especializados;
- el ordinal tiene semántica y debe conservar el orden fuente durante cualquier paralelización.

Acción indicada: recolector iterativo por unidad que escriba records en buffers locales. La fusión
estable por `(projectOrdinal, sourceOrdinal, sourceOffset)` asigna exactamente los ordinals.

### 4.5 `discoverTemplateDependencies` y `collectTemplateDependencies`

Ambos son recorridos recursivos. `stateForSyntax` hace un `find_if(templateStates_)` cada vez que
se pregunta si un nodo es raíz de plantilla. Para cada tipo genérico, `collectTypeDependency` llama
`resolveTypeTemplate`, y para cada llamada genérica, `collectCallDependency` ejecuta las mismas
rutas de búsqueda usadas durante materialización.

Complejidad problemática:

- `stateForSyntax`: `O(T)` por nodo consultado;
- `resolveTypeTemplate`: hasta dos barridos del catálogo por referencia;
- dependencia de llamada: `templatesNamedBy`, luego `collectMethodTemplates` —que vuelve a llamar
  `templatesNamedBy`—, y luego `resolveMethodTemplate`, que puede volver a llamar una o dos veces;
- copia completa de `GenericLookupScope` al descender.

Acción indicada: índice `syntax -> TemplateId` y un `GenericResolutionResult` único por sitio que
contenga rung ganador, candidatos, dueño único, fallback y razón de ambigüedad.

### 4.6 `templatesNamedBy` (56 líneas) y `resolveMethodTemplate` (55 líneas)

`templatesNamedBy` itera cada rung de `qualifiedNameLadder`, luego todo `templateStates_`, luego
cada spelling del rung. A esto añade otro barrido para namespace global. Es correcto respecto al
sombreado, pero su forma es multiplicativa.

`processGenericCall` obtiene primero `namedAtWinningRung`, luego llama `collectMethodTemplates` y
finalmente `resolveMethodTemplate`. Una sola llamada puede reconstruir la escalera y barrer el
catálogo entre tres y cinco veces.

Acción indicada:

- índice primario `methodName -> candidate ids`;
- subíndice `(ownerCanonical, methodName, genericArityClass, valueParameterCount)`;
- consulta única que prueba rungs en orden y filtra sólo la cubeta pertinente;
- memo local por `(project, lexical scope id, written owner, name, arity, argc)` durante la fase.

El índice no puede colapsar overloads ni cambiar la regla “primer rung que produce algo”.

### 4.7 `resolveTypeTemplate` (58 líneas)

Barre `templateStates_` para nombres `module::type` o para identidad cualificada visible. El caller
`processTypeSyntax` puede invocarlo dos veces: aridad exacta y aridad sin filtro.

Acción indicada: índices separados por `(module, qualifiedTypeName)` y por nombre visible/aridad,
con un resultado triestado `Missing/Unique/Ambiguous`. Las variantes de pack deben almacenarse en
la cubeta, no provocar un barrido alternativo.

### 4.8 `processNode` (343 líneas, recursión directa)

Es el centro de demanda. Recorre first/second/third/children, tipos, constraints, declaraciones,
scopes, inferencia, llamadas explícitas e implícitas, lambdas y casos contextuales.

Problemas:

- copia `GenericLookupScope` por valor al descender; el scope contiene vectores, múltiples mapas y
  sets;
- clona tipos para expected types, value types y sustituciones;
- mezcla descubrimiento, resolución, mutación del AST y materialización;
- una materialización llama `processNode` sobre el clon y crea una pila semántica implícita;
- el estado `currentInstantiation*` se guarda/restaura manualmente como si fuera call stack.

Acción indicada: dividirlo en dos pasadas. `GenericDemandCollector` recorre iterativamente un AST
inmutable con scopes persistentes/parent-linked y emite demandas. `GenericMaterializer` consume una
cola, produce clones aislados y publica un mapa de reescrituras. Una tercera aplicación estable
modifica las unidades propietarias.

### 4.9 `processTypeSyntax` (151 líneas, recursivo)

Desciende argumentos y funciones, resuelve la plantilla, normaliza argumentos, distingue referencias
abiertas y llama `ensureTypeSpecialization`. Después busca linealmente en reversa
`specializations_` por `specializedName` para recuperar la identidad que acaba de crear.

Acción indicada:

- traversal postorden con `TypeFrame` explícito;
- `ensureTypeSpecialization` devuelve `SpecializationId`/record, no sólo string;
- eliminar la búsqueda reversa;
- memo de `TypeSyntax* -> TypeResolution` por snapshot.

### 4.10 `ensureTypeSpecialization` (163 líneas) y `ensureMethodSpecialization` (168 líneas)

Los dos métodos repiten la misma transacción:

- calculan spellings e identidades;
- forman la clave;
- consultan `specializationByKey_`;
- entran al presupuesto;
- reservan record provisional;
- forman sustituciones y packs;
- salvan múltiples campos globales de contexto;
- clonan el AST;
- revocan la entrada si hay error;
- insertan el clon en el padre;
- ejecutan `processNode` sobre el clon;
- restauran contexto y salen del presupuesto.

Además de duplicación, el patrón impide paralelizar: modifica `specializations_`, el AST del padre,
mapas compartidos, budget y campos `current*`.

Acción indicada: `SpecializationTransaction` común con contexto por valor, un slot reservado por
clave y publicación commit/abort. El worker clona en arena privada y devuelve un resultado; el
coordinador asigna ordinals y adjunta clones en orden canónico. El contexto deja de ser estado
global mutable.

### 4.11 `cloneType` (325 líneas) y `cloneNode` (315 líneas)

Son clonadores profundos recursivos. `cloneType` contiene siete llamadas recursivas directas y
`cloneNode` seis. Copian cada subestructura y aplican sustituciones, packs, proyecciones asociadas,
origen de instanciación, constraints y metadata auxiliar.

Acción indicada en dos etapas:

1. reemplazo mecánico por clonadores iterativos de dos fases (reservar nodo, después poblar hijos)
   con frames explícitos y las mismas reglas;
2. sólo después de equivalencia, reducir clones mediante `SubstitutionView` inmutable y materializar
   un nodo físico únicamente cuando un consumidor exige mutabilidad o serialización.

La segunda etapa tiene mayor rendimiento potencial y mayor riesgo; no debe mezclarse con la primera.

### 4.12 Inferencia genérica

Funciones grandes relevantes:

- `inferGenericCall`: 322 líneas;
- `inferExpressionType`: 178 líneas, ocho llamadas recursivas directas;
- `inferTypeArguments`: 136 líneas, recursiva;
- `applyExpectedTypeToExpression` y `inferLambdaTypeArguments` forman ciclos de colaboración.

Acción indicada: separar recolección de evidencia, unificación, ranking y escritura del resultado.
La unificación de tipos usa una pila de pares `(pattern, actual)` y un mapa de bindings con unión
transaccional; la expresión se evalúa con un stack de frames, no con llamadas anidadas.

### 4.13 `materializeBodyTypeSpecializationDemands` (19 líneas)

Ya usa ondas deterministas, pero ordena toda la onda y procesa cada demanda de forma serial. Las
demandas pueden duplicarse antes de entrar al cache de especialización.

Acción indicada: deduplicar por clave al insertar, ordenar sólo claves únicas, resolver en paralelo
cuando sus slots no colisionan y fusionar resultados por clave. Una nueva demanda va a la siguiente
onda, nunca a la onda en ejecución.

## 5. Auditoría de LLVM: 53 segundos

### 5.1 `ProjectOopObjectGenerator::generate` (314 líneas)

El método construye particiones, calcula claves, prepara resultados por módulo y define un lambda
`processModule`. Cuando hay particiones, crea un `CompilationWorkerPool`, somete una tarea por
módulo, cancela en el primer error y fusiona estadísticas por ordinal. El paralelismo ya existe.

Oportunidades:

- registrar tiempos separados de fingerprint, restore, lowering, verify, optimize, emit y store;
- emitir `cache-miss-reason` por unidad;
- revisar el balance: 16 módulos no implican 16 tareas de costo equivalente;
- evitar recomputar identidad MIR global para cada unidad si la función serializadora recorre más
  datos que los `definitions/declarations` entregados;
- persistir MIR por callable/partición antes de esta frontera.

### 5.2 `ProjectOopCodePartition::build` (213 líneas)

Construye unidades usando costos de nodos y dependencias. `nodeCost` conserva recursión directa.
El diseño 3.0.5 debe reemplazarla por una suma iterativa y almacenar el costo junto al MIR
inmutable. La planificación debe usar pesos históricos además del conteo estático para que un
lowering enorme no quede solo en el último worker.

### 5.3 `ProjectOopLlvmLowerer::lowerExpression` (3,877 líneas)

Es el mayor hotspot estructural del backend: 946 ramas, 2,712 llamadas detectadas y 190 llamadas
recursivas directas. `lowerStatement` añade 698 líneas. Aunque LLVM consuma tiempo en librerías,
esta forma concentra dispatch, conversión, ownership, cleanup, agregados, llamadas y debug en una
sola función difícil de perfilar y paralelizar internamente.

Acción indicada: máquina iterativa de evaluación con `ExpressionFrame`, pila de resultados y
emitters por familia. La división concreta está en el capítulo 03. Cada función LLVM se mantiene
propiedad exclusiva de un worker; no se comparte `LLVMContext` mutable entre tareas.

### 5.4 Caché de objeto

`CompilationArtifactCache::{restoreObject,storeObject,prune}` es pequeño y no recursivo. Sus
operaciones son atómicas/verificadas y no son candidatas primarias a refactor. La mejora necesaria
es observabilidad y una capa anterior; no reemplazar este almacén.

## 6. Monolitos que amplifican el riesgo

| Archivo | Líneas | Evidencia estructural principal |
|---|---:|---|
| `ProjectMethodBodyModel.cpp` | 27,087 | 269 funciones; `analyzeExpressionCore` 4,303 líneas; `analyzeCall` 3,728. |
| `ProjectOopLlvmLowerer.cpp` | 20,247 | 278 funciones; `lowerExpression` 3,877 líneas y recursión intensa. |
| `Parser.cpp` | 16,186 | 177 funciones; `parseClassMember` 1,464; `parseGlobalDeclaration` 935. |
| `ProjectOopHirModel.cpp` | 10,678 | 78 funciones; `lowerExpressionImpl` 5,167; `lowerStatement` 1,026. |
| `Driver.cpp` | 8,359 | `Driver::run` 1,811; `buildPrecompiledPublicSource` 1,358. |
| `WindowsRuntimeBuilder.cpp` | 6,544 | Generación/runtime concentrados; fuera de la primera ruta causal. |
| `ProjectGenericModel.cpp` | 5,812 | 85 funciones; catálogo, inferencia, clonación y materialización mezclados. |
| `LibraryMetadata.cpp` | 5,317 | Decode/encode/compatibilidad juntos; tocar sólo cuando una medición lo justifique. |
| `ProjectOopMirModel.cpp` | 4,218 | `lowerNode` 1,641; `build` 1,387. |
| `ProjectMemberModel.cpp` | 3,914 | `collectTypeMembers` 1,489; `build` 384. |

El tamaño no prueba lentitud por sí mismo. Sí prueba una frontera de cambio insegura: una pequeña
optimización obliga a recompilar, revisar y razonar sobre miles de líneas y estado compartido.

## 7. Recursión presente en la ruta de compilación

Ejemplos confirmados por AST o lectura:

- genéricos: `processNode`, `processTypeSyntax`, `cloneType`, `cloneNode`, `inferExpressionType`,
  `inferTypeArguments`, expansión de aliases y descubrimiento de dependencias;
- parser: tipos, expresiones unarias y patrones;
- miembros/nombres: binding, claves canónicas y recorridos de declaración;
- HIR/MIR: lowering de statements, expresiones y nodos;
- LLVM: lowering de expresiones y conversiones;
- grafo de visibilidad genérica: lambda recursivo `visit`;
- MIR: recolección recursiva de tuples/nodos.

El requisito 3.0.5 es eliminar la recursión de producción en el pipeline caliente. Cada reemplazo
debe preservar preorden/postorden explícitamente y tener una prueba con profundidad superior al
límite razonable de stack del proceso.

## 8. Qué todavía no puede afirmarse

- No se conoce el top de plantillas por tiempo, clones o nodos: falta instrumentación.
- No se conoce si los 53 s de LLVM se concentran en lowering, optimización, emisión o una sola
  partición.
- No se conoce la razón exacta de los cero hits del run observado.
- No se demostró todavía que un cambio sólo en el juego invalide objetos de SaruDX; es una hipótesis
  que la traza de claves debe confirmar o negar.
- No hay medición actual de contención, RSS pico, bytes leídos o asignaciones de arena.

Estas ausencias no bloquean el diseño; definen obligatoriamente la primera tanda.
