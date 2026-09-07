# 130 — Evolución didáctica de NativeSharp hasta 0.9.0

**Fecha de reconstrucción:** 2026-09-07  
**Estado documentado:** `nsc 0.9.0`, baseline G00 certificado  
**Objeto:** explicar cómo evolucionó el compilador, qué problema resolvió cada familia de cambios,
qué modelos y métodos aparecieron, qué archivos quedaron como autoridad y cómo se comprobó cada
salto.

Este documento no es un resumen de los últimos commits. La historia real comenzó antes de que el
árbol actual tuviera una genealogía Git descriptiva. La reconstrucción cruza:

1. las conversaciones históricas de diseño e implementación, desde «00 — Introducción» hasta los
   cierres de Stage 50 y las correcciones posteriores;
2. los documentos didácticos y bitácoras conservados en `mkdown/`;
3. el código, las fixtures y los gates que existen hoy en `A:\nsc-devel`.

Los commits son anclas temporales, no el relato. Un mensaje `fixes` no explica un invariante y seis
commits no pueden reconstruir un compilador que llegó al repositorio tras decenas de etapas.

---

## 1. Tres relojes históricos

NativeSharp ha usado tres numeraciones superpuestas:

- **Stage 00–50:** construcción pedagógica, desde contratos hasta ownership.
- **Stage 51–57 y bloques CC1/CC2:** maduración, biblioteca nativa y correctness.
- **0.7.5 → 0.9.0:** versión de producto, gates reproducibles y bibliotecas precompiladas.

No son proyectos distintos. Son tres maneras de describir el mismo árbol en momentos diferentes.
El campo `stage` del ejecutable conserva identidad pedagógica; `0.9.0` identifica el producto.

También se reutilizaron números. El primer plan llamaba Stage 46 al capstone de Parte 3; la
evolución posterior usó Stage 46 para productos de biblioteca. La autoridad no es el número solo,
sino **nombre de entrega + archivos + pruebas**.

---

## 2. La evolución completa

```text
contratos del lenguaje
        ↓
fuentes, diagnósticos, lexer, parser y AST
        ↓
tipos, símbolos y análisis semántico
        ↓
HIR → MIR → CFG → dominancia → SSA → passes propios
        ↓
LLVM → optimización → COFF → runtime → PE x64
        ↓
proyectos, módulos, namespaces y lookup global
        ↓
clases parciales, miembros, scopes, overloads y construcción
        ↓
layout, herencia, dispatch virtual, interfaces y RTTI
        ↓
structs por valor, arrays, string y Core
        ↓
delegates, closures, genéricos AOT y excepciones nativas
        ↓
RAII, move, owned y borrowed
        ↓
maduración, interoperabilidad y correctness
        ↓
gates reproducibles y dos enlazadores
        ↓
.lib / DLL + NSMETA + consumo NativeSharp entre módulos
        ↓
nsc 0.9.0 certificado
```

Una construcción completa no salta capas. Debe dejar traza en semántica, HIR, MIR, LLVM y el
producto físico cuando su naturaleza lo exige.

---

## 3. Etapas 00–05 — primero el contrato

Las primeras conversaciones no comenzaron escribiendo `Parser.cpp`. Primero separaron decisiones
de lenguaje de decisiones de implementación:

- sintaxis familiar, AOT, binario nativo y control explícito;
- tipos primitivos, literales y conversiones;
- expresiones, statements y precedencia;
- alcance léxico y modelo de nombres;
- memoria, recursos, punteros y frontera nativa;
- ABI Windows x64 y producto COFF/PE.

Eso evitó que el frontend reflejara accidentes del backend. Mucho después se comprobó con los
shifts: no pueden tiparse como operación simétrica sólo porque LLVM recibe dos operands.

Los documentos originales 00–05 viven en el historial de conversaciones y no en el checkout. Su
resultado físico se reconoce hoy en:

- `include/nativesharp/Sema/Type.h` y `TypeContext.h`;
- `include/nativesharp/Sema/ConversionKind.h`;
- `include/nativesharp/Sema/OwnershipKind.h`;
- `include/nativesharp/Target/TargetInfo.h`;
- `include/nativesharp/Target/TargetDataLayout.h`.

No es posible asignar una fecha Git precisa a archivos que entraron en un snapshot previo. Sí puede
reconstruirse su responsabilidad y estado actual.

---

## 4. Etapas 06–20 — el compilador de procedimientos

