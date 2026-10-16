# 02 — Diseño incremental, algoritmos iterativos y paralelismo determinista

## 1. Principios arquitectónicos

1. **La conducta 3.0.4 manda.** Si una optimización cambia diagnóstico, orden, símbolo, ABI, layout,
   ownership, dump o bytes sin una autorización separada, se rechaza.
2. **Evitar es mejor que acelerar.** Primero no ejecutar trabajo cuyo input no cambió; después
   mejorar complejidad; al final paralelizar el trabajo inevitable.
3. **Inmutabilidad para leer, transacciones para publicar.** Los workers leen snapshots y producen
   resultados privados. Sólo un coordinador asigna ordinals y modifica catálogos/AST compartidos.
4. **Una sola fuente de identidad.** Toda clave se construye desde tipos canónicos, huellas y
   configuración, no desde paths temporales ni orden de scheduling.
5. **Cero recursión en rutas calientes.** Cada traversal usa un vector de frames o una cola acotada.
6. **Un presupuesto global de CPU.** `-j` gobierna el pipeline. No se crean pools anidados que
   multipliquen threads.
7. **Telemetría explicable.** Todo hit y miss tiene capa, clave abreviada, unidad y razón.

## 2. Nueva frontera de sesión

### 2.1 `CompilationSession`

**PROPUESTA.** Introducir un dueño explícito de servicios por invocación:

```text
CompilationSession
  JobScheduler
  CancellationToken
  CompilationTelemetry
  ProjectDescriptorCache
  DecodedMetadataCache
  ParsedTemplateCache
  GenericCatalogCache
  SemanticArtifactCache
  MirArtifactCache
  ObjectArtifactCache (adaptador de la caché actual)
```

`ProjectCompilation` recibe una referencia no propietaria a la sesión. En CLI normal la sesión
dura una invocación. En un futuro modo servidor puede durar más, pero ninguna fase depende de ello
para ser correcta.

### 2.2 `JobScheduler`

Debe sustituir la creación de pools independientes por fase. Contrato:

- máximo efectivo derivado una sola vez de `-j` y `--reserve-cores`;
- cola global con clases `Latency`, `Normal`, `Bulk`;
- no ejecutar una tarea que espera activamente a otra tarea del mismo pool;
- `TaskGroup` por fase para esperar, cancelar y contabilizar;
- slots de salida preasignados para resultados deterministas;
- límite de memoria por tareas grandes; LLVM y clonación de AST no deben ocupar todos los cores si
  cada worker excede el presupuesto de memoria;
- sin work stealing que cambie publicación: puede cambiar ejecución, nunca orden de merge.

## 3. Telemetría causal de `generics`

### 3.1 Subfases obligatorias

El timer único `generics` se conserva para compatibilidad, pero contiene:

```text
generics.visibility
generics.alias-expansion
generics.trait-merge
generics.opaque-results
generics.ordinary-return-index
generics.associated-items
generics.template-collection
generics.template-index
generics.dependency-discovery
generics.imported-specialization-seed
generics.root-demand-discovery
generics.specialization-waves
generics.rewrite-commit
generics.body-type-demands
```

### 3.2 Contadores obligatorios

- unidades propias, API precompiladas y unidades de templates importadas;
- bytes de template source, tokens y nodos AST importados;
- templates por clase/struct/interface/delegate/método;
- consultas por índice y fallback;
- candidatos examinados por consulta: total, p50, p95, máximo;
- copias de scope y bytes estimados copiados;
- demandas descubiertas, duplicadas, cacheadas, materializadas y fallidas;
- nodos/tipos clonados y bytes de arena;
- especializaciones por módulo/template y tiempo exclusivo;
- profundidad lógica máxima de worklist;
- tiempo serial, tiempo de worker acumulado, utilización y espera de merge.

La traza detallada sólo se habilita a petición; los contadores agregados son baratos y siempre
disponibles. Un modo `--generic-hotspots=N` imprime los N templates por tiempo, clones y demandas.

## 4. Catálogo genérico indexado

### 4.1 Tipos de identidad

```text
TemplateId          entero estable dentro del snapshot
SpecializationId    entero estable dentro del resultado
ScopeId             nodo inmutable de scope léxico
UnitId              (projectOrdinal, sourceOrdinal, unitKind)
TemplateKey         identidad canónica existente
SpecializationKey   TemplateKey + argumentos canónicos
```

### 4.2 Índices mínimos

```text
byCanonicalKey: TemplateKey -> TemplateId o Ambiguous
bySyntax: AstNode* -> TemplateId
typesByModuleAndQualifiedName: (module, qualifiedName) -> [TemplateId]
typesByVisibleName: terminalName -> [TemplateId]
methodsByName: methodName -> [TemplateId]
methodsByOwnerNameArityCount:
  (canonicalOwner, methodName, arityClass, valueParameterCount) -> [TemplateId]
specializationByKey: SpecializationKey -> SpecializationSlot
specializationBySyntax: AstNode* -> SpecializationId
```

`arityClass` distingue exacta, pack y sin filtro sin duplicar records. Las listas de candidatos se
guardan en orden fuente estable.

### 4.3 Algoritmo de resolución de método

Entrada: proyecto consumidor, `ScopeId`, owner escrito, nombre, aridad y argc.

1. Construir una sola vez `qualifiedNameLadder`.
2. Consultar `methodsByName`; si no existe, `Missing`.
3. Por cada rung, buscar sus owner spellings en el subíndice y concatenar sólo las cubetas
   compatibles.
4. Aplicar visibilidad sin reordenar.
5. El primer rung no vacío gana.
6. Calcular en el mismo resultado si hay dueño único, overload set, ambigüedad o si procede el
   fallback de receiver-value.
7. Memoizar el resultado durante el snapshot.

Salida:

```text
GenericMethodResolution {
  WinningRung rung;
  vector<TemplateId> candidates;
  ResolutionDisposition disposition;
  CanonicalOwner owner;
}
```

`processGenericCall`, dependencia y diagnóstico consumen el mismo objeto. No vuelven a consultar.

### 4.4 Algoritmo de resolución de tipo

1. Resolver `module::qualified` directamente si está escrito.
2. Si no, producir la escalera visible una vez.
3. Consultar cubetas por nombre y aridad.
4. Conservar `Missing/Unique/Ambiguous` y el rung.
5. Sólo si aridad exacta no encuentra, consultar candidatos pack/sin filtro dentro de la misma
   cubeta; nunca volver a barrer el catálogo.

Complejidad esperada: de `O(T)` por consulta a `O(1 + C)` promedio, donde `C` es el pequeño conjunto
de overloads homónimos.

## 5. Scopes persistentes sin copias profundas

`GenericLookupScope` actual se copia al descender. **PROPUESTA:**

```text
ScopeFrame {
  ScopeId parent;
  small_vector<ImportedNamespace> importsAdded;
  small_map<Alias, Target> aliasesAdded;
  small_map<TypeAlias, Declaration> typeAliasesAdded;
  small_set<TypeParameter> typeParametersAdded;
  small_map<ValueName, TypeAndPassKind> valuesAdded;
  optional<MatchContext> match;
  optional<ReturnContext> expectedReturn;
}
```

Cada bloque crea un frame con sólo deltas. Lookup camina parents con un pequeño cache por
`(ScopeId, category, name)`. Una fase opcional aplana scopes muy consultados. Copiar un `ScopeId` es
`O(1)`; la semántica de sombreado se mantiene por búsqueda del hijo al padre.

## 6. Worklist genérica iterativa

### 6.1 Separación de fases

```text
TemplateCatalogBuilder -> GenericTemplateCatalog
DependencyScanner      -> GenericDependencyGraph
RootDemandCollector    -> set<GenericDemand>
SpecializationPlanner  -> deterministic waves
SpecializationWorker   -> private SpecializationProduct
SpecializationCommitter-> GenericBuildResult + rewrite map
RewriteApplier         -> mutated consumer units
```

### 6.2 Estados por clave

```text
Unseen -> Queued -> Building -> Ready
                    |          ^
                    +-> Failed-+
```

Una demanda duplicada encuentra el slot. Si está `Ready`, cuenta hit; si `Queued/Building`, agrega
un borde de dependencia; si `Failed`, reutiliza el error causal. Ningún worker bloquea esperando
otro: las dependencias nuevas entran en la siguiente onda.

### 6.3 Ondas deterministas

1. Deduplicar demandas por `SpecializationKey` al insertarlas.
2. Ordenar la onda por clave estable y source origin.
3. Reservar `SpecializationId` provisional según ese orden.
4. Ejecutar cada demanda en worker privado.
5. Recolectar demandas hijas sin publicarlas.
6. Fusionar productos exitosos por ordinal reservado.
7. Fusionar diagnósticos por `(path, offset, diagnosticId, specializationKey)`.
8. Si no hubo error, publicar hijos como siguiente onda.