| Stage | Problema resuelto | Autoridad actual |
|---:|---|---|
| 06 | arquitectura por capas | directorios `include/nativesharp/*` y `src/*` |
| 07 | fuentes, posiciones, rangos y diagnósticos | `SourceManager`, `DiagnosticEngine`, `DiagnosticRenderer` |
| 08 | tokenización determinista | `Lexer`, `Token`, `TokenKind` |
| 09 | parser y AST con arena | `Parser`, `AstArena`, `AstNode`, `TypeSyntax` |
| 10 | símbolos, tipos y semántica | `SemanticAnalyzer`, `SemanticModel`, `TypeContext` |
| 11 | IR de alto nivel | `HirBuilder`, `HirNode`, `HirVerifier` |
| 12 | IR medio no-SSA | `MirBuilder`, `MirInstruction`, `MirVerifier` |
| 13 | grafo de control | `CfgBuilder`, `CfgTraversal`, `CfgVerifier` |
| 14 | dominadores y fronteras | `DominanceBuilder`, `DominatorTree`, `DominanceFrontier` |
| 15 | construcción SSA | `SsaBuilder`, `SsaPromotability`, `SsaVerifier` |
| 16 | passes nativos | `PassManager`, `AnalysisManager`, propagaciones y DCE |
| 17 | lowering LLVM | `LlvmLowerer`, `LlvmModule` |
| 18 | target x64 y COFF | `LlvmTargetMachine`, `CoffObjectVerifier` |
| 19 | runtime, imports y PE | `WindowsRuntimeBuilder`, `WindowsExecutableBuilder`, `LldLinker` |
| 20 | optimización O0–O3 | `LlvmOptimizer`, `LlvmOptimizationLevel` |

### 4.1 La decisión que sostuvo el resto

```text
AST
 ↓
HIR          conserva intención semántica
 ↓
MIR          explicita operaciones y control
 ↓
CFG/SSA      permite análisis y transformación
 ↓
LLVM         backend, no modelo del lenguaje
 ↓
COFF/PE      producto nativo
```

Esto evitó generar LLVM desde el parser. Cada modelo recibió dumper y verificador, por lo que una
regresión puede ubicarse antes de llegar al binario.

### 4.2 Archivos que formaron el esqueleto

**Fuente y frontend**

- `Source/SourceManager.*`, `Diagnostics/DiagnosticEngine.*`;
- `Lexer/Lexer.*`, `Lexer/Token*`;
- `Parser/Parser.*`, `AST/AstNode.*`, `AST/TypeSyntax.*`;
- `Sema/SemanticAnalyzer.*`, `Sema/SemanticModel.*`, `Sema/TypeContext.*`.

**IR y análisis**

- `HIR/HirBuilder.*`, `HIR/HirVerifier.*`;
- `MIR/MirBuilder.*`, `MIR/MirVerifier.*`;
- `CFG/CfgBuilder.*`, `Dominance/DominanceBuilder.*`, `SSA/SsaBuilder.*`;
- `Passes/PassManager.*`, `Passes/AnalysisManager.*` y passes concretos.

**Backend**

- `LLVM/LlvmLowerer.*`, `LlvmOptimizer.*`, `LlvmTargetMachine.*`;
- `Runtime/WindowsRuntimeBuilder.*`;
- `Linker/WindowsExecutableBuilder.*`, `Linker/LldLinker.*`.

### 4.3 Red de pruebas nacida aquí

`LexerTests`, `ParserTests`, `SemanticTests`, `HirTests`, `MirTests`, `CfgTests`,
`DominanceTests`, `SsaTests`, `OptimizationPassesTests`, `LlvmLoweringTests`,
`LlvmCodeGenerationTests`, `WindowsRuntimeTests` y `WindowsLinkerTests`.

Que la última capa funcione no demuestra que una decisión intermedia sea correcta. Esa idea llega
intacta hasta G00.

---

## 5. Etapas 21–30 — del archivo al proyecto y al primer OOP

### 5.1 Stages 21–24 — identidad nominal y compilación multifichero

Stage 21 fijó objetos y accesibilidad. Stage 22 introdujo `.nsproj`, descubrimiento reproducible y
`ProjectSourceSet`. Stage 23 creó el grafo transitivo y una unidad sintáctica por archivo:

```text
proyecto raíz
 ↓
ProjectGraphLoader
 ↓
dependencias primero
 ↓
ProjectCompilation
 ↓
ProjectSyntaxUnit por .ns
```

Stage 24 creó namespaces, aliases y lookup global. El orden cambió de “parsear un archivo completo
y seguir con el siguiente” a:

```text
lex de todas las fuentes
 ↓
recolectar semillas nominales
 ↓
parse de todas las fuentes
 ↓
predeclarar todos los tipos
 ↓
ligar nombres y detectar ambigüedad
```

Archivos consolidados:

- `Compilation/Project.*`, `ProjectLoader.*`, `ProjectSourceSet.*`;
- `Compilation/ProjectGraph.*`, `ProjectGraphLoader.*`, `TypeIdentity.*`;
- `Frontend/ProjectCompilation.*`, `ProjectSyntaxUnit.*`;
- `Frontend/NamespaceSymbol.*`, `ProjectNameLookup.*`, `QualifiedNameLadder.*`.

### 5.2 Stages 25–27 — clases parciales, miembros y cuerpos

Stage 25 añadió `class`, `partial class` y `PartialTypeAggregator`. Stage 26 creó el espacio de
miembros. Stage 27 añadió scopes, locals, shadowing, tipos de expresiones, conversiones y overloads.

Autoridades actuales:

- `ProjectMemberModel::collectTypeMembers`;
- `ProjectMethodBodyModel::build`;
- `ProjectMethodBodyModel::analyzeExpression`;
- `ProjectMethodBodyModel::analyzeCall`;
- `ProjectMethodBodyModel::betterCandidate`;
- `ProjectTypeConversion` y `ProjectConversionClassifier`.

Los overloads no se eligen sumando puntos. La comparación es argumento por argumento; si dos
candidatos ganan posiciones diferentes, la llamada sigue ambigua.

### 5.3 Stage 28 — construcción como modelo separado

Se añadieron declaraciones y overloads de constructores, accesibilidad, constructor implícito,
`new C(args)` y `ObjectConstructionBinding`. La autoridad quedó en
`ProjectConstructionModel`, no en el lowerer.

### 5.4 Stages 29–30 — layout y ruta OOP completa

Stage 29 dio representación física, offsets y allocation. Stage 30 conectó proyecto → OOP HIR →
OOP MIR → LLVM → COFF. Su baseline histórico fue:

```text
nsc 0.2.0-dev30
106/106 CTest
ProjectOopLoweringTests 55/55
DriverTests 166/166
```

Archivos columna vertebral:

- `ProjectObjectLayoutModel.*`;
- `ProjectOopHirModel.*`;
- `ProjectOopMirModel.*`;
- `ProjectOopLlvmLowerer.*`.

---

## 6. Etapas 31–39 — objetos generales, herencia y RTTI

### 6.1 Stage 31 — receiver explícito

Antes, `fieldAddress()` y las llamadas de instancia suponían `thisValue_`. Stage 31 normalizó:

```text
FieldValue(receiver, field)
InstanceCall(receiver, method, arguments)
```

Eso habilitó receivers locales, parámetros, temporales y resultados de llamada. También introdujo
el trap determinista `NullReference = 7`.

### 6.2 Stages 32–34 — herencia y dispatch

Stage 32 creó `ProjectInheritanceModel`, ciclos, lookup heredado, accesibilidad y
`ReferenceUpcast`. Stage 33 fijó base subobject en offset cero, un solo header y chaining
`base(...)`/`this(...)`. Stage 34 añadió slots virtuales estables.

```text
objeto
+0   ClassDescriptor*
+8   campos del ancestro
...  campos derivados
```

El descriptor reservó desde el principio base, vtable, número de slots, identidad estable, mapa de
interfaces y destructor. `base.Method()` es directo; una llamada polimórfica usa
`VirtualInstanceCall` y una llamada indirecta LLVM.

### 6.3 Recovery de Stages 35 y 36

Una auditoría descubrió que los relatos de 35 y 36 existían, pero sus cambios no estaban en el
snapshot físico usado para 37. La genealogía honesta fue:

```text
Stage 34 certificado
 ↓
Stage 37 físicamente implementado
 ↓
Stage 35 Recovery
 ↓
Stage 36 Recovery
```

La corrección se conserva en `Stage35RecoveryTests.cpp`, `Stage36RecoveryTests.cpp` y los ejemplos
`project_stage35_recovery/` y `project_stage36_recovery/`. La lección: un documento no demuestra
que una feature esté en el árbol.

### 6.4 Stage 37 — estáticos, inicialización y constantes

Se incorporaron fields/métodos estáticos, initializers de instancia, inicialización estática,
`const` y `readonly`. El cctor usa estados 0 sin iniciar, 1 iniciando y 2 iniciado.

El evaluator se corrigió para ancho, signo, wrap, división/remainder con signo y shift aritmético.
También se corrigió un bug donde `true` consultaba un flag AST no inicializado.

Métodos centrales: `ProjectMethodBodyModel::isCompileTimeConstantExpression`, análisis readonly y
generación de `__nsc_cctor_<Type>` en el lowerer. `Stage37StaticInitializationTests` llegó a 56/56.

### 6.5 Stage 38 — properties, indexers y operators

La arquitectura reutiliza callables ordinarios:

```text
property  → get_X / set_X
indexer   → get_Item / set_Item
operator  → static callable
conversion definida → op_Implicit / op_Explicit
```