Este algoritmo preserva reproducibilidad aun cuando los workers terminan en distinto orden.

### 6.4 Contexto de instanciación

Los campos `currentInstantiationProjectOrdinal_`, namespace, range, overload group, packs y
proyecciones dejan de ser variables del modelo. Se encapsulan en:

```text
InstantiationContext {
  requestOrigin;
  overloadGroup;
  packSubstitutions;
  activeAssociatedProjections;
  budgetPath;
}
```

Cada demanda posee su contexto. No hay salvado/restauración manual ni dependencia del stack C++.

## 7. Traversals iterativos

### 7.1 Frame común de AST

```text
AstFrame {
  node;
  parentOutput;
  nextEdge;
  phase: Enter | Children | Exit;
  scopeId;
  ownerId;
  scratch;
}
```

La función procesa `Enter`, empuja hijos en orden inverso y finaliza en `Exit`. Para postorden se
conserva el resultado de cada hijo en `scratch`. First/second/third y `children` tienen un orden de
arista formal; no se intercambian.

Aplicaciones: alias expansion, template collection, dependency scanning, `processNode`, HIR, MIR,
LLVM, cálculo de costo y verificadores.

### 7.2 Clonado iterativo de tipos

`TypeCloneFrame` reserva primero el nodo destino, registra sustitución/origen/modifier limit y
procesa argumentos, función-return, error y modifiers. Al salir completa los vectores.

Invariantes:

- mismo `SourceRange` que la implementación actual;
- conservación exacta de origin tags;
- packs expandidos en el mismo orden;
- proyección asociada detecta ciclos mediante set de claves activo, no stack de llamadas;
- el límite lógico de profundidad sigue contando frames y conserva el diagnóstico.

### 7.3 Clonado iterativo de nodos

El cloner usa un `ClonePlan` por nodo. En `Enter` decide sustituciones especiales; en `Exit` fija
campos y aristas. Los errores de pack/proyección forman parte del resultado, no de campos globales.

### 7.4 Parser sin recursión

La precedencia de expresiones se implementa con Pratt iterativo o shunting-yard adaptado, usando
pilas de operadores/operandos. Declaraciones, tipos y patterns usan frames con estados. La migración
es tardía porque el parser no aparece en el perfil dominante y su riesgo semántico es alto.

### 7.5 Lowering HIR/MIR/LLVM

Cada lowering se expresa como evaluación de frames:

```text
LowerFrame { node, state, childIndex, expectedType, cleanupRegion, destination }
ValueStack  [lowered child results]
```

Los emitters por clase de nodo son funciones pequeñas que avanzan el estado del frame. Los
constructos con cleanup/exception agregan continuaciones explícitas. Esto evita overflow, facilita
telemetría por kind y permite probar cada emitter sin construir el monolito completo.

## 8. Caché persistente por capas

### 8.1 Reglas generales de clave

Toda entrada contiene:

- magic y versión de **esquema interno de caché**;
- identidad del compilador compatible;
- target/ABI y configuración semántica relevante;
- hash SHA-256 de entradas canónicas;
- dependencias directas con su huella;
- checksum de payload;
- estado completo, nunca parcial.

Publicación: archivo temporal bajo la misma raíz de `A:`, flush, verificación, rename atómico.
Lectura inválida: miss con razón y cuarentena. Ninguna caché convierte un error en hit.

### 8.2 Capa C0 — metadata decodificada

Clave: bytes de metadata + esquema del decoder + target ABI.

Valor: tablas públicas ya validadas e indexadas. Evita decodificar y reindexar cada referencia.

### 8.3 Capa C1 — templates parseados

Clave: SHA-256 de `templateSourceText` + versión de lexer/parser + opciones sintácticas congeladas.

Valor: AST relocatable o serialización compacta sin punteros. No modifica NSMETA; es un artefacto
privado reconstruible.

### 8.4 Capa C2 — catálogo genérico

Clave: huellas C1 de unidades + grafo visible + esquema semántico del catálogo.

Valor: records, índices, dependencias y scopes normalizados. No contiene punteros crudos.

### 8.5 Capa C3 — especialización semántica

Clave:

```text
template semantic fingerprint
+ normalized argument identities
+ transitive public/interface fingerprints
+ ABI/target
+ generic semantic schema
```