Participaron `Parser.cpp`, `AstNode.*`, `ProjectMemberModel.cpp`,
`ProjectMethodBodyModel.cpp`, `ProjectOopHirModel.cpp` y `ProjectOopMirModel.cpp`.

### 6.6 Stage 39 — identidad dinámica mínima

`is`, `as` y checked casts reutilizan `stableTypeId`, `baseDescriptor` e `interfaceMap`.

- semántica: `TypeTest`, `SafeCast`, `CheckedCast`;
- HIR: `RuntimeTypeTest`, `SafeReferenceCast`, `CheckedReferenceCast`;
- MIR: operación, ordinal y `stableTypeId`.

La suite enfocada histórica cerró 93/93; la global, 144/144.

---

## 7. Etapas 40–45 — valores, callables AOT y excepciones

### 7.1 Stage 40 — struct como valor físico

```text
class  → referencia a objeto con descriptor
struct → bytes inline sin descriptor oculto
```

Asignación y parámetros copian; el receiver de instancia es dirección al storage; structs
anidados agregan inline; recursión directa por valor se rechaza. Los carriers Win64 directos de 1,
2, 4 y 8 bytes se materializan como `i8/i16/i32/i64`.

Pruebas: `Stage40StructValueAbiTests.cpp` y `Stage40_2AggregateAbiExecutionTests.cpp`.

### 7.2 Stage 41 — arrays, string y Core mínimo

Se incorporaron arrays dinámicos, longitud, indexing, bounds, representación nativa de string y las
primeras operaciones Core. La suite enfocada llegó a 72/72 y la global a 152/152.

### 7.3 Stage 42 — delegates y events

```text
delegate cell              event node
+0 code                    +0 delegate cell
+8 receiver                +8 next
```

HIR/MIR hicieron explícitos `DelegateCreate`, `DelegateInvoke`, `EventAdd`, `EventRemove` y
`EventInvoke`. Retirar handler compara `(code, receiver)`.

### 7.4 Stage 43 — lambdas y closures

Una lambda sin captura genera callable estático y receiver nulo. Una capturante genera environment
y usa el receiver del delegate:

```text
MethodLambdaBinding + captures
 ↓
HIR LambdaCreate / ClosureCaptureValue
 ↓
MIR LambdaCreate / ClosureCaptureLoad
 ↓
__nsc_lambda_...
```

La primera política fue snapshot por valor, con slots de ocho bytes y límites explícitos. La suite
enfocada cerró 66/66.

### 7.5 Stage 44 — genéricos AOT

LLVM nunca recibe `T`:

```text
template fuente → ProjectGenericModel → clave/cache → AST concreto
→ lookup/sema/HIR/MIR ordinarios → LLVM concreto
```

La cache registra la especialización antes de procesar el clone; así `Node<int>` recursivo produce
cache hit. Las claves incluyen módulo, namespace, aridad y argumentos.

Métodos: `ensureTypeSpecialization`, `ensureMethodSpecialization`,
`typeSpecializationIdentity`, `resolveDeclaredTypeIdentity` y `appendTypeModifiers`.

### 7.6 Stage 45 — excepciones y cleanup Win64

Se integraron `try`, catches tipados, `finally`, `throw`, rethrow y `using`, con cleanup normal y
unwind. LLVM produce `__C_specific_handler`, `invoke`, `catchswitch`, `catchpad`, `cleanuppad` y
`cleanupret`; matching usa RTTI y el estado actual es thread-local.

La integración descubrió imports ausentes y obligó a sincronizar runtime y linker. Hoy existen
suites separadas de sintaxis, semántica, HIR, MIR, scopes, finally, `using`, RTTI, funclets,
runtime, PE/EH y ejecución de cleanup.

---

## 8. Etapas 46–50 — productos binarios y ownership

### 8.1 Stage 46 — productos de biblioteca

Se añadieron `StaticLibrary`, `SharedLibrary`, `.lib`, DLL, import library y runtime sin startup.

Archivos principales:

- `Linker/CoffArchiveVerifier.*`;
- `Linker/WindowsLibraryBuilder.*`;
- extensiones de `PeExecutableVerifier`;
- `Stage46NativeLibrariesTests.cpp`.

### 8.2 Stage 47 — ABI C estable

`export extern C` separó nombre público y mangling interno. Se añadieron thunks, handles opacos,
structs/enums/pointers compatibles y callbacks. La prueba central es
`Stage47HostCAbiTests.cpp`: 78/78 al nacer; 80/80 después de ampliar la superficie publicable.

### 8.3 Stage 48 — header, metadata y consumidores

Se dividió en seis cierres:

- 48.1 `CHeaderGenerator`, desde el modelo semántico;
- 48.2 `.nsmeta`, writer/reader/verifier, checksum y fingerprint;
- 48.3–48.6 integración vertical usando la misma frontera.

Header y metadata nacen de la misma verdad semántica; no se reconstruyen inspeccionando el PE.

Archivos: `Interop/CHeaderGenerator.*`, `Interop/LibraryMetadata.*` y suites 48.1–48.6.

### 8.4 Stage 49 — RAII y move

Owners léxicos, destrucción inversa, nullificación del source, destroy-before-overwrite, joins de
control y cleanup normal/excepcional:

```text
Alive → Moved
Alive → Destroyed
Moved no vuelve a destruir
```

### 8.5 Stage 50 — `owned` y `borrowed`

Ownership pasó de un local `raii` a firmas, parámetros, retornos, fields y metadata. Las suites se
repartieron en sintaxis, type system, parámetros/retornos, préstamos, joins, escape analysis,
HIR/MIR/LLVM, metadata, ejemplos y ejecución.

`MoveValue`, `BorrowValue`, cleanup y destrucción dejaron de ser convenciones implícitas y pasaron
a la traza IR.

---

## 9. Etapas 51–57 — madurez y correctness

### 9.1 Stage 51 — `String`

`51_nativesharp_complete_string_implementacion_paso_a_paso.md` registra la ampliación real de
`String.ns`, las reglas de ownership medidas contra el compilador y un fix de enums hallado durante
la integración.

### 9.2 Stage 52 — colecciones, `default(T)` y `foreach`

No fue sólo biblioteca. Corrigió:

- `move()` como target de un elemento;
- `default(T)`;
- `move()` de parámetros owned que son valores;
- lowering que asumía pointer para todo `MoveValue`;
- double free por guardar ownership sin transferencia;
- cleanup region mal cerrada;
- `foreach continue` y variable de iteración readonly.

Autoridad didáctica: docs `52_...`, `52_x_...` y `52_x_2_...`.

### 9.3 Stages 53–55 — operaciones de sistema

Tiempo, entorno, archivos, rutas, argumentos, enumeración, threads, atómicos, mutex, monitor,
semaphore/event, proceso, hardware y diagnostics siguieron una ruta común:

```text
declaración Core → MethodBodyModel → HIR → MIR → LLVM → runtime/import Windows
```

Los atómicos pasaron a instrucciones LLVM reales. El guard de inicialización estática se volvió
atómico y después incorporó reentrancia por hilo dueño para evitar el self-deadlock creado por la
primera corrección.

### 9.4 Stage 56 — interop y sistema de tipos

C1–C8 añadió function pointers de ida y vuelta, identidad de imports, borde de excepción de
callbacks, arrays embebidos, unions, packing, aliases FFI, `ref/out/in`, miembros completos de
struct, definite assignment y raíz de tipos sin boxing.

Documentos: `63_...`, `64_...`, `65_...`, `66_...`, `67_...`.

### 9.5 Stage 57 y CC1/CC2 — respuestas correctas antes que más superficie

Se cerraron operador condicional, `do...while`, `switch`, `++/--`, asignaciones compuestas,
`p->`, folding y promociones numéricas, `stackalloc`, `sbyte`, parsing/formato numérico, vistas y
escape checking, I/O binaria, buffers, creación de procesos, arenas, diagnósticos de división por
cero, enums por nombre/ancho y contrato de layout.

Los docs 73–120 también registran errores de método: medir otro `nsc.exe`, ejecutar media matriz,
perder evidencia en `%TEMP%` o mantener tablas duplicadas.

---

## 10. Versiones 0.7.5 → 0.9.0

### 10.1 0.7.5 — observación real de LLVM

`NSC-LIMIT-005` demostró que redirigir la CRT no capturaba con fiabilidad stderr de `LLVM-C.dll`.
Se creó un supervisor padre/hijo:

- `LlvmProcessTrace.*`;
- `LlvmOptimizationObservation.*`;
- `mkdown/124_gate1_nsc_limit_005.md`.

El contrato quedó preciso: último pass observado no significa pass culpable.

### 10.2 0.7.6-alpha — defectos reportados por consumidores

| Límite | Síntoma | Corrección |
|---|---|---|
| LIMIT-011 | constraint pierde identidad cross-project | origen canónico y cache módulo/namespace |
| LIMIT-012 | bitwise del mismo enum rechazado | tipado enum-preserving |
| LIMIT-013 | tipo y field homónimos colisionan | lookup de tipos separado de valores |
| LUNAR-007 | `ulong << int` genera `shl i64, i32` | `normalizeShiftCount` |
| LIMIT-015 | overwrite owned destruye antes del RHS | RHS → conservar → destruir → almacenar |
| LIMIT-019 | `const` calificado/transitivo no pliega | recorrido del grafo nominal |