Valor: AST especializado relocatable, dependencias hijas, diagnósticos normalizados y mapa de
orígenes. Una especialización sólo es reutilizable si todas sus dependencias coinciden.

### 8.6 Capa C4 — HIR/MIR por callable

Separar huellas:

- `InterfaceFingerprint`: firma, constraints, layout/ABI observable y dependencias públicas;
- `ImplementationFingerprint`: cuerpo normalizado + interfaces de lo que usa.

Una modificación privada cambia implementación, no obliga a invalidar consumidores que sólo ven
la interfaz. Un caller se invalida si el contrato o una decisión de inlining/const-eval que consumió
cambia.

### 8.7 Capa C5 — objetos y artefactos finales

Se conserva `CompilationArtifactCache`. Sus claves pasan a referenciar particiones MIR estables y
la telemetría reporta por unidad. No se debilita la verificación COFF ni la atomicidad existente.

### 8.8 Razones de miss

Enum obligatorio:

```text
Disabled, Cold, EntryMissing, SchemaChanged, CompilerChanged, TargetChanged,
OptimizationChanged, DebugChanged, AbiChanged, SourceChanged, InterfaceChanged,
ImplementationChanged, DependencyChanged, Corrupt, Ineligible, Quarantined
```

La salida agregada muestra conteos; la traza detallada incluye unidad y primera dependencia causal.

## 9. Alcanzabilidad de genéricos

No se debe monomorfizar “todo lo importado”, pero tampoco podar sin prueba.

Raíces conservadoras:

- tipos/métodos genéricos nombrados por fuentes propias;
- firmas públicas y exports del artefacto raíz;
- entradas exigidas por metadata/precompiled specializations;
- runtime hooks, entry point y registration tables;
- demandas derivadas de constraints, associated items, default arguments, comptime y layouts;
- cualquier superficie reflexiva confirmada por el lenguaje.

El algoritmo construye el cierre mediante `GenericDependencyGraph`. Las dependencias no resueltas de
una raíz alcanzable son error como hoy. Una plantilla privada no alcanzable puede conservarlas sin
materialización, igual que el contrato actual documenta.

## 10. Paralelismo por fase

| Fase | Unidad de trabajo | Estado compartido permitido | Merge |
|---|---|---|---|
| Read/comptime/lex/parse | archivo/unidad | caches thread-safe inmutables | ordinal de unidad |
| Catálogo | unidad | sólo índices leídos | orden fuente |
| Dependencias | template | catálogo inmutable | `TemplateId` |
| Demandas raíz | unidad/callable | catálogo + scopes inmutables | clave estable |
| Especialización | `SpecializationKey` | sólo lectura | onda/ordinal reservado |
| Bodies | callable | modelos inmutables | member ordinal |
| HIR/MIR | callable | modelos inmutables | function ordinal |
| LLVM | partición pesada | target config inmutable; contexto LLVM privado | partition ordinal |
| Cache I/O | entrada | filesystem store sincronizado por key | publicación atómica |

### Evitar oversubscription

- un solo scheduler;
- LLVM puede adquirir tokens de memoria además de CPU;
- si una fase tiene menos unidades que workers, deja capacidad ociosa disponible para I/O/caché,
  no crea subtareas anidadas;
- tareas demasiado pequeñas se agrupan por costo;
- tareas enormes se parten por callable/dependency cut, nunca a mitad de una estructura mutable.

## 11. Determinismo

Cada tanda paralela debe demostrar:

- mismos diagnósticos y orden en 20 ejecuciones con scheduling perturbado;
- mismos dumps semánticos, HIR, MIR y LLVM normalizados;
- mismos nombres especializados y ordinals;
- mismos SHA-256 de OBJ/LIB/EXE cuando el formato lo permite;
- igualdad entre `-j1`, `-j2` y `-jN`;
- un error en un worker no publica resultados parciales de otros.

## 12. Modo servidor: fase posterior, no atajo

Un daemon puede conservar metadata, catálogos y arenas entre compilaciones, pero sólo después de
que las capas persistentes tengan claves correctas. De otro modo escondería invalidaciones y haría
el rendimiento depender del estado invisible de un proceso.

La interfaz futura sería un `CompilationService` que recibe un request autocontenido y usa los
mismos snapshots/cachés que CLI. La CLI sin daemon sigue siendo referencia funcional y de
reproducibilidad.