Métodos tocados: `Parser::canStartTypeAt`, `parseStatement`,
`ProjectMethodBodyModel::analyzeUnary`, `analyzeBinary`, `analyzeAssignment`,
`isCompileTimeConstantExpression`, `ProjectOopHirModel::lowerExpressionImpl` y el lowerer LLVM.

LIMIT-011 tuvo tres causas: clones sin origen, cache por spelling y contexto mutable filtrado entre
candidatos. Se resolvió estampando origen recursivo, identidad canónica y restauración por
especialización.

### 10.3 0.8.0-alpha — compatibilidad fuente con semántica completa

Se incorporaron:

- borrow de temporal owned durante la full expression;
- `raii T[]` con move/unwind;
- `new T[] { ... }` con tamaño inferido y evaluación izquierda-a-derecha;
- análisis perezoso de cuerpos genéricos;
- defaults de métodos/constructores;
- argumentos con nombre: evaluación fuente, colocación ABI;
- clases estáticas, initializer de auto-property, slot `new` y `String.Empty`;
- identidad canónica genérica entre proyectos.

Nodos/campos: HIR `OwnedTemporaryBorrow`, `FullExpressionCleanup`,
`argumentParameterOrdinals`, `defaultValueSyntax`, `selectedDefaultArguments`,
`AstNode::argumentName`, `ClassMemberSymbol::isNewSlot`, `NamedTypeDeclaration::isStatic`.

Métodos: `Parser::parseArgument`, `wrapCallArgumentBorrow`, `wrapFullExpressionCleanup`,
`lowerDefaultArgument`, `bindConstructorInitializer`, `bindConstruction`,
`resolveDeclaredTypeIdentity` y `appendTypeModifiers`.

La regla: aceptar sintaxis no basta. Defaults, nombres y temporales deben conservar significado a
través de ranking, IR, ABI y cleanup.

### 10.4 0.9.0 — consumo NativeSharp entre módulos

0.9 convirtió `.lib` y DLL en módulos consumibles sin recompilar fuentes.

**Grafo precompilado**

- `Project::LibraryReference`;
- `ProjectGraph::addPrecompiledProjects`;
- `ProjectCompilation::setPrecompiledModules`;
- `ProjectSyntaxUnit::isPrecompiled`.

**NSMETA**

- 1.3 defaults;
- 1.4 `Value/ref/out/in`;
- 1.5 firma de delegates `extern C`.

Métodos: `metadataOwnership`, `buildMetadataModel`, `encodePayload`, `decodePayload` y
`verifyCurrentCompiler`.

**Frontend normal, no lookup paralelo**

El driver genera fuente sintética precompilada y la entrega a lookup, miembros, layout, HIR y MIR.
Helpers: `sourceTypeForMetadataKey`, `ownershipPrefix`, `canMaterializeContractType`,
`canMaterializeNativeMethod`, `collectMaterializedContractTypes`, `publishedNativeSymbols`,
`sourceDefaultValue`, `buildPrecompiledPublicSource` y `appendValidatedLibraryReferences`.

**Verificación física**

`CoffArchiveInfo::symbolNames` y `CoffArchiveVerifier::verify` contrastan símbolos. DLL, import
library, metadata, target, pointer ABI, productor, fingerprint y layouts deben coincidir antes del
enlace.

**Linker y librarian separados**

Un fallo con NSLink demostró que `NSC_LLD_LINK` no podía actuar también como archivador. Se
separaron `NSC_LLD_LINK`, `NSC_LLVM_LIB`, `LldLinker::librarianExecutablePath`,
`createStaticLibrary`, `WindowsLibraryBuilder::buildStatic` y `buildDynamic`.

La superficie final incluye métodos/defaults, `ref/out/in`, structs con layout, enums, métodos de
struct, clases opacas con pointer carrier, métodos concretos no virtuales, factory/borrow/move/sink
y callbacks crudos. Lo que no puede materializarse fielmente se conserva y no se degrada.

---

## 11. Registro de problemas corregidos

| Problema | Causa | Autoridad corregida | Prueba |
|---|---|---|---|
| receiver siempre `this` | lowerer anclado a `thisValue_` | HIR/MIR/LLVM OOP | Stage31 |
| base duplicaba header | subobjeto no modelado | object layout | Stage33 |
| override abría slot | falta modelo dispatch | virtual dispatch | Stage34 |
| stages no persistidos | documento tratado como árbol | recovery | Stage35/36 |
| `true` podía bajar falso | flag AST no inicializado | HIR literal | Stage37 |
| evaluator signed incorrecto | carrier unsigned accidental | constant evaluator | Stage37 |
| user operator perdía | orden de candidatos | method body model | Stage38 |
| cast secuestraba `op_Explicit` | clasificación desordenada | conversion classifier | Stage39 |
| struct grande fingía ABI | frontera Win64 incompleta | MIR/LLVM | Stage40 |
| closure truncaba agregado | slot fijo sin guard | closure binding | Stage43 |
| `Node<T>` recursaba | cache registrada tarde | generic model | Stage44 |
| EH pidió imports ausentes | runtime/linker desfasados | runtime imports | Stage45 |
| overwrite owned temprano | RHS/store mal ordenados | LLVM lowerer | LIMIT-015 |
| `MoveValue` siempre pointer | value carrier ignorado | LLVM lowerer | Stage52 |
| `foreach continue` cuelga | incremento saltado | desugaring | Stage52.x |
| cctor tenía carrera | guard no atómico | runtime lowering | Stage54.3 |
| cctor hacía self-deadlock | faltaba owner del init | runtime lowering | Stage54.3 |
| shift LLVM inválido | RHS sin normalizar | LLVM lowerer | LUNAR-007 |
| constraint homónimo colisiona | cache por spelling | generic model | c48 |
| named argument altera efectos | evaluación en orden ABI | binding/HIR | 0.8 |
| temporal muere antes del call | cleanup demasiado temprano | HIR/MIR | c49 |
| linker usado de librarian | una ruta para dos contratos | LldLinker | Stage46 |
| biblioteca falsa pasa por nombre | sin validación física | verifiers | Stage46 |
| fixture exigía herramienta | medía marca, no contrato | CMake fixture | linker-info |
| fixture Stage47 quedó vieja | superficie creció | Stage47 test | 80/80 |
| verde sin hash | corrida no ligada al producto | G00 | gate |

---

## 12. Mapa actual de archivos

Hoy hay **177 headers** bajo `include/nativesharp`, **161 sources C++** bajo `src` y **128 suites**
en `tests/unit`. Muchos entraron como snapshot; este mapa asigna responsabilidad, no fecha falsa.

```text
Source/         texto, IDs, posiciones y rangos
Diagnostics/    IDs, engine y renderer
Lexer/          tokens
AST/            nodos, tipos y arena
Parser/         sintaxis
Sema/           símbolos, tipos, conversiones y ownership base
Target/         target y layout

HIR/            intención semántica
MIR/            operaciones explícitas
CFG/            bloques y aristas
Dominance/      árbol y fronteras
SSA/            promoción
Passes/         folding, propagación, DCE y simplificación
LLVM/           módulo, optimizer, target y lowering OOP

Compilation/    proyecto, loader, grafo, sources e identidad
Frontend/       lookup, miembros, cuerpos, construcción, herencia,
                interfaces, dispatch, layout, destrucción, generics,
                constraints, OOP HIR y OOP MIR

Runtime/        helpers Windows
Linker/         EXE, DLL, LIB, imports y verificación
Interop/        headers y NSMETA
Driver/         orquestación de productos
nsc/            ejecutable
```

Bibliotecas actuales:

- `lib/NativeSharp.Core`;
- `lib/NativeSharp.Core.Collections.Generic`;
- `lib/NativeSharp.Core.Diagnostics`;
- `lib/NativeSharp.Core.IO`;
- `lib/NativeSharp.Core.Threading`.

---

## 13. Los métodos que más evolucionaron

### `Parser`

`parseStatement`, `parsePostfixExpression`, `canStartTypeAt`, `parseArgument`, parsers de members
y desugaring de `foreach`. Pasó de expresiones básicas a clases, generics, properties, lambdas,
excepciones, ownership, argumentos con nombre y arrays inferidos.

### `ProjectMethodBodyModel`

`build`, `analyzeExpression`, `analyzeVariable`, `analyzeCall`, `analyzeUnary`, `analyzeBinary`,
`analyzeAssignment`, `betterCandidate`, `checkArgumentPassKinds` e
`isCompileTimeConstantExpression`. Acumula scopes, typing, overloads, operators, constraints,
constantes, ownership, borrows y operaciones Core.

### `ProjectOopHirModel`

`build`, `lowerExpressionImpl`, `wrapImplicitConversion`, `wrapCallArgumentBorrow`,
`wrapFullExpressionCleanup` y `lowerDefaultArgument`. Su deber es no perder la decisión semántica
antes de MIR.

### `ProjectOopMirModel`

`lowerNode` recibió receivers, virtual calls, values, delegates, closures, excepciones,
move/borrow, defaults y argumentos reordenados. LLVM no debe redescubrir intención fuente.

### `ProjectGenericModel`

`ensureTypeSpecialization`, `ensureMethodSpecialization`, `typeSpecializationIdentity`,
`resolveDeclaredTypeIdentity` y `appendTypeModifiers`: de monomorfización local a identidad
canónica cross-project.

### `ProjectOopLlvmLowerer`

Concentra receiver, object header, vtables, RTTI, carriers de struct, arrays, delegates, closures,
EH, ownership, cleanup, `normalizeShiftCount` y orden de overwrite owned.

### `Driver`

Pasó de seleccionar comandos a orquestar proyecto, dumps, LLVM, objetos, PE, `.lib`, DLL, headers,
metadata, referencias precompiladas, validación física y reporte de linker/librarian.

---

## 14. Evolución del método de trabajo

1. **Stage como capítulo:** especificación, paso a paso, matriz, worklog, conformance, manifest,
   ZIP y handover.
2. **Macroentrega:** menos interrupciones sin perder profundidad interna.
3. **Medir antes de arreglar:** repro mínimo, capa, autoridad, positivos/negativos, O0/O2.
4. **Gate reproducible:** hashes de árbol, producto, herramientas, resultados y evidencia local.
5. **Producto obligatorio:** toda pasada de compilador genera `nsc.exe`, lee identidad y SHA-256.

G00 corrigió una carencia histórica: una corrida verde sin hash no prueba qué compilador se usó.
Una pasada queda `LANDED` sólo si el árbol no se mueve y el `nsc.exe` construido coincide con el
instalado.

---

## 15. Baseline certificado de 0.9.0

`_abi/gates/G00/gate.json` registra:

```text
gate:       G00
run:        20260907-002511-f146ecd7
state:      LANDED
certified:  true
problems:   0

S0: d486932800a0d494fa2fe695ab78be4d4059240b19510e889234a94f16839280
S1: d486932800a0d494fa2fe695ab78be4d4059240b19510e889234a94f16839280

built nsc:
98ac9f65d159da63d7f089be827ed6d4bdc3cbd497530a59c845c7cc4430004b

installed nsc:
98ac9f65d159da63d7f089be827ed6d4bdc3cbd497530a59c845c7cc4430004b
```

Éste es el baseline certificado. Crear este documento mueve el árbol documental después de la
certificación, pero no modifica el compilador ni los bytes de ese ejecutable.

---

## 16. Plantilla obligatoria para futuras evoluciones

Toda entrega que cambie el compilador debe registrar:

1. punto de partida: versión, hash del árbol y de `nsc.exe`;
2. repro mínimo: esperado frente a observado;
3. capa responsable;
4. causa raíz: invariante roto;
5. diseño elegido y autoridad que lo resuelve;
6. símbolos añadidos o modificados;
7. archivos creados, modificados, fixtures y documentos;
8. traza antes/después desde fuente hasta producto;
9. positivos, negativos, O0/O2 y ejecución aplicable;
10. identidad y SHA-256 del nuevo `nsc.exe`;
11. S0/S1, resultado del gate y ubicación de evidencia;
12. límites honestos que todavía no puedan materializarse fielmente.

La lista de archivos no debe ser sólo `diff --stat`: debe decir qué responsabilidad ganó cada
archivo. La lista de métodos debe explicar qué invariante protege cada uno.

---

## 17. Índice del camino conservado

**Biblioteca, runtime y ownership:** docs 51, 52/52.x, 53, 58, 59 y 62.

**Correctness e infraestructura:** docs 68, 73, 74, 79, 82, 85, 89, 93, 94, 97, 104 y 107.

**Versiones:**

- `122_native_sharp_0_7_6_alpha_big_delivery.md`;
- `124_gate1_nsc_limit_005.md`;
- `125_nsc_limit_011_constraints_cross_project.md`;
- `126_native_sharp_0_8_0_alpha_closure.md`;
- `127_native_sharp_0_9_0_alpha_libraries.md`;
- `128_regla_de_generacion_por_pasada.md`;
- `129_cierre_native_sharp_0_9_0.md`.

---

## 18. Lectura final

NativeSharp no evolucionó por acumular keywords. Evolucionó convirtiendo decisiones implícitas en
modelos explícitos:

- `this` implícito se convirtió en receiver;
- una lista de clases se convirtió en grafo nominal;
- un nombre de método se convirtió en candidate set con dominancia;
- un generic escrito igual se convirtió en identidad canónica de módulo;
- un owner se convirtió en estados y operaciones visibles en IR;
- una biblioteca dejó de ser bytes enlazables y se convirtió en código + contrato + verificación;
- una corrida verde dejó de ser una anécdota y se ligó al hash exacto del compilador.

Ésa es la evolución importante hasta 0.9.0: no sólo aceptar formas, sino conservar y explicar su
significado hasta un binario Windows x64 reutilizable.
