# NSC — Análisis de rendimiento del compilador, función por función

**Fecha:** 2026-10-20
**Base analizada:** `A:\nsc-devel` (rama `development`, con el refactor modular aplicado), copia completa preparada el 2026-10-19: 1720 archivos y 519 `.cpp` del compilador.
**Alcance:** solo el código del compilador `nsc`: driver de build, lectura de NSMETA, frontend, HIR, MIR, LLVM, enlazador y verificadores que corren en cada `nsc build`. No toca la infraestructura (scripts, IDE, LSP, CI, instalador). Las pruebas solo se consideran como restricción: nada de lo propuesto puede romper lo que usan.
**Destinatario:** el agente que implementa. Cada hallazgo indica archivo, líneas, función, qué pasa, por qué cuesta y el cambio exacto, con la condición de equivalencia funcional.

---

## 0. Cómo leer este documento

### 0.1 Etiquetas de certeza

- **HECHO**: lo leí en el código fuente de esta copia, con número de línea. Las líneas son las de la copia preparada; si el archivo cambió después, buscar la función por su nombre.
- **INFERENCIA**: conclusión razonada a partir de hechos, sin medir. Por ejemplo: "esto domina el tiempo", o "MSVC asigna memoria en el constructor por defecto de `unordered_map`".
- **NO VERIFICADO**: no se pudo comprobar con lo disponible. Por ejemplo: número real de miembros de Core, o qué hacen las pruebas que no estaban en la copia.

### 0.2 Método

1. **Lectura manual** de toda la ruta caliente de `nsc build`, siguiendo el flujo real:
   - restore, proyecto generado, `configurePrecompiledModules`;
   - fases de `ProjectCompilation`;
   - HIR, MIR, LLVM;
   - `WindowsExecutableBuilder`.
2. **Extracción sistemática con libclang 18** sobre los 519 `.cpp` (8515 definiciones de función). De cada función se registró:
   - bucles y profundidad de anidamiento;
   - cada llamada con la profundidad de bucle en que ocurre;
   - cada invocación de constructor o asignación **de copia** (por AST, no por texto);
   - parámetros por valor de tipos registro de más de 16 bytes;
   - variables de `for` por rango declaradas por valor.
3. **Barrido estructural** de todos los bucles que recorren una colección **de todo el proyecto**:
   - `namedTypes()`, `members()`, `locals()`, `cleanupRegions()`, `structLayouts`, `functions()`, `dispatchTables`, etc.;
   - 603 bucles, cada uno asignado a su función contenedora;
   - grafo de llamadas para saber si esa función se ejecuta una vez por build o **una vez por consulta** (por nodo, por expresión o por miembro).
4. **Clasificación a mano** de cada caso por consulta frente a pasada única. Las pasadas únicas (`O(N)` una vez) se dejan fuera; solo se reportan si hacen algo caro por elemento.

### 0.3 Lo que NO pude leer

- `A:\LINEAMIENTOS-DESARROLLO-COMPILADOR-NATIVESHARP.md` está en la raíz de `A:\`. Esa carpeta no está conectada a la sesión: solo lo están `A:\nsc-devel`, `A:\NSC-IDE-Avalonia` y `A:\nativesharp-exec-mcp`. Seguí los lineamientos que ya conozco por las guías anteriores:
  - una clase por archivo;
  - Allman;
  - líneas de 120 columnas;
  - funciones de 60 líneas ejecutables como máximo;
  - sin recursión nueva;
  - sin lambdas "cool";
  - nunca borrar comentarios;
  - comentarios bilingües;
  - documentación en español.
- De `tests/` solo se copiaron 10 archivos. Lo que la guía §1.3.4 dice sobre miembros privados usados por pruebas se respeta, pero **cada cambio que toca un miembro existente** lleva la nota "grep en `tests/` antes de aplicar".

### 0.4 Restricciones de implementación (aplican a TODO)

1. **Equivalencia funcional.** Cada cambio produce exactamente los mismos resultados: mismo orden de funciones, mismos diagnósticos, mismo IR y mismos símbolos. Cuando un índice sustituye una búsqueda lineal, conserva la regla "gana el primero en orden de ordinal".
2. **Miembros congelados por las pruebas.** No se renombran ni se cambia el tipo de los miembros privados que usan las pruebas. Ejemplos conocidos:
   - `Parser::knownTypeNames_`;
   - `ProjectOopMirModel::structLayouts_` y `functions_`;
   - campos de HIR.

   Se **añaden** miembros nuevos; los existentes se dejan.
3. **Firmas públicas.** Solo se cambian cuando el cambio es compatible en código fuente con todos los llamadores. Ejemplo: devolver `const std::string&` donde antes se devolvía `std::string`. Al final del R-01 está la lista de comprobaciones de lifetime.
4. **Estilo.** El código nuevo cumple:
   - Allman, 120 columnas, ≤60 líneas ejecutables por función;
   - comentarios bilingües (ES/EN);
   - sin lambdas nuevas en código de producción (los comparadores ya existentes se quedan);
   - sin recursión nueva.
5. **Comentarios.** No se borra ningún comentario existente. Si un comentario describe un bucle que se sustituye, se conserva y se añade debajo `// PERF (NSC-PERF-xx): ...`.

### 0.5 Correcciones del 2026-10-20 (tras leer los lineamientos y las pruebas completas)

Prevalecen sobre el texto de abajo. El orden de implementación, los gates y la autorización de cada cambio están en `NSC-TRACKING-RENDIMIENTO.md`, en la misma carpeta.

1. **`A:\LINEAMIENTOS-DESARROLLO-COMPILADOR-NATIVESHARP.md` ya está leído completo.** §12 protege el "comportamiento observable del programa generado", y §9.2 prohíbe eliminaciones por anticipación. Por eso:
   - **R-09 deja de ser "lote 1".** R-09.3 cambia el `.exe` generado. R-09.1 y R-09.2 retiran instrumentación. Las tres requieren autorización explícita (Grupo B en el tracking).
   - **R-08.1, R-08.2, R-10.1 y R-15.3 también son Grupo B**, porque añaden formatos persistentes o cambian la política de caché. Lo mismo pasa con R-07.3.2, R-07.3.3, R-07.5.2 y la memoización de R-08.3, porque retiran verificaciones existentes.
2. **"Sin riesgo semántico" estaba mal dicho.** R-01 y R-07.1 tienen gate propio y prueba diferencial nueva.
3. **Requisito descubierto en las pruebas.** `tests/unit/ProjectOopLoweringTests.cpp` arma `ProjectNameLookup`, `ProjectObjectLayoutModel` y `ProjectMemberModel` a mano, con `built_ = true` y sin `build()`, y vacía y rellena sus vectores. Todo índice nuevo debe:
   - construirse solo en `build()`;
   - llevar una bandera propia;
   - caer al recorrido lineal original si no se construyó.

   Prohibido el índice perezoso.
4. **R-08.2: NSLink no enlaza ninguna DLL dummy.** Con `/def` y sin objetos de entrada, el producto ya es `ImportLibrary` antes de mirar `/dll` (`A:\nslink-devel\src\driver\CompatibleDriver.cpp:782-795`, `:900-923`). La opción `/lib /def:` no aporta nada. El costo real es un proceso por DLL, y solo se elimina con caché (Grupo B) o con un escritor en proceso (diseño).
5. **R-06.4 queda en NO IMPLEMENTAR.** Hay mutadores en sitio de `TypeSyntax` (`TypeAliasExpander.cpp:196`, `ConstGenericArguments.cpp:177`/`:194`, `setOrigin*`) que impiden demostrar que no clonar un tipo hoja sea equivalente.
6. **R-06.8: solo entra la precomputación de la ruta canónica.** Quitar la recursión de `std::function` es limpieza de estilo, y §2 prohíbe mezclarla con la optimización.
7. **Verificado:**
   - `scanParallelOrdinals` y `applyParallelOrdinalOffsets` aceptan una función sin cuerpo (`HirBuildCoordinator.cpp:266-272`, `:300-306`);
   - todo `ObjectFieldLayout::memberOrdinal` cae dentro de `members()` (`ProjectObjectLayoutModel.cpp:867-870`, `:1168-1171`, `:1250`);
   - `nsc build` no reenvía `--time`; para medir se usa `--progress` (`ProjectCommands.cpp:1280-1301`, `DriverPhaseTimer.cpp:9`).
8. **El snippet de la §6 queda sustituido por el tracking.** Cada entrada `T-xx` es un commit. La suite la corre Alberto desde Builder; el implementador entrega la entrada para validación.

---

## 1. Resumen ejecutivo

### 1.1 Por qué sigue lento (INFERENCIA, sostenida por los HECHOS de abajo)

LLVM no es el problema. En la medición anterior de hola mundo, la generación de objetos LLVM tardó unos 19 ms. El tiempo está en tres familias de costo que **escalan con el tamaño de Core** y no con el programa del usuario. Por eso un hola mundo tarda casi lo mismo que un programa mediano.

1. **Búsquedas lineales por consulta que además construyen strings.**
   - `TypeIdentity::key()` y `fullyQualifiedName()` construyen un `std::string` nuevo en **cada** llamada (240 sitios).
   - Unas 60 funciones de consulta, en Bodies, HIR, MIR, LLVM y modelos, recorren **todos** los tipos del proyecto llamando `key()` por candidato, una vez por expresión, campo o tipo mapeado.
   - Con Core dentro del proyecto, cada una de esas consultas cuesta `O(T)` asignaciones de memoria.
2. **Trabajo cuadrático en HIR y MIR.**
   - HIR lanza un *worker* por **cada** miembro del proyecto, incluidos los miembros precompilados de Core, que solo producen una declaración de importación.
   - Cada worker recorre y redimensiona estructuras de tamaño `M` (miembros) y recorre todos los `locals()` y `cleanupRegions()` del proyecto.
   - MIR copia **todas** las layouts de struct por cada función y busca el layout de un campo recorriendo todos los campos de todos los tipos.
3. **Costos fijos por build que no dependen del programa**, todos con línea exacta abajo:
   - re-hash de todas las fuentes y de `Core.lib`, `.nsmeta` y `.dll` (procedencia), siempre;
   - lectura y verificación del mismo `.nsmeta` dos veces en el mismo proceso;
   - regeneración del texto de la superficie pública de Core;
   - ocho recorridos del AST de Core en genéricos;
   - construcción y optimización del módulo runtime sin caché;
   - cuatro o más procesos NSLink para las bibliotecas de importación;
   - lectura byte a byte de archivos grandes (SHA-256 y PE);
   - trazas a disco siempre activas.

**Condición previa a cualquier medición (HECHO de la sesión anterior).** El ejecutable del compilador se está construyendo con `/Od`. No es código del compilador, pero **multiplica** todo lo anterior: sin optimizar, cada `std::string` temporal, cada `std::function` y cada iterador cuesta varias veces más. Las mediciones de antes y después de este plan deben hacerse con el host en `/O2` (o `RelWithDebInfo`). Si no, se compara ruido de depuración.

### 1.2 Ranking de causas

La columna "magnitud" es INFERENCIA y está ordenada por impacto esperado en hola mundo con Core.

| Rango | ID | Causa | Dónde (resumen) | Magnitud esperada |
|---|---|---|---|---|
| 1 | R-01 | `TypeIdentity::key()` y `fullyQualifiedName()` sin caché | `Compilation/TypeIdentity.cpp:126-158` (240 llamadores) | Muy alta: multiplica el costo de todas las demás búsquedas |
| 2 | R-02 | ~60 consultas `O(T)` sobre `namedTypes()` con `key()` por candidato | Bodies, HIR, MIR, LLVM, modelos (tabla completa en §3.2) | Muy alta |
| 3 | R-03 | HIR: un worker por miembro, `resize(M)` por worker, escaneo de `locals()` y `cleanupRegions()` por función | `HIR/HirBuildCoordinator.cpp:736-779, 1015-1036, 1538-1548, 1590-1595, 1666-1671, 1796-1802, 2068-2070` | Muy alta (cuadrática) |
| 4 | R-04 | MIR: copia de `structLayouts_` por función, `fieldLayoutForMember` lineal en todos los campos y ABI de cada parámetro buscando en todas las layouts | `MIR/MirFunctionLowerer.cpp:256-282, 844`, `ProjectObjectLayoutModel.cpp:161-181` | Alta (cuadrática) |
| 5 | R-05 | LLVM: `llvmType` repite el chequeo de puntero y hace 3 o 4 escaneos completos de tipos por mapeo, sin memo | `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:11-19`, `Support/LlvmTypeMapping.cpp:89-175`, `LlvmTypeLayout.cpp`, `LlvmTypeClassification.cpp` | Alta |
| 6 | R-06 | Genéricos: `resolveDeclaredTypeIdentity` lineal, `Cow` con 6 `make_shared` por nodo, copias de `children()`, 8 recorridos de AST, `cloneAliasType` de cada firma | `Frontend/Generics/*`, `ProjectGenericModel.h:384-395` | Alta |
| 7 | R-07 | Driver: procedencia siempre, doble hash en restore, doble carga de proyecto, NSMETA leído y verificado dos veces, `sha256File` byte a byte | `Driver/ProjectCommands.cpp`, `Basic/Sha256.cpp`, `Driver/LibraryReferenceValidation.cpp` | Alta (E/S fija) |
| 8 | R-08 | Enlace: runtime construido y optimizado en cada build, más de 4 NSLink para import libs, PE leído byte a byte (3 veces) | `Linker/WindowsExecutableBuilder.cpp:590-601`, `WindowsRuntimeImports.cpp:276-324`, `PeExecutableVerifier.cpp:116-123` | Alta (procesos) |
| 9 | R-09 | Trazas e instrumentación siempre activas | `LLVM/Internal/ColdLlvmTrace.cpp:22-23`, `OopLlvmImplementation.cpp:166-189`, `OopLlvmRuntimeDebugTrace.cpp:31-39` | Media |
| 10 | R-10 | Superficie pública de Core regenerada cada build con `O(M×T)` asignaciones; `ContractMaterialization` sin memo; DFS exponencial al verificar plantillas | `Driver/PublicSurfaceBuilder.cpp`, `PrecompiledSurface/ContractMaterialization.cpp`, `Interop/LibraryMetadataVerification.cpp:839-907` | Media/Alta |
| 11 | R-11 | Lexer y parser: 3 tablas de palabras clave, copia de `knownTypeNames` por unidad, `std::string` por consulta | `Lexer/Lexer.cpp:181-236`, `Parser/ParserContext.cpp:19-60`, `Parser/TypeParser.cpp:57` | Media |
| 12 | R-12 | AST: ~450 B y una asignación de heap por nodo (no es arena real) | `AST/AstNode.h:222-243`, `AST/AstArena.h:23-28` | Media (global) |
| 13 | R-13 | Bodies: escaneos de `members()` completos por consulta (`callTypeIsSend/Sync` recursivos, `memberCandidatesForType`, etc.) | `Frontend/Bodies/*` (§3.3) | Media |
| 14 | R-14 | Enlace con precompilados: `bindPrecompiledPhysicalNames` con canonicalización `O(M×A)`; `bindPrecompiledEscapeSummaries` `O(F×I)` | `MemberModel/BuildSupport.cpp:316-390`, `ProjectNamespaceFunctionModel.cpp:1593-1625` | Media |
| 15 | R-15 | Varios: checksum de debug info por unidad y archivo, `interfaceSlotForMember` lineal, carga de grafo de proyecto con `weakly_canonical` en comparadores, poda de caché en cada invocación, destrucción completa de `ProjectCompilation` | §3.15 | Baja/Media |

### 1.3 Orden de implementación recomendado (lotes)

Cada lote es coherente y se compila y prueba por separado. El orden busca el máximo impacto con el mínimo riesgo.

- **Lote 1 (impacto inmediato; ver correcciones §0.5):**
  - R-01 (caché en `TypeIdentity`);
  - R-09 (apagar trazas);
  - R-05.1 (quitar el chequeo de puntero duplicado en `llvmType`);
  - `Sha256` y lectura PE por bloques (R-07.1, R-08.3).
- **Lote 2 (índices):**
  - R-02 (índice `typeOrdinalsForIdentityKey` en `ProjectNameLookup` y sustitución de los ~60 escaneos);
  - R-04.2 (índice de campo por miembro);
  - R-04.3 y R-04.4 (índice de layout por typeKey en MIR, usado también por la ABI).
- **Lote 3 (HIR y MIR):**
  - R-03 completo;
  - R-04.1.
- **Lote 4 (driver y enlace):**
  - R-07 (procedencia condicional, NSMETA una sola vez, restore sin doble hash);
  - R-08 (caché de `runtime.obj` y de las import libs).
- **Lote 5 (genéricos y parser):**
  - R-06;
  - R-11.
- **Lote 6 (superficie y verificación de metadatos):**
  - R-10;
  - R-14.
- **Lote 7 (estructural, opcional):**
  - R-12 (arena real y nodo AST compacto);
  - partir la superficie de Core en varias unidades para paralelizar lex y parse.

Después de cada lote, medir como indica §4 y anotar en el documento de seguimiento.


---

## 2. Hallazgos transversales (afectan a todas las fases)

### R-01 · `TypeIdentity::key()` y `fullyQualifiedName()` construyen un string nuevo en cada llamada · P0

**HECHO.**

- `include/nativesharp/Compilation/TypeIdentity.h:106-107` declara `std::string fullyQualifiedName() const;` y `std::string key() const;`, ambas por valor.
- `src/Compilation/TypeIdentity.cpp:126-153` (`fullyQualifiedName`): construye `result` concatenando todos los segmentos de namespace, los tipos contenedores y el nombre con aridad (`appendTypeSegment`). Hay varias realocaciones por llamada.
- `src/Compilation/TypeIdentity.cpp:155-158` (`key`): `return moduleName_ + "::" + fullyQualifiedName();`. Es la construcción anterior más dos concatenaciones.
- `TypeIdentity` es **inmutable** después del constructor:
  - miembros privados en `TypeIdentity.h:116-121`;
  - sin setters;
  - el constructor (`TypeIdentity.cpp:76-89`) solo mueve los argumentos y llama `validate()`.
- En `src/` hay 151 llamadas a `.key()` sobre identidades y unas 240 entre `key()` y `fullyQualifiedName()`. Los puntos que más pesan:
  - **Comparadores de ordenación:**
    - `NameLookup/TypeAggregation.cpp:83-86` (`pendingTypeLess`) construye hasta 4 strings por comparación al ordenar **todos** los tipos pendientes, incluidos los de Core;
    - `Compilation/PartialTypeAggregator.cpp:57`.
  - **Bucles de búsqueda que llaman `key()` por candidato:** 73 sitios, ver R-02.
  - **Cascada de intrínsecos de `CallAnalyzer`:** `fullyQualifiedName()` se reconstruye en cada `recognizeCore*Operations` para **cada llamada analizada**:
    - `CallAnalyzer.cpp:987` (String), `:1207` (Half), `:1237`, `:1255`, `:1276` (BitOperations);
    - `:1371`, `:1398` (Console), `:1420` (RuntimeHelpers), `:1459` (NativeClock), `:1507` (Environment);
    - `:1604-1609` (File y Directory), `:1793` (Path), `:1836` (Thread), `:2275` (AtomicFence);
    - `:2343` (Mutex), `:2410` (Monitor), `:2514` (Semaphore), `:2584` (Event), `:2661` (Process);
    - `:2735` (SystemInfo), `:2781` (Debug).
  - **Bucles con `fullyQualifiedName()` por elemento:**
    - `HIR/HirBuildCoordinator.cpp:663` (mangling: `symbolFragmentForIdentityKey(type.identity.key())` por cada función);
    - `MIR/MirBuildCoordinator.cpp:68` y `:264`.

**Por qué cuesta (INFERENCIA).** Toda consulta que compara una identidad paga una construcción de string con varias asignaciones, aunque solo vaya a comparar. Como R-02 hace esto `O(T)` veces por consulta, el costo real de cada consulta es `O(T × asignaciones)`. Con el host en `/Od` es todavía peor.

**Cambio exacto.**

1. `include/nativesharp/Compilation/TypeIdentity.h`:
   - Líneas 106-107. Antes:
     ```cpp
         std::string fullyQualifiedName() const;
         std::string key() const;
     ```
     Después:
     ```cpp
         // PERF (NSC-PERF-01): ES: la identidad es inmutable; el nombre completo y la clave se calculan una
         // vez en el constructor y se devuelven por referencia. EN: the identity is immutable; the fully
         // qualified name and the key are computed once in the constructor and returned by reference.
         const std::string& fullyQualifiedName() const noexcept;
         const std::string& key() const noexcept;
     ```
   - En la sección `private:`, después de `void validate();` (línea 114):
     ```cpp
         std::string buildFullyQualifiedName() const;
     ```
   - Después de `std::string validationError_;` (línea 121):
     ```cpp
         std::string fullyQualifiedName_;
         std::string key_;
     ```
2. `src/Compilation/TypeIdentity.cpp`:
   - Constructor (líneas 76-89): después de `validate();` añadir:
     ```cpp
         fullyQualifiedName_ = buildFullyQualifiedName();
         key_ = moduleName_ + "::" + fullyQualifiedName_;
     ```
   - Línea 126: renombrar `std::string TypeIdentity::fullyQualifiedName() const` a `std::string TypeIdentity::buildFullyQualifiedName() const`. El cuerpo (126-153) queda **idéntico**.
   - Añadir estas dos funciones nuevas:
     ```cpp
     const std::string& TypeIdentity::fullyQualifiedName() const noexcept
     {
         return fullyQualifiedName_;
     }
     ```
     ```cpp
     const std::string& TypeIdentity::key() const noexcept
     {
         return key_;
     }
     ```
     La segunda sustituye a la versión actual de las líneas 155-158.
   - `operator==` (línea 160 en adelante) **no cambia**: sigue comparando los campos, no `key_`.

**Equivalencia.** Las cadenas son las mismas, porque se calculan con el mismo cuerpo y en el mismo momento lógico. `validate()` no depende de `key()`: lo verifiqué en `TypeIdentity.cpp:208+`, que solo valida identificadores. Las copias y movimientos por defecto copian también los dos campos cacheados.

**Comprobaciones de lifetime (HECHO, ya hechas por mí).**

- No hay ninguna llamada `TypeIdentity(...).key()` ni `.fullyQualifiedName()` sobre un **temporal** ligado a una referencia o a un `string_view`. Las dos ligaduras `const std::string& x = ...` que existen están sobre lvalues estables (`MirBuildCoordinator.cpp:68` y `:264`).
- No hay punteros a miembro `&TypeIdentity::key` ni `&TypeIdentity::fullyQualifiedName`. La única mención es un comentario en `TemplateSpelling.cpp:56`.
- `auto k = identity.key();` sigue funcionando: se copia, igual que antes.
- **Pendiente para el implementador:** hacer `grep -rn "\.key()\|fullyQualifiedName()" tests/`. Si alguna prueba hace `std::string&& x = id.key();`, adaptarla: es el único patrón que deja de compilar.

**Costo de memoria.** Dos strings más por identidad. Hay una identidad por tipo nombrado, del orden de miles con Core, así que es despreciable.

---

### R-02 · ~60 consultas recorren todos los tipos del proyecto comparando `key()` · P0

**HECHO.** No existe en `ProjectNameLookup` un índice de "clave de identidad → ordinal":

- `ProjectNameLookup.h:510-522` solo tiene:
  - `typesByModule_`, por FQN y por módulo;
  - `typeOrdinalBySyntax_`, por nodo;
  - `bindingBySyntax_`.
- El índice correcto existe, pero **escondido** en `ProjectMemberModel`:
  - declarado en `ProjectMemberModel.h:524-528` (`typeOrdinalByKey_`, con `TransparentStringHash`);
  - poblado en `MemberModel/BuildSupport.cpp:52-55`;
  - consultado en `OwnershipQueries.cpp:252-260`.

  Ni HIR, ni MIR, ni LLVM, ni Bodies lo usan.
- Las claves son **únicas** después de la agregación de parciales:
  - `finalizePredeclarationGroup` (`NameLookup/TypeAggregation.cpp:404-442`) agrega **un** tipo por grupo de identidad igual;
  - los grupos se forman ordenando por `key()` (`pendingTypeLess`, `:79-92`) y cortando por `identity ==` (`pendingTypeGroupEnd`, `:94-103`);
  - el comentario NSC-LIMIT-009 (`TypeIdentity.h:12-35`) documenta que la codificación de la clave es inyectiva.

  Por tanto "primer candidato con esa clave" es "el único".

**Cambio exacto (infraestructura del índice, una vez).**

1. **Archivo nuevo** `include/nativesharp/Frontend/TransparentStringHash.h`. Es un tipo por archivo. `ProjectMemberModel` puede pasar a usarlo después, pero no es obligatorio.
   ```cpp
   #pragma once

   #include <cstddef>
   #include <functional>
   #include <string_view>

   namespace nativesharp::frontend
   {
   // PERF (NSC-PERF-02): ES: hash heterogeneo para buscar con std::string_view sin construir std::string.
   // EN: heterogeneous hash so lookups by std::string_view do not build a std::string.
   struct TransparentStringHash final
   {
       using is_transparent = void;

       std::size_t operator()(std::string_view value) const noexcept
       {
           return std::hash<std::string_view>{}(value);
       }
   };
   }
   ```
2. `include/nativesharp/Frontend/ProjectNameLookup.h`, en la sección pública, junto a `namedTypes()`:
   ```cpp
       // PERF (NSC-PERF-02): ES: ordinal del tipo nombrado cuya identity.key() es `identityKey` (sin `type:`).
       // Las claves son unicas tras la agregacion de parciales. EN: ordinal of the named type whose key is
       // `identityKey` (without `type:`); keys are unique after partial aggregation.
       std::optional<std::size_t> typeOrdinalForIdentityKey(std::string_view identityKey) const noexcept;
       // PERF (NSC-PERF-02): ES: igual, para la forma `type:<clave>`; nullopt si no empieza por `type:`.
       // EN: same, for the `type:<key>` spelling; nullopt when it does not start with `type:`.
       std::optional<std::size_t> typeOrdinalForTypeKey(std::string_view typeKey) const noexcept;
   ```
   En la sección privada:
   ```cpp
       void indexTypeOrdinalsByIdentityKey();
       std::unordered_map<std::string, std::size_t, TransparentStringHash, std::equal_to<>>
           typeOrdinalByIdentityKey_;
       bool typeOrdinalByIdentityKeyBuilt_ = false;
   ```
   Añadir `#include "nativesharp/Frontend/TransparentStringHash.h"`.
3. **Archivo nuevo** `src/Frontend/NameLookup/TypeIdentityKeyIndex.cpp`, con las tres funciones (≤60 líneas cada una):
   ```cpp
   void ProjectNameLookup::indexTypeOrdinalsByIdentityKey()
   {
       typeOrdinalByIdentityKey_.clear();
       typeOrdinalByIdentityKey_.reserve(namedTypes_.size());
       for (std::size_t ordinal = 0; ordinal < namedTypes_.size(); ++ordinal)
       {
           // ES: try_emplace conserva el PRIMER ordinal, igual que el recorrido lineal que sustituye.
           // EN: try_emplace keeps the FIRST ordinal, exactly like the linear walk it replaces.
           typeOrdinalByIdentityKey_.try_emplace(namedTypes_[ordinal].identity.key(), ordinal);
       }
       typeOrdinalByIdentityKeyBuilt_ = true;
   }

   std::optional<std::size_t> ProjectNameLookup::typeOrdinalForIdentityKey(
       std::string_view identityKey) const noexcept
   {
       if (!typeOrdinalByIdentityKeyBuilt_)
       {
           // ES: modelos armados a mano (pruebas) sin build(): se conserva el recorrido original.
           // EN: hand-assembled models (tests) without build(): keep the original walk.
           for (std::size_t ordinal = 0; ordinal < namedTypes_.size(); ++ordinal)
           {
               if (namedTypes_[ordinal].identity.key() == identityKey) return ordinal;
           }
           return std::nullopt;
       }
       const auto found = typeOrdinalByIdentityKey_.find(identityKey);
       if (found == typeOrdinalByIdentityKey_.end()) return std::nullopt;
       return found->second;
   }

   std::optional<std::size_t> ProjectNameLookup::typeOrdinalForTypeKey(
       std::string_view typeKey) const noexcept
   {
       if (!typeKey.starts_with("type:")) return std::nullopt;
       return typeOrdinalForIdentityKey(typeKey.substr(5));
   }
   ```
4. `src/Frontend/ProjectNameLookup.cpp:94`. Después de `finalizePredeclarations(diagnostics);` añadir:
   ```cpp
       indexTypeOrdinalsByIdentityKey();
   ```
   `namedTypes_` solo crece en `appendNamedType` (`TypeAggregation.cpp:341`), que solo se llama desde `finalizePredeclarations`, así que el índice queda final. **HECHO:** `grep namedTypes_.push_back` devuelve un único sitio.
5. Añadir el `.cpp` nuevo a `CMakeLists.txt`, en la lista de fuentes de `Frontend/NameLookup`.

**Cambio exacto (sustitución en cada consulta).** Hay dos patrones y todos los sitios de la tabla son uno de ellos.

- **Patrón A.** La función devuelve el ordinal o el primero que coincide.

  Antes:
  ```cpp
  for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
  {
      if (nameLookup_->namedTypes()[ordinal].identity.key() == identity) return ordinal;
  }
  return std::nullopt;
  ```
  Después:
  ```cpp
  // PERF (NSC-PERF-02): ES/EN: indexed lookup; keys are unique after aggregation.
  return nameLookup_->typeOrdinalForIdentityKey(identity);
  ```
- **Patrón B.** La función busca el tipo y evalúa un predicado sobre él, con `continue` si la clave o el `kind` no coinciden.

  Antes:
  ```cpp
  for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
  {
      if (type.identity.key() != key) continue;
      return <predicado(type)>;
  }
  return <valor-por-defecto>;
  ```
  Después:
  ```cpp
  const std::optional<std::size_t> ordinal = nameLookup_->typeOrdinalForIdentityKey(key);
  if (!ordinal.has_value()) return <valor-por-defecto>;
  const NamedTypeDeclaration& type = nameLookup_->namedTypes()[*ordinal];
  return <predicado(type)>;
  ```
  Si la condición original era `key != X || kind != K → continue` (o `|| syntax == nullptr → continue`), después de la búsqueda se escribe `if (type.kind != K) return <valor-por-defecto>;`. Es equivalente porque la clave es única: el bucle no podía encontrar otro candidato.

**Tabla completa de sitios R-02.**

- Columnas: la línea del bucle y la de la comparación; la función contenedora con su rango; el patrón; y el número de llamadores según el grafo de libclang.
- Todos estos sitios se ejecutan **por consulta**, no una vez por build.
- Las pasadas únicas que recorren tipos para construir algo se excluyen expresamente al final.

| # | Archivo:bucle/comparación | Función (rango) | Patrón | Llamadores | Nota |
|---|---|---|---|---|---|
| 1 | `Frontend/ProjectInheritanceModel.cpp:272/274` | `ProjectInheritanceModel::typeOrdinalForTypeKey` (264-280) | A | **40+** | Centro de todo: lo usan `ProjectConversionClassifier::classifyImplicit:201-203` (×2 por clasificación), `findUserDefined:246-247`, `isReferenceType:298`, `isClassReferenceType:306`, `ProjectMethodBodyModel::namedTypeOrdinalForTypeKey` (`AssignmentAnalyzer.cpp:2609-2617`) y `accessorReceiverTypeOrdinalForTypeKey` (`ConversionAndPassKindChecks.cpp:241-258`). Cambiar el cuerpo por `return nameLookup_->typeOrdinalForTypeKey(typeKey);` (el prefijo ya se valida dentro). |
| 2 | `Frontend/ProjectInterfaceModel.cpp:335/337` | `ProjectInterfaceModel::typeOrdinalForTypeKey` (327-343) | A | 3 | Igual que el #1. |
| 3 | `Frontend/ProjectGenericConstraintModel.cpp:71/73` | `declarationForTypeKey` (63-78) | A, con `ordinalOut` | 4 | Después: `const auto o = nameLookup.typeOrdinalForIdentityKey(identity); if (!o) return nullptr; ordinalOut = *o; return &types[*o];` |
| 4 | `Frontend/ProjectObjectLayoutModel.cpp:222/225` | `valueLayoutForTypeKey` (189-264) | B | **20** (9 en bucles) | `"type:" + declaration.identity.key() != typeKey` hace **2 asignaciones por tipo**. Sustituir el `for` por `typeOrdinalForTypeKey(typeKey)` y dejar el cuerpo interno (225-260) igual sobre `declaration = namedTypes()[*ordinal]`. Además tiene recursión para arreglos fijos (líneas 212-219), que es preexistente y no se toca. |
| 5 | `Frontend/ProjectObjectLayoutModel.cpp:615/618` | `hasTrivialStructLifetime` (605-638) | B | 2 | |
| 6 | `Frontend/ProjectObjectLayoutModel.cpp:653/655` | `structBitFieldWidth` (640-686) | B | 1 (en bucle) | |
| 7 | `Frontend/ProjectObjectLayoutModel.cpp:1065/1067` | `isClassBitFieldBase` (1059-1071) | B | — | |
| 8 | `Frontend/HIR/HirBuildCoordinator.cpp:2112/2114` | `ProjectOopHirModel::isStructValueType` (2107-2117) | B | **26** | Llamado por nodo HIR. |
| 9 | `Frontend/HIR/HirBuildCoordinator.cpp:2126/2128` | `isClassReferenceType` (2119-2134) | B | — | |
| 10 | `Frontend/HIR/HirBuildCoordinator.cpp:2155/2158` | `isReferenceType` (2145-2166) | B | 11 (4 en bucles) | |
| 11 | `Frontend/HIR/HirExpressionMachine.cpp:480-483` (+`isEnumTypeWithIdentity` 468-474) | `isEnumBinaryTypeKey` (476-484) | B (`any_of` + `bind_front`) | 3 | Después: `const auto o = nameLookup_->typeOrdinalForTypeKey(typeKey); return o && nameLookup_->namedTypes()[*o].kind == NamedTypeDeclarationKind::Enum;`. Conservar `isEnumTypeWithIdentity`, que puede tener otros usos o pruebas. |
| 12 | `Frontend/HIR/HirCallLowering.cpp:1646/1649` | `lowerEnumToStringCoreCall` (1592-1664) | A | — | |
| 13 | `Frontend/HIR/ExpressionStatementMachine/HirCoreCallFrameLowering.cpp:589/592` | `initializeEnumToStringCallFrame` (561-611) | A | — | |
| 14 | `Frontend/MIR/Internal/MirNodeMachine.cpp:1976/1978` | `MirNodeMachine::lowerGroup21` (1950-2008) | A | — | Sustituir el bucle 1976-1983 por `spanTypeOrdinal = model_.nameLookup_->typeOrdinalForIdentityKey(identity);` |
| 15 | `Frontend/Bodies/MethodBodyTypeQueries.cpp:48/50` | `typeKeyHasDefinedDefaultStorage` (32-56) | B | 2 | |
| 16 | `Frontend/Bodies/MethodBodyTypeQueries.cpp:65/67` | `typeKeyIsStruct` (60-71) | B | 6 | |
| 17 | `Frontend/Bodies/TypeKeyPredicates.cpp:61/63` | `isEnumTypeKey` (57-67) | B | 13 | |
| 18 | `Frontend/Bodies/TypeKeyPredicates.cpp:97/99` | `elementTypeHasTrivialLifetime` (81-124) | A + B | 10 | Además recorre `memberModel_->members()` en :113. Ver R-13. |
| 19 | `Frontend/Bodies/TypeKeyPredicates.cpp:133/135` | `isExternCDelegateTypeKey` (126-140) | B | 9 | |
| 20 | `Frontend/Bodies/ConstructionAnalyzer.cpp:45/47` | `aggregateIsUnion` (41-51) | B (`|| syntax==nullptr`) | 4 | |
| 21 | `Frontend/Bodies/ConstructionAnalyzer.cpp:57/59` | `aggregateRequiresActiveMember` (53-63) | B | 4 | |
| 22 | `Frontend/Bodies/ConstructionAnalyzer.cpp:77/79` | `aggregateAllowsSynthesizedMember` (65-149) | B | 3 | |
| 23 | `Frontend/Bodies/ConstructionAnalyzer.cpp:299/301` | `aggregateIsBorrowedView` (295-305) | B | 1 | |
| 24 | `Frontend/Bodies/ConstructionAnalyzer.cpp:329/331` | `aggregateElementTypeKeys` (307-347) | B | 2 | `typeKey.substr(5)` en la comparación. |
| 25 | `Frontend/Bodies/ConstructionAnalyzer.cpp:376/378` | `aggregateElementNames` (349-392) | B | 2 | |
| 26 | `Frontend/Bodies/LambdaDelegateBinder.cpp:64/67` | `delegateTypeOrdinalForTypeKey` (59-73) | B (kind Delegate) | 5 | |
| 27 | `Frontend/Bodies/ConversionAndPassKindChecks.cpp:147/149` | `classifyCVariadicPromotion` (119-184) | B | 3 | |
| 28 | `Frontend/Bodies/ConversionAndPassKindChecks.cpp:287/289` | `isAnyDelegateTypeKey` (282-293) | B | 8 | |
| 29 | `Frontend/Bodies/CallAnalyzer.cpp:1855/1858` | `recognizeCoreThreadOperations` (1811-1909) | FQN | 1 | Compara FQN; con R-01 queda como comparación de `const&`. Mejor todavía: resolver `ThreadStart` una vez por modelo. Ver R-13.4. |
| 30 | `Frontend/Bodies/CallAnalyzer.cpp:3073/3075` | `analyzeNativeStringPatternRestCall` (3054-3092) | FQN | 1 | Igual que el #29. |
| 31 | `Frontend/Bodies/CallAnalyzer.cpp:1917` | `isMemoryOrderTypeKey` (1911-1919) | FQN sobre ordinal ya resuelto | 6 | Con R-01 es suficiente. |
| 32 | `Frontend/Bodies/MemberAccessAnalyzer.cpp:412/414` | `analyzeNativeStringFacadeMember` (391-460) | FQN | 1 | Igual que el #29. |
| 33 | `Frontend/Bodies/StatementAnalyzer.cpp:2112/2116` | `exceptionRootTypeOrdinal` (2106-2122) | FQN | 4 | Por `throw`, `catch` y `using`. Ver R-13.5. |
| 34 | `Frontend/Bodies/StatementAnalyzer.cpp:2188/2192` | `disposableTypeOrdinal` (2182-2198) | FQN | 1 | Igual que el #33. |
| 35 | `Frontend/Bodies/AssignmentAnalyzer.cpp:2647` | `resolveTypeIdentifier` (2634-2660) | compara `identity.name()` | 1 | `name()` ya es `const&`, así que solo cuesta `O(T)` comparaciones. Ver R-13.6 (índice por nombre simple). |
| 36 | `Frontend/MemberModel/BuildAndDeclaration.cpp:103/105` | `namedTypeForKey` (97-108) | A | 7 | |
| 37 | `Frontend/MemberModel/TypeIdentity.cpp:294/296` | `canonicalGenericArgumentKey` (263-307) | `key()==s \|\| FQN==s` | 5 | La parte `key()==` usa el índice; la parte `FQN==` no tiene índice. Con R-01 queda en comparaciones. Dejar el bucle, pero con `const&`. |
| 38 | `Frontend/MemberModel/TypeIdentity.cpp:180-184` | `canonicalNamedTypeKey` (fallback `ends_with`) | — | 1 | Solo en la ruta de "clon sin binding". Con R-01 queda en comparaciones. No se indexa: `ends_with` no se presta. |
| 39 | `Frontend/MemberModel/BuildSupport.cpp:95` | `collectExternalTraitImplMembers` (83-…) | compara `name()` | por impl externo | Con R-01 basta. |
| 40 | `LLVM/LlvmTypeLayout.cpp:30/32` | `findEnumUnderlyingInfo` (25-38) | B (kind Enum) | **11** | **Peor caso**: para un struct, la clave coincide pero el `kind` no, así que hace `continue` y **recorre toda la lista**. Lo usa `mapNonBuiltinLlvmType` en cada mapeo de tipo (R-05). |
| 41 | `LLVM/LlvmTypeClassification.cpp:21/23` | `isDelegateReferenceType` (13-27) | B | 7 | Por mapeo de tipo (R-05). |
| 42 | `LLVM/LlvmTypeClassification.cpp:34/36` | `isClassReferenceType` (29-41) | B | 8 | Igual que el #41. |
| 43 | `LLVM/Lowering/OopLlvmDebugInfo.cpp:345/347` | `debugNamedType` (341-350) | A (devuelve puntero) | 4 | |
| 44 | `LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:578/580` | `isExternCDelegateType` (571-585) | B | 6 | |
| 45 | `LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:594/596` | `isVectorCallDelegateType` (587-602) | B | 4 | |
| 46 | `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:266/268-270` | `isFloat32x4TypeKey` (262-273) | B + FQN | 3 | |
| 47 | `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:279/281` | `compactFloatVectorLaneCount` (275-291) | B + FQN (`const std::string name = ...` es una **copia**; pasar a `const std::string&`) | 1 | |
| 48 | `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:297/299` | `integerSimd128LaneBits` (293-308) | Igual que el #47 | 1 | |
| 49 | `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:314/316` | `floatingSimd256LaneBits` (310-325) | Igual que el #47 | 1 | |
| 50 | `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:331/333` | `integerSimd256LaneBits` (327-342) | Igual que el #47 | 1 | |
| 51 | `Interop/CHeaderGenerator.cpp:136/138` | `namedTypeForCoreKey` (130-141) | A | 5 | Solo al generar headers C de una biblioteca. Prioridad baja. |
| 52 | `Interop/CHeaderGenerator.cpp:289/291` | `collectReachableHostTypes` (253-329) | A en bucle | — | Igual que el #51. |

**Excluidos por ser pasada única** (HECHO, revisados uno a uno):

- `HirBuildCoordinator.cpp:2028` (`buildImplicitConstructors`);
- `ProjectInterfaceModel.cpp:107` y `:1253`;
- `MethodBodyBuildCoordinator.cpp:131` (`validateDeclaredErrorTypes`);
- `ProjectInheritanceModel.cpp:28` (`build`) y `:307` (`objectRootTypeOrdinal`, con un solo llamador, `:462`);
- `ProjectObjectLayoutModel.cpp:97` y `:105`;
- `AnonymousMemberPromotion.cpp:211`;
- `BuildSupport.cpp:53`, `:62`, `:153` y `:199`;
- `TypeAggregation.cpp:450` (`finalizeContainingTypeOrdinals`, que además construye un `TypeIdentity` temporal por tipo anidado; aceptable);
- `BuiltinTypeValidation.cpp:120` (`appendObjectTypeRoot`, solo cuando el usuario escribe `object` y la búsqueda normal falló).

**Pruebas.** El fallback de `typeOrdinalForIdentityKey` cuando el índice no se construyó mantiene el comportamiento de los fixtures que arman `namedTypes_` a mano. Hay que añadir una prueba unitaria nueva: build de un proyecto con un tipo anidado `A+B` y un tipo de namespace `A.B`, y verificar que `typeOrdinalForIdentityKey` devuelve ordinales distintos y coincide con el recorrido lineal para **todos** los tipos.


---

## 3. Hallazgos por fase

### R-03 · HIR: trabajo cuadrático en la construcción de funciones regulares · P0

Archivo principal: `src/Frontend/HIR/HirBuildCoordinator.cpp`.

#### R-03.1 · Un *worker* completo por cada miembro, incluidos los precompilados y los que se saltan

**HECHO.**

- `buildRegularFunctionsInParallel` (736-779) recorre **todos** los miembros (743-744).
- Solo filtra por `kind` y `isAbstract` (746-752).
- Por cada miembro restante hace `workers.submit(...)`. Dentro de cada trabajo:
  - construye un `ProjectOopHirModel worker;` completo (754), que tiene **22** `std::unordered_map` miembro (`ProjectOopHirModel.h`, bloque `...BySyntax_`);
  - llama `worker.buildImpl(..., memberOrdinal, this)` (756-770).
- `buildImpl` (2036-2091) hace en cada worker:
  - `functionByMember_.resize(memberModel.members().size());` (2068);
  - `functionByNamespaceFunction_.resize(...)` (2069-2070).

  Son vectores de `std::optional<std::size_t>` (16 B por entrada). **Cada worker** reserva e inicializa `M` entradas.
- Luego `buildRegularFunctionsSequentially(memberFilter)` (1538-1548) **recorre otra vez los `M` miembros** comparando `*memberFilter != memberOrdinal`.
- Dentro de `buildRegularFunction` (1487-1536), los miembros **precompilados** de Core solo producen una declaración de importación (`appendPrecompiledRegularFunction`, 878-924). Los importados de C también (`appendImportedCRegularFunction`, 926-966). Los que cumplen `regularFunctionShouldBeSkipped` (826-873) no producen nada.

**Costo (INFERENCIA).**

- `O(M)` trabajos × (`O(M)` de `resize` + `O(M)` de filtro) = `O(M²)` en memoria tocada y en comparaciones.
- A eso se suman 22 mapas por worker. MSVC `std::unordered_map` reserva en el constructor por defecto el nodo centinela y los buckets: INFERENCIA basada en la implementación conocida de MSVC STL, no medido aquí.
- Con miles de miembros precompilados de Core, la mayor parte de los trabajos **no tienen cuerpo que bajar**.

**Cambio exacto.**

1. **Extraer la construcción de la función importada sin efectos sobre el modelo.**
   - En `appendPrecompiledRegularFunction` (878-924), el cuerpo actual construye `imported` (889-920) y luego hace la contabilidad (921-922: `functionByMember_[memberOrdinal] = functions_.size(); functions_.push_back(...)`).
   - Mover las líneas 889-920 a una función nueva y `const`:
     ```cpp
     // PERF (NSC-PERF-03): ES: construye la declaracion importada sin tocar functions_/functionByMember_.
     // EN: builds the imported declaration without touching functions_/functionByMember_.
     bool ProjectOopHirModel::makePrecompiledRegularFunction(
         std::size_t memberOrdinal,
         const ClassMemberSymbol& member,
         ProjectOopHirFunction& imported,
         std::string& error) const;
     ```
   - `appendPrecompiledRegularFunction` queda así:
     ```cpp
     ProjectOopHirFunction imported;
     if (!makePrecompiledRegularFunction(memberOrdinal, member, imported, error)) return false;
     functionByMember_[memberOrdinal] = functions_.size();
     functions_.push_back(std::move(imported));
     return true;
     ```
   - Hacer lo mismo con `appendImportedCRegularFunction` (926-966) usando `makeImportedCRegularFunction(memberOrdinal, member, imported)`. La lógica de las líneas 934-962 se mueve tal cual. **No se borra ningún comentario**: los comentarios C1.2 e Interop se mueven con el código.
2. **Resolver en el coordinador los casos que no bajan cuerpo.** En `buildRegularFunctionsInParallel`, sustituir el filtro de las líneas 746-752 por el mismo orden de decisiones que `buildRegularFunction` (1491-1500):
   ```cpp
   const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
   RegularFunctionResult& direct = results[memberOrdinal];
   if (regularFunctionShouldBeSkipped(memberOrdinal, member))
   {
       direct.success = true;
       continue;
   }
   if ((member.kind == ClassMemberKind::Method || member.kind == ClassMemberKind::Constructor) &&
       member.isPrecompiled && !member.isImportedC)
   {
       ProjectOopHirFunction imported;
       direct.success = makePrecompiledRegularFunction(memberOrdinal, member, imported, direct.error);
       if (direct.success) direct.functions.push_back(std::move(imported));
       continue;
   }
   if (member.isImportedC)
   {
       ProjectOopHirFunction imported;
       makeImportedCRegularFunction(memberOrdinal, member, imported);
       direct.functions.push_back(std::move(imported));
       direct.success = true;
       continue;
   }
   workers.submit(...);   // ES/EN: only members with a body to lower reach a worker.
   ```
   - `regularFunctionShouldBeSkipped` ya contiene el filtro de `kind` y `isAbstract` (830-833), así que el filtro antiguo queda absorbido.
   - Para respetar el límite de 60 líneas, extraer este bloque a `bool resolveRegularFunctionWithoutBody(std::size_t memberOrdinal, RegularFunctionResult& result) const`, que devuelve `true` si lo resolvió.

   **Equivalencia.**
   - `mergeParallelRegularFunctions` (781-824) recorre `results` en orden de `memberOrdinal` y ejecuta `functions_.push_back(...)` en ese orden, así que el orden final de `functions_` es idéntico.
   - El worker producía exactamente esa función para esos miembros: `scanParallelOrdinals` y `applyParallelOrdinalOffsets` sobre `body == nullptr` no cuentan nada (verificar que aceptan `nullptr`; hoy ya reciben esas funciones desde el worker).
   - `lastErrorRange` no lo fija `appendPrecompiledRegularFunction`.
3. **El worker no necesita tablas de tamaño `M`.** En `buildImpl` (2068-2070), antes:
   ```cpp
       functionByMember_.resize(memberModel.members().size());
       if (namespaceFunctionModel_ != nullptr)
           functionByNamespaceFunction_.resize(namespaceFunctionModel_->functions().size());
   ```
   Después:
   ```cpp
       // PERF (NSC-PERF-03): ES: un worker (lookupSource != nullptr con filtro) descarta estas tablas; no se
       // dimensionan a M por cada miembro. EN: a worker discards these tables; do not size them to M per member.
       const bool isMemberWorker = lookupSource != nullptr && memberFilter.has_value();
       if (!isMemberWorker)
       {
           functionByMember_.resize(memberModel.members().size());
           if (namespaceFunctionModel_ != nullptr)
               functionByNamespaceFunction_.resize(namespaceFunctionModel_->functions().size());
       }
   ```
   - Añadir el helper privado:
     ```cpp
     void ProjectOopHirModel::recordFunctionForMember(std::size_t memberOrdinal, std::size_t functionIndex)
     {
         if (memberOrdinal < functionByMember_.size()) functionByMember_[memberOrdinal] = functionIndex;
     }
     ```
   - Usarlo en lugar de la escritura directa en las líneas **921**, **963** y **1479**.
   - **No** cambiar la 797 (`mergeParallelRegularFunctions`): el coordinador sí tiene la tabla completa.
   - **HECHO:** las únicas lecturas de `functionByMember_` son:
     - `markLambdaClosureCells:1847-1850`, que solo se ejecuta en el coordinador porque el worker sale en `buildImpl` con `memberFilter` antes de la 2083;
     - `functionForMember` (`HirModelFacade.cpp:76-87`), API pública que nadie invoca sobre un worker.
4. **Filtro directo.** `buildRegularFunctionsSequentially` (1538-1548). Antes:
   ```cpp
       for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel_->members().size(); ++memberOrdinal)
       {
           if (memberFilter.has_value() && *memberFilter != memberOrdinal) continue;
           if (!buildRegularFunction(memberOrdinal, error)) return false;
       }
       return true;
   ```
   Después: añadir al principio
   ```cpp
       if (memberFilter.has_value())
       {
           // PERF (NSC-PERF-03): ES/EN: a filtered build lowers exactly one member; no O(M) walk.
           return *memberFilter >= memberModel_->members().size() || buildRegularFunction(*memberFilter, error);
       }
   ```
   El bucle original queda para el caso sin filtro.

#### R-03.2 · Cada función recorre **todos** los `locals()` y `cleanupRegions()` del proyecto

**HECHO.** `methodBodyModel_->locals()` y `cleanupRegions()` son los vectores **fusionados de todo el proyecto**: `ProjectCallableResultMerger::appendShardCollections`, `ProjectCallableResultMerger.cpp:223-262`, añade los de cada shard. Los recorren:

| Línea | Función | Qué filtra |
|---|---|---|
| 1015-1020 | `initializeRegularFunction` (967-1022) | `local.methodMemberOrdinal == memberOrdinal && !local.lambdaOrdinal` |
| 1029-1057 | `appendRegularFunctionParameterCleanups` (1024-1058) | `region.methodMemberOrdinal == function.memberOrdinal`, `kind ∈ {OwnedParameterDestroy, ValueParameterDrop}`, `resourceParameterOrdinal` |
| 1590-1600 | `appendNamespaceFunctionParameterCleanups` (1582-…) | Igual, con `callableOrdinal` |
| 1666-1671 | `buildNamespaceFunction` (1622-…) | `local.methodMemberOrdinal == callableOrdinal && !local.lambdaOrdinal` |
| 1796-1802 | `buildLambdaFunction` (1759-…) | `local.lambdaOrdinal == lambda.lambdaOrdinal && !local.lambdaParameterOrdinal` |

**Costo.** `O(F × L)` y `O(F × C)`, donde `F` son las funciones, `L` los locals y `C` las regiones de limpieza del proyecto. Se ejecuta dentro de cada worker.

**Cambio exacto.**

1. **Miembros nuevos** en `ProjectOopHirModel.h`, junto a `localBySyntax_`:
   ```cpp
       // PERF (NSC-PERF-03): ES: indices por callable/lambda construidos una vez por el coordinador.
       // EN: per-callable/per-lambda indexes built once by the coordinator.
       std::vector<std::vector<std::size_t>> localOrdinalsByCallable_;
       std::vector<std::vector<std::size_t>> localOrdinalsByLambda_;
       std::vector<std::vector<std::size_t>> parameterCleanupOrdinalsByCallable_;
   ```
2. **Construcción.** En `indexBuildBindings` (706-734), que ya sale si `lookupSource_ != nullptr` (710), añadir al final la llamada a una función nueva `indexCallableLocalsAndCleanups(methodBodyModel)`:
   - Recorrer `methodBodyModel.locals()` una vez, en orden. Si `!local.lambdaOrdinal`, añadir a `localOrdinalsByCallable_[local.methodMemberOrdinal]`. Si `local.lambdaOrdinal && !local.lambdaParameterOrdinal`, añadir a `localOrdinalsByLambda_[*local.lambdaOrdinal]`. Redimensionar con `max(size, idx+1)`.
   - Recorrer `methodBodyModel.cleanupRegions()` una vez, en orden. Si `kind ∈ {OwnedParameterDestroy, ValueParameterDrop} && resourceParameterOrdinal`, añadir a `parameterCleanupOrdinalsByCallable_[region.methodMemberOrdinal]`.
3. **Accesores.** Siguen el patrón exacto de `HirBindingIndex.cpp:7`:
   ```cpp
   std::span<const std::size_t> ProjectOopHirModel::callableLocalOrdinals(std::size_t callableOrdinal) const noexcept
   {
       const auto& index = lookupSource_ == nullptr ? localOrdinalsByCallable_ : lookupSource_->localOrdinalsByCallable_;
       if (callableOrdinal >= index.size()) return {};
       return index[callableOrdinal];
   }
   ```
   Lo mismo para `lambdaLocalOrdinals(lambdaOrdinal)` y `parameterCleanupOrdinals(callableOrdinal)`.
4. **Sustituciones.**
   - **1015-1020.** Antes:
     ```cpp
         for (std::size_t localOrdinal = 0; localOrdinal < methodBodyModel_->locals().size(); ++localOrdinal)
         {
             const MethodLocalSymbol& local = methodBodyModel_->locals()[localOrdinal];
             if (local.methodMemberOrdinal == memberOrdinal && !local.lambdaOrdinal.has_value())
                 function.localOrdinals.push_back(localOrdinal);
         }
     ```
     Después:
     ```cpp
         const std::span<const std::size_t> ownLocals = callableLocalOrdinals(memberOrdinal);
         function.localOrdinals.assign(ownLocals.begin(), ownLocals.end());
     ```
   - **1666-1671:** igual, con `callableOrdinal`.
   - **1796-1802:** igual, con `lambdaLocalOrdinals(lambda.lambdaOrdinal)`.
   - **1029-1036 y 1590-1600:** el `for` sobre todas las regiones pasa a ser `for (const std::size_t cleanupOrdinal : parameterCleanupOrdinals(function.memberOrdinal))`. El cuerpo, de la comprobación `parameterOrdinal >= member.parameters.size()` en adelante, queda igual. El `if (... ) continue;` de filtro se elimina porque el índice ya lo aplicó; **conservar el comentario** si lo hubiera.

**Equivalencia.** El índice conserva el orden ascendente de los ordinales, igual que el bucle. Las condiciones son literalmente las mismas.

#### R-03.3 · Menores en HIR (P2)

- `markLambdaClosureCells` (1827-1880): para lambdas anidadas busca su creadora recorriendo **todo** `functions_` (1838-1845). Antes del bucle, construir una sola vez un `std::vector<std::optional<std::size_t>> functionByLambdaOrdinal` con un recorrido de `functions_`. Con eso queda `O(Λ)` en vez de `O(Λ × F)`.
- `mangleCallable` (~650-667): `compilation::symbolFragmentForIdentityKey(type.identity.key())` se calcula por función. Con R-01 `key()` es gratis. La codificación hex sigue haciéndose una vez por función; memoizar por `declaringTypeOrdinal` (vector de `std::optional<std::string>`) solo si la medición lo justifica.

---

### R-04 · MIR: copia de layouts por función y búsquedas lineales de layouts · P0

#### R-04.1 · `structLayouts_` copiado entero por cada función HIR

**HECHO.**

- `MirFunctionLowerer.cpp` `lowerFunctions` (847-…). En modo paralelo, por **cada** función HIR (864-875) construye `ProjectOopMirModel lowerer;` y llama `configureFunctionLowerer(...)`.
- `configureFunctionLowerer` (827-845), en la línea **844**, hace `lowerer.structLayouts_ = structLayouts_;`. Es una copia profunda de `std::vector<ProjectOopMirStructLayout>`, donde cada elemento tiene strings y un `vector<ObjectFieldLayout>`.
- `lowerOneFunction` (797-825) **ya recibe** `std::span<const ProjectOopMirStructLayout> functionLayouts` y lo usa en `configureFunctionAbi` y `buildPollFrame`.
- El único otro lector interno del miembro copiado es `MirNodeMachine`, en `Internal/MirNodeMachine.cpp:1593` (`FutureFieldAddress`) y `:1626` (`lowerAwaitFuture`). Las dos hacen una búsqueda lineal por `typeKey`.

**Costo.** `O(F × S)` copias profundas.

**Cambio exacto.**

1. En `ProjectOopMirModel.h`, sección privada, añadir sin tocar `structLayouts_`, que está congelado por las pruebas:
   ```cpp
       // PERF (NSC-PERF-04): ES: un lowerer por funcion lee las layouts del modelo dueño sin copiarlas.
       // EN: a per-function lowerer reads the owner's layouts without copying them.
       const std::vector<ProjectOopMirStructLayout>* sharedStructLayouts_ = nullptr;
       const std::vector<ProjectOopMirStructLayout>& activeStructLayouts() const noexcept;
   ```
   Con esta implementación:
   ```cpp
   const std::vector<ProjectOopMirStructLayout>& ProjectOopMirModel::activeStructLayouts() const noexcept
   {
       return sharedStructLayouts_ != nullptr ? *sharedStructLayouts_ : structLayouts_;
   }
   ```
2. `MirFunctionLowerer.cpp:844`. Antes: `lowerer.structLayouts_ = structLayouts_;`. Después: `lowerer.sharedStructLayouts_ = &structLayouts_;`.
3. `MirNodeMachine.cpp:1593` y `:1626`: `model_.structLayouts_` pasa a ser `model_.activeStructLayouts()`.
4. **Verificar con grep** que ningún otro código del lowerer temporal lee `structLayouts_`. Ya está hecho: los 15 usos están en `MirLayoutImporter`, en el accesor `structLayouts()` (`MirBuildCoordinator.cpp:723-725`, que solo se llama sobre el modelo dueño) y en los dos sitios del punto 3.

**Equivalencia.** Las layouts son las mismas y el modelo dueño no las modifica durante `lowerFunctions`. Los `push_back` solo ocurren en `MirLayoutImporter`, antes del lowering.

#### R-04.2 · `ProjectObjectLayoutModel::fieldLayoutForMember` recorre todos los campos del proyecto

**HECHO.**

- `src/Frontend/ProjectObjectLayoutModel.cpp:161-181` recorre:
  1. todos los `structLayouts_` y sus `fields`;
  2. todos los `classLayouts_` y sus `fields`;
  3. `promotedFieldLayouts_`.

  Se queda con el primero cuyo `field.memberOrdinal == memberOrdinal`.
- Se llama:
  - por **cada acceso a campo** en MIR: `ProjectOopMirModel::fieldLayout` (`MirBuildCoordinator.cpp:848-853`), llamado desde `MirNodeMachine.cpp:175`, `:1028-1029`, `:1551` y `:1751`;
  - en HIR: `HirExpressionMachine.cpp:1483-1486`, `HirCompletionAccessFrames.cpp:465-467`, `HirAggregateLowering.cpp:940`;
  - en MIR: `MirBuildCoordinator.cpp:365`.

**Costo.** `O(accesos × campos totales)`, con Core incluido.

**Cambio exacto.**

1. `ProjectObjectLayoutModel.h`, sección privada:
   ```cpp
       // PERF (NSC-PERF-04): ES: memberOrdinal -> layout de campo, con la misma precedencia que el recorrido
       // (struct, luego class, luego promovidos; gana el primero). EN: same precedence as the walk.
       std::vector<const ObjectFieldLayout*> fieldLayoutByMember_;
       void indexFieldLayoutsByMember();
   ```
2. `indexFieldLayoutsByMember()`:
   - `fieldLayoutByMember_.assign(memberModel_->members().size(), nullptr)`;
   - después tres bucles en el **mismo orden** que 163-180, asignando solo si la entrada sigue siendo `nullptr`.
3. Llamarla en `build()` (`ProjectObjectLayoutModel.cpp:65-123`) **inmediatamente antes** de `built_ = true;` (línea 122). Ahí ya terminaron `buildPromotedFieldLayouts` y `buildAllocationPlans`, y no hay más `push_back` en esos tres vectores.
4. Al principio de `fieldLayoutForMember` (161):
   ```cpp
       if (!fieldLayoutByMember_.empty())
       {
           return memberOrdinal < fieldLayoutByMember_.size() ? fieldLayoutByMember_[memberOrdinal] : nullptr;
       }
   ```
   El recorrido original queda como fallback. Lo usa `buildPromotedFieldLayouts` (1258 y 1273) **antes** de que exista el índice, y también los fixtures.

**Equivalencia.**
- La precedencia es la misma.
- Un `memberOrdinal` fuera de rango (`>= members().size()`) devolvía `nullptr` en el recorrido, salvo que algún layout tuviera ese ordinal. **NO VERIFICADO** que nunca ocurra. Si se quiere blindar, dimensionar el vector con `max(members().size(), max memberOrdinal + 1)`, calculado en los mismos tres recorridos.

#### R-04.3 · Búsqueda lineal de layout por `typeKey` en MIR y LLVM

**HECHO.**

- `LLVM/LlvmTypeLayout.cpp:5-13` (`findStructLayout`) y `:15-23` (`findClassLayout`) recorren `mirModel.structLayouts()` y `classLayouts()` comparando `layout.typeKey == typeKey`.
- `findStructLayout` tiene 19 llamadores, uno de ellos en bucle.
- Se ejecuta en **cada mapeo de tipo** de LLVM (`Support/LlvmTypeMapping.cpp:156-157`) y en ABI, limpieza, etc.

**Cambio exacto.**

1. `ProjectOopMirModel.h`, añadir:
   ```cpp
   const ProjectOopMirStructLayout* structLayoutForTypeKey(std::string_view typeKey) const noexcept;
   const ProjectOopMirClassLayout* classLayoutForTypeKey(std::string_view typeKey) const noexcept;
   ```
   Con dos `unordered_map<std::string, std::size_t, TransparentStringHash, std::equal_to<>>` poblados con `try_emplace`, para que gane el primero.
2. Poblarlos en `ProjectOopMirModel::build` (`MirBuildCoordinator.cpp:618-681`) justo antes de `lowerFunctions` (657). En ese punto los importadores ya corrieron dentro de `initializeBuild`. **Verificar** que ningún `structLayouts_.push_back` (`MirLayoutImporter.cpp:94`, `:177`, `:307`, `:422`) se llama después de esa línea.
3. `findStructLayout` y `findClassLayout` delegan en esos métodos. MirNodeMachine (1593 y 1626) puede usar `structLayoutForTypeKey` en lugar del bucle: el modelo dueño se alcanza vía `sharedStructLayouts_`, o se expone un puntero al dueño.
4. **Fallback** si los mapas están vacíos: el bucle original, para los fixtures.

#### R-04.4 · `configureFunctionAbi` recorre todas las layouts por cada parámetro de cada función

**HECHO.**

- `MirFunctionLowerer.cpp:256-262` (`abiPassingForType`) recorre **todas** las struct layouts comparando `layout.typeKey == typeKey`.
- `configureFunctionAbi` (264-282) la llama para el retorno y para **cada parámetro** de **cada** función. `lowerOneFunction:805` llama a `configureFunctionAbi` **antes** de salir para las funciones importadas de Core (806), así que el costo es `O(F × P × S)` incluyendo todo Core.
- `sumAwaitedFrameSizeNodes` (290-310) hace lo mismo por nodo `await`.

**Cambio exacto.**

- Dar a `abiPassingForType` y `sumAwaitedFrameSizeNodes` una sobrecarga que reciba el índice de R-04.3 (`const ProjectOopMirModel&` dueño, o el mapa `typeKey → ordinal`).
- En `lowerFunctions` (847-…), el índice ya está construido (R-04.3, antes de la línea 657 del coordinador), así que se pasa por `const&` a `lowerOneFunction` y de ahí a `configureFunctionAbi`.
- Las firmas con `span` se conservan como fallback lineal.

**Equivalencia.** Gana el primero por ordinal (`try_emplace`) y, si no hay layout, sigue devolviendo `Direct`.

---

### R-05 · LLVM: el mapeo de tipos repite trabajo y no tiene memo · P0

**HECHO.**

- `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:11-19` (`OopLlvmImplementation::llvmType`) llama `mapBuiltinLlvmType`, luego `mapPointerLikeLlvmType`, y después `mapLlvmType(...)`.
- `mapLlvmType` (`Support/LlvmTypeMapping.cpp:179-191`) **vuelve a llamar** `mapBuiltinLlvmType` y `mapPointerLikeLlvmType` antes de `mapNonBuiltinLlvmType`.
- `mapPointerLikeLlvmType` (89-103) llama `isClassReferenceType` y `isDelegateReferenceType` (`LlvmTypeClassification.cpp:13-41`). Cada una recorre `namedTypes()` con `key()` hasta encontrar el tipo.
- `mapNonBuiltinLlvmType` (105-176) llama `findEnumUnderlyingInfo` (`LlvmTypeLayout.cpp:25-38`). Para un struct recorre **todos** los tipos, porque coincide la clave pero no el `kind` y hace `continue`. Después llama `findStructLayout` (lineal).
- Total, para un struct de usuario: 2 × (`isClass` + `isDelegate`) + 1 recorrido completo + 1 recorrido de layouts. En total son unos 4 o 5 recorridos `O(T)`, con un string `key()` por candidato, **en cada `llvmType(...)`**. Hay 144 sitios en `LLVM/Lowering/` que llaman `llvmType(`.

**Cambio exacto.**

1. **R-05.1 (lote 1, trivial).** `OopLlvmTypesAbiAttributes.cpp:11-19`. Antes:
   ```cpp
   LLVMTypeRef OopLlvmImplementation::llvmType(std::string_view typeKey, std::string& error) const
   {
           if (LLVMTypeRef builtin = mapBuiltinLlvmType(context_, typeKey))
               return builtin;
           if (LLVMTypeRef pointerLike = mapPointerLikeLlvmType(context_, nameLookup_, typeKey))
               return pointerLike;
           return mapLlvmType(context_, nameLookup_, mirModel_, typeKey, error);
       }
   ```
   Después: conservar los dos comentarios `NSC305-L03` y dejar solo `return mapLlvmType(context_, nameLookup_, mirModel_, typeKey, error);`. Añadir: `// PERF (NSC-PERF-05): ES: mapLlvmType ya prueba builtin y puntero; no se repite. EN: mapLlvmType already tries builtin and pointer; not repeated.`

   **Equivalencia:** `mapLlvmType` hace exactamente esas dos pruebas en el mismo orden (`LlvmTypeMapping.cpp:186-189`).
2. **R-05.2.** Memo por implementación (una por unidad de código, un solo hilo):
   - En `OopLlvmImplementation.h`:
     ```cpp
         // PERF (NSC-PERF-05): ES: cache typeKey -> LLVMTypeRef (solo resultados no nulos). Los tipos LLVM
         // literales son unicos por contexto, asi que devolver el mismo ref es identico a recalcularlo.
         // EN: typeKey -> LLVMTypeRef cache (non-null results only); literal LLVM types are uniqued per context.
         mutable std::unordered_map<std::string, LLVMTypeRef, frontend::TransparentStringHash, std::equal_to<>>
             llvmTypeCache_;
     ```
   - `llvmType`:
     ```cpp
         const auto cached = llvmTypeCache_.find(typeKey);
         if (cached != llvmTypeCache_.end()) return cached->second;
         LLVMTypeRef mapped = mapLlvmType(context_, nameLookup_, mirModel_, typeKey, error);
         if (mapped != nullptr) llvmTypeCache_.emplace(std::string(typeKey), mapped);
         return mapped;
     ```
   - **Equivalencia:** el mapeo es una función pura de (contexto, modelos inmutables, clave). Los errores no se cachean, así que el mensaje se genera igual.
   - **Comprobar** que ninguna rama de `mapLlvmType` crea un **struct con nombre** (`LLVMStructCreateNamed`), que no sería único por llamada. Hoy no lo hace: las ramas devuelven enteros, arreglos, vectores o el struct literal vacío.
3. **R-05.3.** Las funciones de `LlvmTypeClassification.cpp` y `LlvmTypeLayout.cpp` pasan al patrón B de R-02 (filas #40-#42 de la tabla) y `findStructLayout` al índice de R-04.3.


---

### R-06 · Genéricos: búsquedas lineales, asignaciones por nodo y recorridos repetidos · P0/P1

Archivos: `src/Frontend/ProjectGenericModel.cpp` y `src/Frontend/Generics/*`. Todo es HECHO salvo donde se indica.

#### R-06.1 · `resolveDeclaredTypeIdentity` recorre todas las identidades declaradas por cada peldaño y cada spelling

**HECHO.**

- `Generics/TypeSpellingAndVisibility.cpp:245-330`. Por cada `rung` de `qualifiedNameLadder` (290) y cada `spelling` (293), recorre **todo** `declaredTypeIdentities_` (295-318) comparando `declaration.sourceName != spelling`.
- `declaredTypeIdentities_` tiene un solo punto de escritura: `TemplateCollector.cpp:165`, dentro de `collectDeclaredTypeIdentity` (150-170). Contiene todos los tipos nombrados de todas las unidades, incluida la superficie de Core.
- Se llama por cada referencia de tipo no genérica, desde:
  - `processTypeSyntax`;
  - `recordDeclaredTypeIdentity` (`TypeDemandScheduler.cpp:497-509`);
  - `collectTypeDependency` (`TemplateDependencyCollector.cpp:213`).
- La variante con módulo, `resolveModuleQualifiedDeclaredTypeIdentity` (219-243), recorre todo **y además** construye `std::string(requestedModule) + "::"` **dentro** del bucle (236).

**Cambio exacto.**

1. `ProjectGenericModel.h`, junto a `declaredTypeIdentities_` (1576):
   ```cpp
       // PERF (NSC-PERF-06): ES: sourceName -> ordinales en declaredTypeIdentities_, en orden de insercion.
       // EN: sourceName -> ordinals into declaredTypeIdentities_, in insertion order.
       std::unordered_map<std::string, std::vector<std::size_t>, TransparentStringHash, std::equal_to<>>
           declaredTypeOrdinalsBySourceName_;
   ```
2. `TemplateCollector.cpp:165-169`, después del `push_back`:
   ```cpp
       declaredTypeOrdinalsBySourceName_[declaredTypeIdentities_.back().sourceName].push_back(
           declaredTypeIdentities_.size() - 1U);
   ```
3. `TypeSpellingAndVisibility.cpp:295-296`. Antes:
   ```cpp
               for (const DeclaredTypeIdentity& declaration : declaredTypeIdentities_)
               {
                   if (declaration.sourceName != spelling) continue;
   ```
   Después:
   ```cpp
               const auto bySpelling = declaredTypeOrdinalsBySourceName_.find(spelling);
               if (bySpelling == declaredTypeOrdinalsBySourceName_.end()) continue;
               for (const std::size_t declarationOrdinal : bySpelling->second)
               {
                   const DeclaredTypeIdentity& declaration = declaredTypeIdentities_[declarationOrdinal];
   ```
   El resto del cuerpo (298-317) queda igual.
4. `resolveModuleQualifiedDeclaredTypeIdentity`:
   - antes del bucle, calcular `const std::string modulePrefix = std::string(requestedModule) + "::";`;
   - recorrer solo `declaredTypeOrdinalsBySourceName_.find(requestedType)`;
   - usar `declaration.identityKey.starts_with(modulePrefix)`.

**Equivalencia.** Los mismos candidatos en el mismo orden de inserción. La deduplicación por `identityKey` (310-313) y la regla "1 encontrado" o "ambiguo" no cambian.

**Riesgo.** `collectDeclaredTypeIdentity` tendría que ejecutarse en un solo hilo. Hoy hace `push_back` sin lock sobre el vector, así que ya es secuencial (HECHO: no hay mutex alrededor).

#### R-06.2 · `GenericLookupScope::Cow` asigna memoria en su constructor por defecto

**HECHO.**

- `ProjectGenericModel.h:384-395`: `Cow() : value_(std::make_shared<T>()) {}`.
- Hay 6 contenedores `Cow` por scope (394-436): `importedNamespaces`, `aliases`, `typeAliases`, `typeParameters`, `valuePassKinds` y `genericLambdas`.
- Por **cada nodo** de **cada unidad**, incluida la superficie de Core, `processNode` hace `GenericLookupScope currentScope;` (`GenericNodeProcessor.cpp:534`). Eso son 6 `make_shared`, y cuatro de ellos construyen `unordered_map`/`unordered_set`.
- Después `prepareNodeScope` hace `outScope = scope;` (111) y libera lo recién asignado.
- El mismo patrón con `GenericLookupScope{}` aparece en `SpecializationExecutor.cpp:262`, `:413`, `:482` y `:729`, y en los recorridos de `TypeAliasExpander` y `TemplateDependencyCollector`.
- **HECHO:** nadie accede a `.value_` desde fuera del `Cow` (lo comprobé con grep).

**Cambio exacto.** En `ProjectGenericModel.h:384-395`, sustituir el cuerpo de `Cow` por esta versión con asignación perezosa. El comentario `NSC305-G04` existente se conserva y se añade el nuevo debajo:
```cpp
        // PERF (NSC-PERF-06): ES: asignacion perezosa; un scope por defecto no reserva nada y read() devuelve
        // un contenedor vacio estatico. EN: lazy allocation; a default scope allocates nothing and read()
        // returns a static empty container.
        template <typename T>
        struct Cow final
        {
            Cow() = default;
            const T& read() const
            {
                return value_ != nullptr ? *value_ : emptyValue();
            }
            T& write()
            {
                if (value_ == nullptr)
                    value_ = std::make_shared<T>();
                else if (value_.use_count() > 1)
                    value_ = std::make_shared<T>(*value_);
                return *value_;
            }
            static const T& emptyValue()
            {
                static const T empty;
                return empty;
            }
            std::shared_ptr<T> value_;
        };
```
**Equivalencia.** `read()` sobre un `Cow` sin escribir devuelve un contenedor vacío, como antes. `write()` reserva la primera vez. La copia de un `Cow` nulo sigue siendo nula y la semántica de copia en escritura no cambia. La inicialización del `static` local es segura entre hilos (C++11).

**Extra (mismo archivo, `GenericNodeProcessor.cpp:107-110`).** Envolver la llamada `telemetry_.noteScopeCopy(...)` en `if (telemetry_.enabled)`. Hoy los seis `size()` se evalúan aunque la telemetría esté apagada (`GenericTelemetry.cpp:31`).

#### R-06.3 · Se copia `children()` por nodo en los recorridos iterativos

**HECHO.** `AstNode::children()` devuelve `const std::vector<AstNode*>&`, pero cada recorrido hace una **copia** a un `std::vector` local por nodo para iterar en orden inverso:

- `ProjectGenericModel.cpp:247` (`indexOrdinaryCallableReturnTypes`);
- `Generics/TypeAliasExpander.cpp:384`;
- `Generics/GenericNodeProcessor.cpp:447`;
- `Generics/TemplateDependencyCollector.cpp:181` y `:370`;
- `Generics/TemplateCollector.cpp:284`.

**Cambio exacto.** En cada sitio, sustituir la copia `std::vector<...> children = node->children();` (o `(node->children().begin(), ...end())`) por `const std::vector<ast::AstNode*>& children = node->children();` y seguir iterando con `rbegin()/rend()`.

**Requisito antes de aplicar en cada sitio.** Comprobar que el cuerpo del bucle **no modifica** `node->children()` mientras se itera; si lo hiciera, la copia sería necesaria. Eso pasa, por ejemplo, en `applyExpandedAliasSyntax` (`TypeAliasExpander.cpp:203-206`), donde la copia **sí** es necesaria y se deja.

#### R-06.4 · `cloneAliasType(..., {}, ...)` de cada tipo de retorno, parámetro y campo, con sustitución vacía

**HECHO.** `GenericNodeProcessor.cpp` `prepareNodeScope`, líneas 154-181, clona en la arena el `TypeSyntax` del tipo de retorno de **cada** método y el de cada parámetro y campo, con mapa de sustitución vacío, en todas las unidades. Es una instantánea antes de que la pasada reescriba spellings genéricos.

**Cambio propuesto (INFERENCIA; requiere verificación).** Clonar solo si el tipo tiene `typeArguments()` no vacíos, si es función o tupla, o si tiene un argumento anidado. Para una hoja no genérica, la pasada nunca reescribe el nodo, así que la instantánea es idéntica al original.

**Antes de aplicar**, comprobar con grep que ningún código posterior **compara punteros** entre la instantánea y el nodo vivo. Si los compara, esta optimización **no** se aplica.

#### R-06.5 · Ocho recorridos completos del AST de todas las unidades

**HECHO.** `ProjectGenericModel::build` (558-610) ejecuta en secuencia:

1. alias-expansion;
2. trait-merge;
3. opaque-results;
4. ordinary-return-index;
5. associated-items;
6. template-collection;
7. dependency-discovery;
8. root-demand-discovery.

Cada uno recorre **todas** las unidades, incluidas `.public-api.ns` y `.public-templates.ns` de Core.

**Cambios propuestos (de menor a mayor riesgo).**

1. **Bandera por unidad**, calculada por el parser al crear el nodo: `hasUsingAliasDirectives`, `hasTraitImplementations`, `hasOpaqueResults`. Las pasadas 1, 2 y 3 se saltan las unidades sin esas construcciones. Si la bandera es falsa, la pasada es trabajo nulo, y eso es verificable leyendo cada pasada (`expandAliasesInNode`: si no hay ningún `UsingAliasDirective` no expande nada).
2. **Fusionar 4 y 5** (índice de retornos y elementos asociados) en un solo recorrido. Solo si no hay dependencia de orden entre ellas. NO VERIFICADO: revisar antes.
3. **Para Core:** la superficie pública precompilada no cambia entre builds. Las pasadas 1-5 sobre esas dos unidades producen siempre lo mismo. Cachear sus resultados junto al NSMETA (ver R-10.1) es el paso grande, pero es un cambio de diseño. Lo dejo como recomendación, sin edición exacta.

#### R-06.6 · `originNamespace()` devuelve `std::optional<std::string>` por valor y se llama 2 o 3 veces seguidas

**HECHO.**

- `include/nativesharp/AST/TypeSyntax.h:232`.
- Llamadas dobles o triples en:
  - `GenericCloner.cpp:192-194`, `:284-287`, `:999-1002`;
  - `TypeAliasExpander.cpp:146-147`, `:212-213`;
  - `TypeSpellingAndVisibility.cpp:210-211`, `:270-271`.

**Cambio exacto.** Cambiar la firma a `const std::optional<std::string>& originNamespace() const noexcept;`. El miembro ya existe en `TypeSyntax`; verificar el nombre del campo. Cada `x.originNamespace().has_value() ? x.originNamespace().value() : ...` deja de copiar.

**Lifetime.** Buscar con grep `auto&& ... = ...originNamespace()` y ligaduras a temporales. No encontré ninguna.

#### R-06.7 · `TemplateResolver::resolveTypeTemplate` construye strings antes de buscar y se llama dos veces

**HECHO.**

- `Generics/TemplateResolver.cpp:133`: construye `qualified` antes de buscar.
- `:139`: `std::string(terminalName)` para buscar en un `unordered_map`.
- `:151-152`: `candidate` por cada estado del bucket.
- `processTypeSyntax:573-579` lo llama **dos veces** cuando el resultado no es `Unique`, que es el caso normal de un tipo no genérico.

**Cambio exacto.**

1. Hash transparente en el mapa de plantillas por nombre terminal, para buscar con `string_view` sin crear el string de `:139`.
2. Precalcular `qualifiedName` en cada `TemplateState` al registrarlo y comparar contra ese campo en `:151-152`.
3. En `processTypeSyntax:573-579`, guardar el resultado de la primera llamada y reutilizarlo si la segunda tiene los mismos argumentos. Hay que leer las dos llamadas: si difieren en algún parámetro, solo aplican 1 y 2.

#### R-06.8 · `buildVisibility` con `std::function` recursiva y `weakly_canonical`

**HECHO.** `ProjectGenericModel` `buildVisibility` (398-428): `std::function` recursiva y `pathKey(weakly_canonical(...))` por referencia y por proyecto origen. Son llamadas al sistema `O(P × E)`. `ProjectNameLookup::buildVisibility` (`NameLookup/ProjectSetup.cpp:198-240`) tiene la misma forma, pero usa `pathKey` sin canonicalizar.

**Cambio exacto.** Calcular `pathKey(weakly_canonical(projectFile))` **una vez por proyecto**, antes del bucle, en un `std::vector<std::string>`. Sustituir la recursión por una pila explícita: un `std::vector<std::size_t>` de trabajo, con el mismo orden de visita (LIFO sobre las referencias en orden inverso). El conjunto `reachable` resultante no depende del orden.

---

### R-07 · Driver de build: E/S y hashing fijos en cada `nsc build` · P0

#### R-07.1 · `sha256File` lee byte a byte y `sha256` copia toda la entrada

**HECHO.** En `src/Basic/Sha256.cpp`:

- `sha256File` (124-145) llena un `std::vector` con `istreambuf_iterator` y un `push_back` por byte, sin `reserve` (131-136).
- `sha256` (112-121) llama `padSha256Input` (29-40), que **copia toda la entrada** a un vector rellenado.
- `sha256Words` (42-58) recibe ese vector.
- Lo usan en cada build:
  - restore (ProjectCommands 358 y 525);
  - procedencia (1036);
  - `LibraryMetadata::read` (154);
  - `authenticateDecodedMetadata` (Codec 757);
  - la caché de artefactos (CompilationArtifactCache 261, 286, 329, 366, 454, 519);
  - ObjectGenerator 713 y 717;
  - CodePartition 491;
  - FunctionAnalysis 72;
  - DebugInfoEmission 137;
  - MirCache 58;
  - TemplateCatalogCodec 40, 54, 777;
  - Comptime 330, 337, 452;
  - WindowsExecutableBuilder 109 y 146;
  - WindowsLibraryBuilder 112;
  - MirCacheIdentity 422.

**Cambio exacto.** Se conserva el estilo propio del archivo (2 espacios, llaves K&R, como está).

1. **Nueva función interna** junto a `sha256Words`:
   ```cpp
   // PERF (NSC-PERF-07): ES: palabras de un bloque de 64 bytes tomado directamente de memoria.
   // EN: message schedule for one 64-byte block read straight from memory.
   std::array<std::uint32_t, 64> sha256BlockWords(const std::uint8_t *block) {
     std::array<std::uint32_t, 64> words{};
     for (std::size_t index = 0; index < 16U; ++index) {
       const std::size_t offset = index * 4U;
       words[index] = (static_cast<std::uint32_t>(block[offset]) << 24U) |
                      (static_cast<std::uint32_t>(block[offset + 1U]) << 16U) |
                      (static_cast<std::uint32_t>(block[offset + 2U]) << 8U) |
                      static_cast<std::uint32_t>(block[offset + 3U]);
     }
     // (mismo bucle 16..63 que sha256Words:51-57, copiado literal)
     return words;
   }
   ```
   `sha256Words(padded, block)` puede quedarse e implementarse como `return sha256BlockWords(padded.data() + block);`. Así no se borra nada.
2. **`sha256` (112-121).** Antes:
   ```cpp
     const std::vector<std::uint8_t> padded = padSha256Input(input);
     for (std::size_t block = 0; block < padded.size(); block += 64U) {
       compressSha256Block(sha256Words(padded, block), state);
     }
   ```
   Después:
   ```cpp
     // PERF (NSC-PERF-07): ES: bloques completos directo de la entrada; solo la cola (1 o 2 bloques) se rellena.
     // EN: full blocks straight from the input; only the tail (1 or 2 blocks) is padded.
     const std::size_t fullBlocks = input.size() / 64U;
     for (std::size_t block = 0; block < fullBlocks; ++block) {
       compressSha256Block(sha256BlockWords(input.data() + block * 64U), state);
     }
     const std::size_t remainder = input.size() - fullBlocks * 64U;
     std::array<std::uint8_t, 128> tail{};
     std::copy(input.begin() + static_cast<std::ptrdiff_t>(fullBlocks * 64U), input.end(), tail.begin());
     tail[remainder] = 0x80U;
     const std::size_t tailSize = remainder + 9U <= 64U ? 64U : 128U;
     const std::uint64_t bitLength = static_cast<std::uint64_t>(input.size()) * 8ULL;
     for (unsigned byte = 0; byte < 8U; ++byte) {
       tail[tailSize - 1U - byte] = static_cast<std::uint8_t>((bitLength >> (byte * 8U)) & 0xffU);
     }
     for (std::size_t offset = 0; offset < tailSize; offset += 64U) {
       compressSha256Block(sha256BlockWords(tail.data() + offset), state);
     }
   ```
   **Equivalencia.** Para una entrada de `n` bytes, `padSha256Input` produce `((n + 9 + 63) / 64) * 64` bytes. Los primeros `64·⌊n/64⌋` bytes son la entrada intacta y la cola mide `((r + 9 + 63) / 64) * 64`, con `r = n mod 64`: 64 si `r ≤ 55` y 128 si no. El contenido de la cola es el mismo: resto, `0x80`, ceros y longitud big-endian.

   **Prueba nueva:** comparar con la versión antigua para longitudes 0..300, más 1 MiB aleatorio.
3. **`sha256File` (131-136).** Antes:
   ```cpp
     std::vector<std::uint8_t> bytes;
     for (std::istreambuf_iterator<char> iterator(stream), end; iterator != end; ++iterator) {
       bytes.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(*iterator)));
     }
   ```
   Después:
   ```cpp
     // PERF (NSC-PERF-07): ES: lectura por bloques de 64 KiB. EN: 64 KiB block reads.
     std::vector<std::uint8_t> bytes;
     std::array<char, 65536> chunk{};
     while (stream.read(chunk.data(), static_cast<std::streamsize>(chunk.size())) || stream.gcount() > 0) {
       bytes.insert(bytes.end(), reinterpret_cast<const std::uint8_t *>(chunk.data()),
                    reinterpret_cast<const std::uint8_t *>(chunk.data()) + stream.gcount());
       if (!stream) break;
     }
   ```
   `stream.bad()` (137) sigue igual. **Mejor todavía**, en un paso posterior: hash incremental con un contexto `Sha256Stream { update(span); finish(); }` sin acumular el archivo. Aquí basta con lo anterior.

#### R-07.2 · La procedencia de dependencias se calcula siempre aunque no se pida

**HECHO.**

- `Driver/ProjectCommands.cpp` `executeBuild`, líneas 1455-1456, siempre ejecuta:
  ```cpp
      std::vector<DependencyProvenance> provenance = dependencyProvenance(*graph, plan);
      updateRootDependencyProvenance(project, *loaded, provenance);
  ```
- `dependencyProvenance` (1046-…) llama `fingerprintFiles` (1027-1044). Esa función hace `sha256File` de cada archivo: el proyecto, **todas** las fuentes de todos los proyectos del grafo y `.lib`, `.nsmeta` y `.dll` de cada `LibraryReference`. Core.lib es grande. Además ordena con un comparador que construye `pathText` por comparación (1029).
- `updateRootDependencyProvenance` (1228-1243) vuelve a hashear las fuentes raíz.
- `provenance` **solo** se usa en:
  - `printDependencyProvenance`, si `options.dependencyReport && !json` (1457-1458);
  - el JSON de éxito, si `options.dependencyReport` (1404-1406).
- `BuildResult` (`Driver/Internal/BuildResult.h:8-14`) no la contiene.
- `fingerprintFiles` no tiene efectos laterales: ignora errores y escribe `"missing"`.

**Cambio exacto.** En 1455-1456. Antes:
```cpp
    std::vector<DependencyProvenance> provenance = dependencyProvenance(*graph, plan);
    updateRootDependencyProvenance(project, *loaded, provenance);
```
Después:
```cpp
    // PERF (NSC-PERF-07): ES: la procedencia hashea todas las entradas; solo se calcula si se reporta.
    // EN: provenance hashes every input; compute it only when it is reported.
    std::vector<DependencyProvenance> provenance;
    if (options.dependencyReport)
    {
        provenance = dependencyProvenance(*graph, plan);
        updateRootDependencyProvenance(project, *loaded, provenance);
    }
```
**Equivalencia.** Sin `dependencyReport` el vector no se lee (1405 y 1457 están condicionados). La salida es idéntica.

#### R-07.3 · Restore hashea cada artefacto dos veces y reescribe el lock siempre

**HECHO.**

- `appendArtifact`:
  - en la línea 343 hace `weakly_canonical(package.sourceRoot)` **por artefacto**, aunque es invariante por paquete;
  - en la 358 hace `sha256File` del artefacto fuente.
- `verifyPlan:525` hace `sha256File` del destino.
- `executeRestore:647` hace `atomicWrite` del lock aunque el contenido sea igual.
- `:649` hace `*completedPlan = plan;`, una copia donde cabe un `std::move` (verificar que `plan` no se usa después).

**Cambio exacto.**

1. Calcular `weakly_canonical(package.sourceRoot)` una vez por paquete, en el llamador, y pasarlo como parámetro.
2. En `verifyPlan`, si destino y fuente tienen el **mismo tamaño y la misma fecha de modificación** que se registraron al copiar, reutilizar el digest de la fuente.

   **Ojo:** esto cambia la garantía; hoy se detecta corrupción del destino. Si no es aceptable, dejarlo como está y aplicar solo 1, 3 y 4.
3. Antes del `atomicWrite` del lock (647), leer el lock existente. Si el contenido es byte a byte igual, no escribir.
4. Cambiar `*completedPlan = plan;` por `*completedPlan = std::move(plan);` si `plan` no se lee después de 649. Leer las líneas siguientes.

#### R-07.4 · El proyecto generado se carga tres veces

**HECHO.**

- `loadBuildClosure` (1198-1226) hace `loader.load(generated, ...)` en la 1205 y `ProjectGraphLoader(...).load(generated, ...)` en la 1212. El segundo vuelve a cargar el proyecto raíz.
- Después, el driver anidado (`configurePrecompiledModules`, en la línea 306 de `PrecompiledModuleConfiguration.cpp`, en su ruta de llamada) carga otra vez.
- Cada carga hace, por fuente, `ProjectSourceSet::add` (`Compilation/ProjectSourceSet.cpp:14-72`), que llama:
  - `weakly_canonical(projectDirectory_)` (23-24), invariante;
  - `is_directory`;
  - `weakly_canonical(source)`;
  - `is_regular_file`.
- `ProjectSourceSet::finalize` (74-…) ordena con un comparador que construye dos `comparisonKey` por comparación (89-98).
- `ProjectGraphLoader.cpp`:
  - `normalizeTransitiveSourceOwnership` (140-185) llama `canonicalPath` (`weakly_canonical`) **dos veces** por fuente (149 y 161);
  - `preferExactSourceModuleArtifacts` (188-…) lo llama dentro de bucles anidados y **dentro de los comparadores de `sort`** (215-226).

**Cambio exacto.**

1. `ProjectSourceSet::add`: cachear `canonicalProject` y el resultado de `is_directory` en un miembro `mutable std::optional<std::filesystem::path> canonicalProjectDirectory_`, calculado en la primera llamada. **Equivalente** mientras el directorio no cambie durante la carga.
2. `ProjectSourceSet::finalize`: precalcular `comparisonKey` y `generic_string` de cada fuente en un vector paralelo y ordenar índices. El orden resultante es el mismo.
3. `normalizeTransitiveSourceOwnership`: calcular `pathKey(canonicalPath(source.absolutePath))` una vez por fuente en el primer bucle, guardarlo en un `std::vector<std::vector<std::string>>` paralelo y reutilizarlo en el segundo.
4. `preferExactSourceModuleArtifacts`:
   - precalcular `pathKey(canonicalPath(project.projectFile()))` fuera del bucle de referencias;
   - en los `sort` de 221-226, precalcular la clave por elemento (patrón *decorate-sort-undecorate*) en lugar de llamar `canonicalPath` dentro de la proyección.
5. **Recomendación (diseño):** pasar el `ProjectGraph` ya cargado al driver anidado en lugar de recargar. NO VERIFICADO si la interfaz del driver anidado lo permite sin cambiar contratos; queda como propuesta.

#### R-07.5 · El mismo `.nsmeta` se lee, hashea, decodifica y verifica dos o más veces por proceso

**HECHO.**

- **Primera vez:** `PrecompiledModuleConfiguration.cpp` `loadExplicitPrecompiledMetadata:106` lee y verifica **antes** de consultar `loadedByModule` (118). Si N proyectos referencian el mismo módulo, se lee N veces.
- **Segunda vez:** `DriverArtifactGeneration.cpp` `prepareExecutableBuilder:175` llama `appendValidatedLibraryReferences`, que llama `validateLibraryReference` (`LibraryReferenceValidation.cpp:144-193`). Por **cada** referencia:
  - `CoffArchiveVerifier::verify(.lib)`;
  - `LibraryMetadataCodec::read` (otra vez, con su SHA-256 completo del payload en `LibraryMetadata.cpp:154`);
  - `authenticateDecodedMetadata` (re-encode y hash, Codec:757);
  - `verifyCurrentCompiler`;
  - `publishedNativeSymbols` (`ContractMaterialization.cpp:499-561`);
  - un `unordered_set` con todos los símbolos del archivo (61-62).
- **Además:** `normalizeDecodedMetadata` (Codec:770-775) hace `stable_sort` por string de tipos y miembros en cada lectura.

**Cambio exacto.**

1. En `loadExplicitPrecompiledMetadata`, mover la consulta `loadedByModule.find(...)` (118) **antes** de leer y verificar (106). Si ya está cargado, reutilizarlo. Hay que leer el bloque 100-125 para conservar el diagnóstico de "dos rutas distintas para el mismo módulo" si existe.
2. **Caché por proceso de metadatos verificados.** Clave: ruta canónica + tamaño + mtime + digest. Que `validateLibraryReference` reciba el `LibraryMetadata` ya verificado en la configuración en lugar de releerlo. Basta con pasar un `const std::unordered_map<std::string, const LibraryMetadata*>&` desde `configurePrecompiledModules` hasta `prepareExecutableBuilder`. Si el diseño no lo permite, como mínimo memoizar `LibraryMetadataCodec::read` por (ruta, tamaño, mtime) en un mapa estático del proceso protegido por mutex.
3. `publishedNativeSymbols` se calcula también al materializar. Guardar su resultado junto al metadata en la misma caché.

#### R-07.6 · La copia profunda de `binaryDependencies`

**HECHO.** `PrecompiledModuleConfiguration.cpp` `expandPrecompiledBinaryClosure:216-220` copia path, path, string y `vector<BinaryDependency>` por entrada. Es una copia defensiva porque `loaded` puede realojarse.

**Cambio exacto.** `loaded.reserve(...)` con una cota superior antes del bucle, o guardar índices en lugar de referencias. Es menor; aplicar si la medición lo justifica.

---

### R-08 · Enlace y verificación de binarios · P0

#### R-08.1 · El módulo runtime se construye y optimiza en cada build

**HECHO.**

- `Linker/WindowsExecutableBuilder.cpp` `buildObjects`: en las líneas 590-593 construye y optimiza el módulo runtime **antes** del chequeo de reuso incremental (596-601), y no tiene caché de `runtime.obj`.
- `WindowsLibraryBuilder.cpp:490-585` sí tiene una caché equivalente para bibliotecas.

**Cambio exacto.** Reutilizar el patrón de `WindowsLibraryBuilder.cpp:490-585`:

- **Clave:** `CodegenIdentity` + opciones de runtime (perfil de pánico, debug, optimización, características) + hash del texto IR del runtime, **o** mejor una versión fija del generador de runtime.
- Si existe `runtime.obj` en la caché de artefactos (`activeCompilationArtifactCache()`), restaurarlo y saltar construcción y optimización.
- Leer las líneas 490-585 y replicar exactamente sus validaciones (tamaño y digest).

#### R-08.2 · Cuatro o más procesos NSLink por build para las bibliotecas de importación

**HECHO.**

- `Linker/WindowsRuntimeImports.cpp` `createWindowsImportLibraries` (296-324) llama `materializeWindowsImportLibraries` (276-291), que llama `LldLinker::createImportLibrary` (`LldLinker.cpp:557-640`) **por DLL**.
- Cada llamada escribe `.def` y `.rsp` y **lanza NSLink** enlazando una DLL dummy (`/dll /noentry /out:dummy.dll`) para obtener el `.lib`.
- El cierre canónico KERNEL32 + KERNELBASE + SHELL32 + ucrtbase (229-260) suma al menos 4 procesos por build.

**Cambio exacto (de menor a mayor).**

1. **Caché por contenido.** Clave: (nombre de DLL, lista **ordenada** de símbolos, identidad de NSLink, arquitectura). Si existe el `.lib` en la caché de artefactos, se copia y no se lanza NSLink. Para hola mundo los símbolos son los mismos en cada build, así que en régimen estable el costo es 0 procesos.
2. **Una sola invocación.** Si NSLink acepta `/lib /def:` (modo librarian, sin enlazar DLL), usar ese modo en lugar de enlazar una dummy. NO VERIFICADO que NSLink lo soporte; probar con un `.def` pequeño.
3. **Escribir el import library corto en proceso.** El formato *short import* COFF es simple. Es la opción definitiva, pero la de mayor esfuerzo.

#### R-08.3 · `readPeBytes` lee byte a byte, tres veces por build

**HECHO.**

- `Linker/PeExecutableVerifier.cpp:110-123`: `while (stream.get(character)) bytes.push_back(...)` sin `reserve`.
- Lo usan:
  - el `.exe` final (`linkAndVerifyExecutable:406-407`);
  - cada DLL de runtime, **dos veces** por build (`LibraryReferenceValidation.cpp:101-102` y `verifyLinkedRuntimeLibraries:412-413`).

**Cambio exacto.** En 121-122. Antes:
```cpp
    char character = 0;
    while (stream.get(character)) bytes.push_back(static_cast<unsigned char>(character));
```
Después:
```cpp
    // PERF (NSC-PERF-08): ES: lectura en bloque del tamaño del archivo. EN: single sized read.
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    stream.seekg(0, std::ios::beg);
    if (length > 0)
    {
        bytes.resize(static_cast<std::size_t>(length));
        stream.read(reinterpret_cast<char*>(bytes.data()), length);
        bytes.resize(static_cast<std::size_t>(stream.gcount()));
    }
```
Las comprobaciones posteriores (`bytes.size() < 64`, etc.) no cambian. Además, memoizar por ruta + mtime las DLL de runtime verificadas en el proceso, para no verificarlas dos veces (101-102 y 412-413).

#### R-08.4 · Copia del texto IR del runtime para la clave incremental

**HECHO.** `prepareIncrementalExecutable:527` llama `runtimeModule.text()`; `incrementalLinkKey:140-143` hashea objetos y bibliotecas. Solo ocurre con `--incremental-link`, que por defecto es `false` (`CompilerInvocation.h:46`). **No** afecta al build normal. Se deja como está.

#### R-08.5 · Destrucción completa de `ProjectCompilation` al terminar

**HECHO.** `DriverArtifactGeneration.cpp:278`: `frontend::ProjectCompilation compilation;` es local, así que al salir se destruyen el AST y todos los modelos. Son millones de liberaciones; magnitud NO VERIFICADA.

**Cambio propuesto.** Medir primero con un temporizador alrededor del cierre del ámbito. Si es significativo, en la ruta CLI (no en el IDE ni en las pruebas) mover la compilación a un `std::unique_ptr` y liberarlo con `release()` al final del proceso. Es una opción de diseño: requiere decisión explícita y solo aplica al proceso CLI de vida corta.

---

### R-09 · Instrumentación siempre activa · P1 (lote 1)

**HECHO.**

- `LLVM/Internal/ColdLlvmTrace.cpp:22-23`: el constructor abre y **trunca** `A:\temp\nsc-llvm-code-unit-timing.log` en cada build y escribe con `flush` por unidad.
- `LLVM/Internal/OopLlvmImplementation.cpp:166-189` (`tracePhase`): abre y cierra `A:\temp\nsc-llvm-lowering-phases.log` en **cada fase de cada módulo**, bajo un mutex global. Su comentario dice "Remove only after the remaining coordinator cost has a measured owner".
- `LLVM/Lowering/OopLlvmRuntimeDebugTrace.cpp:31-39`: con `debug=full` declara la función de traza y fija `flags = 7`. Eso **emite IR adicional** en el programa generado: entrada de función, alloc y free, y stores de campo. Cuesta en compilación y en ejecución.
- **Precedente en el propio código:** `Frontend/Bodies/MethodBodyInternals.h:14` tiene `inline constexpr bool kTraceEnabled = false;` y `MethodBodyInternals.cpp:16` y `:26` hacen `if constexpr (!kTraceEnabled) return;`.

**Cambio exacto.** Sigue el mismo patrón y no borra nada.

1. **Archivo nuevo** `src/LLVM/Internal/LlvmInstrumentationSwitches.h`:
   ```cpp
   #pragma once
   namespace nativesharp::llvm_backend
   {
   // INSTRUMENTATION CODE switches (NSC-PERF-09). ES: apagadas por defecto; encender solo para medir.
   // EN: off by default; turn on only to measure.
   inline constexpr bool kColdLlvmTraceEnabled = false;
   inline constexpr bool kLoweringPhaseTraceEnabled = false;
   inline constexpr bool kRuntimeDebugTraceEnabled = false;
   }
   ```
2. **`ColdLlvmTrace`.** En el constructor y en cada método que escribe, poner `if constexpr (!kColdLlvmTraceEnabled) return;` al principio. El `std::ofstream output_` se construye en la lista de inicialización (22-23), así que:
   - cambiar el miembro a `std::optional<std::ofstream>`;
   - abrirlo dentro del cuerpo solo si la constante es `true`.
3. **`tracePhase` (166-189).** Si `!kLoweringPhaseTraceEnabled`, hacer `return callback();` sin cronómetro ni archivo.
4. **`OopLlvmRuntimeDebugTrace.cpp:13`** (`ensureRuntimeDebugTrace`). Antes:
   ```cpp
           if (debugInformation_ != DebugInformationKind::Full || runtimeDebugTraceFunction_ != nullptr)
   ```
   Después:
   ```cpp
           if (!kRuntimeDebugTraceEnabled ||
               debugInformation_ != DebugInformationKind::Full || runtimeDebugTraceFunction_ != nullptr)
   ```
   Con eso la función no se declara. Los emisores ya manejan `runtimeDebugTraceFunction_ == nullptr`, porque es exactamente el estado que tienen con `debug != full` (HECHO: la misma rama devuelve `true` sin crear nada).

   **Verificar** que las pruebas no dependen de la presencia de `__nsc_runtime_debug_trace` en el IR con `debug=full`. Si alguna lo hace, esa prueba documenta instrumentación temporal: decidir con Alberto.


---

### R-10 · Superficie pública de Core y verificación de NSMETA · P1

**Contexto (HECHO).** `PrecompiledModuleConfiguration.cpp` `materializePrecompiledModules:73` y `:79` regeneran **en cada build** el texto `public-api` y `public-templates` de cada módulo precompilado. Para eso usan `PublicSurfaceEmitter.cpp:34`, que llama a `PublicSurfaceBuilder`. Ese texto se lexea, se parsea y pasa por genéricos (R-06.5).

#### R-10.1 · Cachear el texto generado (diseño, mayor impacto)

**Propuesta.** El texto depende solo de:

- el NSMETA, ya identificado por su digest en `LibraryMetadata::read:154`;
- la versión del generador.

Se puede persistir junto a la caché de artefactos con la clave `(digest del payload NSMETA, CodegenIdentity, versión del emisor)` y saltar `PublicSurfaceBuilder` por completo. **Equivalencia:** el emisor es determinista sobre el mismo metadata. Hay que verificarlo con una prueba que emita dos veces y compare. Si se aplica, R-10.2 y R-10.3 solo afectan al primer build.

#### R-10.2 · `collectPublishedMembers`: `O(M × T)` con dos asignaciones por comparación

**HECHO.**

- `Driver/PublicSurfaceBuilder.cpp:1290-1310`. Por **cada** miembro hace un `find_if` lineal sobre **todos** los tipos (1298-1299) con el predicado `"type:" + candidate.identityKey == member.declaringTypeKey`. Eso construye un string por cada comparación.
- `metadata.types` queda ordenado de forma estable por `identityKey` en `normalizeDecodedMetadata` (`Interop/LibraryMetadataCodec.cpp:770-772`), que también fija `payloadDecodedByCurrentReader = true` (775).

**Cambio exacto.** En 1298-1299. Antes:
```cpp
        const auto type = std::find_if(metadata.types.begin(), metadata.types.end(), [&](const auto &candidate)
                                       { return "type:" + candidate.identityKey == member.declaringTypeKey; });
```
Después: una función miembro nueva `findMetadataTypeByTypeKey(std::string_view typeKey)` que devuelve el iterador.

- Si `!typeKey.starts_with("type:")`, devuelve `end()`. Ningún `identityKey` produce `"type:" + x` igual a algo sin ese prefijo.
- Si `metadata.payloadDecodedByCurrentReader`, usa `std::lower_bound` por `identityKey` contra `typeKey.substr(5)`. Devuelve el primero de los iguales, igual que `find_if` sobre una secuencia estable.
- Si no, ejecuta el `find_if` actual, pero comparando `candidate.identityKey == typeKey.substr(5)` sin construir el string.

El llamador queda `const auto type = findMetadataTypeByTypeKey(member.declaringTypeKey);`.

**Lo mismo en:**

- `publishContractTypes` (1343-1350), que compara `identityKey == identity`;
- `orderedPublishedMethods` (1144-1162): el `find_if` sobre `dispatchTables` por tipo (1149-1151) pasa a un `unordered_map<std::string_view, const LibraryMetadataDispatch*>` construido **una vez** en el constructor del builder.

#### R-10.3 · `orderedPublishedMethods` llama a `dispatchRank` dos veces por comparación

**HECHO.**

- El comparador del `stable_sort` (1153-1160) calcula `dispatchRank(left)` y `dispatchRank(right)` en cada comparación.
- `dispatchRank` (1204-1234) hace búsquedas lineales en las firmas del dispatch. Para interfaces recorre **todas** las `dispatchTables`, sus interfaces y sus firmas (1223-1229).

**Cambio exacto.**

1. Antes del `stable_sort`, calcular `std::vector<std::pair<RankType, std::size_t>> ranks` una vez por método.
2. Ordenar índices con `stable_sort` comparando `ranks[i].first`.
3. Reconstruir `orderedMethods`.

El orden resultante es idéntico porque `stable_sort` con la misma clave produce la misma permutación. Para interfaces, construir una vez un mapa `interfaceTypeKey → firmas`.

#### R-10.4 · `ContractMaterialization` sin memo

**HECHO.**

- `canMaterializeContractType(metadata, typeKey)` (`PrecompiledSurface/ContractMaterialization.cpp:154-158`) crea un `unordered_set` nuevo por llamada y recorre recursivamente structs y delegates.
- Se llama por cada parámetro y retorno de cada miembro, desde:
  - `canMaterializePrecompiledDeclaration:296-301`;
  - `canMaterializeNativeMethod:180-187`;
  - `canMaterializeConstructor:218-231`.
- `isPubliclyReachableMetadataType:31-36` construye `"type:" + module + "::" + parent` por nivel.
- `canMaterializeConstructor:221-222` hace un `find_if` lineal sobre `dispatchTables`.
- `publishedNativeSymbols:499-561` repite todo lo anterior para todos los miembros.

**Cambio exacto.** Crear `class ContractMaterializationCache final` (archivo propio) con:

- `std::unordered_map<std::string, bool> contractTypeResult`;
- `std::unordered_map<const LibraryMetadataType*, bool> publiclyReachable`.

Añadir sobrecargas que reciban `ContractMaterializationCache&`. Las versiones actuales sin caché quedan como envoltorios que crean una caché local, así que la API no cambia. `PublicSurfaceBuilder` y `publishedNativeSymbols` usan una sola caché por metadata.

**Equivalencia.** Las funciones son puras sobre `metadata`, que no cambia. La recursión con `visiting` solo detecta ciclos. Memoizar **solo resultados completos**: no guardar en la caché un resultado obtenido mientras un ancestro estaba en `visiting`, porque en un ciclo un resultado parcial podría diferir. Regla concreta: guardar solo cuando la llamada **raíz** (con `visiting` vacío al entrar) termina.

#### R-10.5 · Verificación de plantillas genéricas: DFS sin memo global

**HECHO.**

- `Interop/LibraryMetadataVerification.cpp` `verifyGenericTemplates` (890-907) llama a `verifyGenericTemplateClosure` (839-869) desde cada raíz pública.
- Es un DFS con `visiting.erase(...)` al salir (868). Un nodo compartido se re-explora por cada camino, lo que es exponencial en un DAG con diamantes.

**Cambio exacto.**

- Añadir un `std::unordered_set<std::string> verified` compartido entre raíces.
- Al terminar con éxito un nodo, `verified.insert(key)`.
- Al entrar, `if (verified.contains(key)) return true;`.

**Equivalencia.** La verificación de un nodo no depende del camino. Hay que comprobarlo leyendo 839-869: si algún diagnóstico incluye la raíz en el mensaje, el primer error se reporta igual porque se sigue el mismo orden de visita.

#### R-10.6 · Verificación de dispatch virtual lineal

**HECHO.**

- `verifyVirtualMemberDispatch:644-645` llama a `findMetadataDispatch` (lineal, 599-601) por cada miembro virtual.
- `findMetadataDispatchSlot` (609-618) es lineal sobre los slots.

**Cambio exacto.** En `verifyMetadataDispatchContracts` (694-702), que ya recorre `dispatchTables`, construir `typeKey → dispatch*` y `(dispatch*, signature) → slot`, y pasarlos a `verifyVirtualMemberDispatch`.

#### R-10.7 · Strings de diagnóstico construidos aunque no haya error

**HECHO.**

- `verifyParameterPackContracts:1492-1505` y `verifyLifetimeMetadataContracts:1560` y `:1574-1584` construyen `owner` por miembro aunque no haya error.
- `:1530` crea un `unordered_set<string>` por callable.

**Cambio exacto.** Construir `owner` solo dentro de la rama de error. Es el mismo texto, calculado en ese punto.

---

### R-11 · Lexer y parser · P1

#### R-11.1 · `keywordKind` hace hasta tres búsquedas hash por identificador

**HECHO.**

- `Lexer/Lexer.cpp:180-201`: `keywordKindPart1`, `Part2` y `Part3` son tres `static const std::unordered_map<std::string_view, TokenKind>` (55-177).
- Un identificador que no es palabra clave, que es el caso mayoritario, hace las **tres** búsquedas y calcula el hash tres veces.

**Cambio exacto.**

1. Nueva función `const std::unordered_map<std::string_view, TokenKind>& keywordTable()`. Construye un `static const` mapa único a partir de las mismas tres listas de inicialización, que se **mueven** a esta función sin cambiar su contenido. Inserta con `try_emplace` en el orden Part1, Part2, Part3, de modo que, si una clave estuviera repetida, gana la primera, igual que hoy.
2. `keywordKind` queda:
   ```cpp
   const auto found = keywordTable().find(text);
   return found == keywordTable().end() ? TokenKind::Identifier : found->second;
   ```
3. **Opcional, exacto:** un prefiltro por longitud y primer carácter. Al construir la tabla, calcular `maxKeywordLength` y un `std::array<bool, 256> keywordInitial`. Si `text.size() > maxKeywordLength || !keywordInitial[text[0]]`, devolver `Identifier` sin calcular el hash.

#### R-11.2 · `lexAll` sin `reserve`

**HECHO.** `Lexer::lexAll` (219-235). Añadir `tokens.reserve(buffer_.size() / 4U + 16U);` al principio. Es una heurística: no cambia el resultado y evita realocaciones al lexear la superficie de Core, que son MB.

#### R-11.3 · El parser copia todos los nombres de tipo conocidos por unidad y asigna un string por consulta

**HECHO.**

- `Parser/ParserContext.cpp:19-60`: el constructor inserta los 19 nombres builtin y **todos** los `knownTypeNames`, que con Core son miles, en un `std::unordered_set<std::string>` **nuevo por unidad**, sin `reserve`.
- `TypeParser.cpp:57`: `knownTypeNames_.contains(std::string(token.lexeme()))`. Asigna un string por consulta, y se ejecuta en el *lookahead* de cada identificador.
- `TypeParser.cpp:369-370` hace `contains(candidate)` con un string ya construido.
- `knownTypeNames_` está declarado en `include/nativesharp/Parser/Parser.h:1660` y lo referencia `KnownGenericTypeScope.h:29` (`std::unordered_set<std::string>&`). **Lo usan las pruebas** (guía §1.3.4).

**Cambio exacto (sin tocar el tipo del miembro).**

1. En `Parser::Parser` (49-57), antes del primer `insert`:
   ```cpp
   knownTypeNames_.reserve(builtinNames.size() + knownTypeNames.size());
   ```
2. En `Parser.h`, añadir el miembro `mutable std::string typeNameLookupScratch_;`.
3. En `TypeParser.cpp:57`. Antes:
   ```cpp
   return token.kind() == TokenKind::Identifier && knownTypeNames_.contains(std::string(token.lexeme()));
   ```
   Después:
   ```cpp
   if (token.kind() != TokenKind::Identifier) return false;
   // PERF (NSC-PERF-11): ES: reutiliza el buffer; sin asignacion por consulta tras el primer uso.
   // EN: reuses the buffer; no allocation per query after first use.
   typeNameLookupScratch_.assign(token.lexeme());
   return knownTypeNames_.contains(typeNameLookupScratch_);
   ```
   Si la función es `const`, el miembro `mutable` lo permite. Si el parser se usa en paralelo, cada unidad tiene su propio `Parser`, así que no hay carrera.

**Alternativa de mayor impacto (requiere revisar las pruebas).** Hacer que `knownTypeNames_` sea `std::unordered_set<std::string, TransparentStringHash, std::equal_to<>>`. Eso permite `contains(string_view)` sin buffer. Rompe cualquier prueba que **nombre el tipo exacto** `std::unordered_set<std::string>`, así que hay que hacer grep en `tests/` antes.

**Alternativa estructural.** Un conjunto base compartido por todas las unidades, más un conjunto local por unidad para los nombres que se insertan al parsear (`MemberParser.cpp:1137`, `KnownGenericTypeScope`). Cambia lo que contiene `knownTypeNames_`, y las pruebas lo observan. Requiere una decisión explícita.

#### R-11.4 · Construcción de la lista de nombres conocidos

**HECHO.** `ProjectCompilation.cpp` `collectParserKnownTypeNames` (484-500):

- doble bucle `alias × nombres conocidos`;
- `prefix = target + "."` construido **dentro** del bucle interno (493);
- comparación `known == target` lineal;
- devuelve un `vector<string>` copiado de un `std::set` (500).

**Cambio exacto.**

- Sacar `prefix` fuera del bucle interno.
- Sustituir el recorrido por `names.find(target)` y `for (auto it = names.lower_bound(prefix); it != names.end() && it->starts_with(prefix); ++it)`. Es el mismo conjunto de coincidencias, en el mismo orden ordenado del `std::set`.
- Para el retorno, construir el vector con `reserve(names.size())` y `std::make_move_iterator` si el `set` es local y no se usa después.

#### R-11.5 · La ruta crítica de lex y parse es la unidad más grande

**HECHO.** Lex y parse se paralelizan **por unidad**. La superficie de Core son 1 o 2 unidades grandes (`.public-api.ns` y `.public-templates.ns`).

**INFERENCIA.** El tiempo de pared de esta fase es el de la unidad de Core.

**Propuesta (diseño).** Que `PublicSurfaceBuilder` emita un archivo por namespace de primer nivel. Hay que revisar las dependencias de ordinal de unidad (0 y 1) en `ProjectCompilation.cpp:1186` y `:1230` antes de partirla.

---

### R-12 · AST: tamaño de nodo y una asignación por nodo · P2 (estructural)

**HECHO.**

- `include/nativesharp/AST/AstNode.h:222-243`: cada `AstNode` tiene 5 `std::string` y 9 `std::vector`, unos 450 B. La mayoría de los nodos usan 0 o 1 de esos campos: `auxiliaryText`, `argumentName`, `allocatorFailureProfile`, `lifetimeName`, `genericParameterSyntaxes`, `lifetimeParameters`, `lifetimeOutlivesConstraints` y `genericConstraints` casi siempre están vacíos.
- `AstArena.h:23-28`: `make()` hace `make_unique` por nodo y un `push_back` de `Entry`. No es una arena real.

**Propuesta.**

1. **Arena por bloques** de 64 KiB con *placement new*. Se conserva `Entry` con el destructor para llamar `~T()` en orden inverso. **Equivalente:** mismos objetos, mismas direcciones estables y destrucción en orden inverso, igual que hoy con el vector de `unique_ptr` destruido al revés. Hay que verificar el orden actual de destrucción de `entries_`.
2. **Mover los campos raros** a un `std::unique_ptr<AstNodeExtras>` creado bajo demanda, manteniendo los accesores con la misma firma. Afecta a pruebas que usan `#define private public` sobre `AstNode`: grep en `tests/` antes. Es el cambio más invasivo del documento; queda para el final.

---

### R-13 · Bodies: escaneos completos de miembros por consulta · P1

Para recorridos "todos los miembros cuyo `declaringTypeOrdinal == t`", el índice existente `ProjectMemberModel::membersOfType` **no sirve tal cual**:

- **HECHO:** los miembros promovidos van a `promotedMembersByType_` (`AnonymousMemberPromotion.cpp:123-124`), no a `membersByType_`.
- El comentario de `ConstructionAnalyzer.cpp:228-230` advierte que los destructores no participan del lookup por nombre. Se refiere a `findMembers`, el índice por nombre.

Para equivalencia exacta hace falta un índice **autoritativo** nuevo.

**Infraestructura (una vez).** En `ProjectMemberModel`:

```cpp
    // PERF (NSC-PERF-13): ES: TODOS los miembros cuyo declaringTypeOrdinal es el tipo, en orden de ordinal
    // (incluye destructores y promovidos). EN: ALL members whose declaringTypeOrdinal is the type, in
    // ordinal order (destructors and promoted aliases included).
    std::span<const std::size_t> membersDeclaredByType(std::size_t typeOrdinal) const noexcept;
private:
    std::vector<std::vector<std::size_t>> membersDeclaredByType_;
```

- Se construye al final de `ProjectMemberModel::build`, con un solo recorrido de `members_` en orden.
- **HECHO:** `members_` solo crece en `MemberCollection.cpp:178`, `EnumMemberCollection.cpp:307` y `AnonymousMemberPromotion.cpp:123`, todos dentro de `build`.
- Si el índice está vacío (fixtures), el accesor devuelve vacío. Por eso cada llamador conserva el recorrido original como fallback cuando `membersDeclaredByType_.empty() && !members_.empty()`. O mejor: el propio accesor recibe un `std::vector<std::size_t>& scratch` y lo rellena linealmente en ese caso.

**Sitios.** Patrón: `for (m : members()) if (m.declaringTypeOrdinal != t) continue; ...` pasa a `for (o : membersDeclaredByType(t)) { const auto& m = members()[o]; ... }`.

| Archivo:línea | Función | Frecuencia |
|---|---|---|
| `Bodies/TypeKeyPredicates.cpp:113` | `elementTypeHasTrivialLifetime` | 10 llamadores |
| `Bodies/CallAnalyzer.cpp:4921` y `:4942` | `callTypeIsSend` y `callTypeIsSync` (recursivas) | Por llamada de concurrencia; la recursión baja por cada campo |
| `Bodies/ConstructionAnalyzer.cpp:232` | `aggregateSupportsSynthesizedMemberVisit` | Por `derive` |
| `Bodies/CallAnalyzer.cpp:2232` | `bindAtomicOperation` (campo `value` del mismo tipo) | Por operación atómica |
| `Bodies/CallAnalyzer.cpp:2444` | `recognizeCoreMutexMonitorOperations` | Por operación de monitor |
| `Bodies/MethodBodyCallableLifecycle.cpp:384` | `verifyRequiredFieldAssignments` | **Bucle anidado `O(C × M)`**. El externo (376) recorre todos los miembros buscando ctors y el interno (384) todos otra vez. Sustituir **solo el interno** por `membersDeclaredByType(member.declaringTypeOrdinal)`. El externo **se conserva**: cambiarlo a un recorrido por tipo alteraría el orden de los diagnósticos |
| `HIR/HirBuildCoordinator.cpp:1364` | `promoteEligibleStackClassLocals` | Por local candidato |
| `HIR/ExpressionStatementMachine/HirCoreCallFrameLowering.cpp:604` | `initializeEnumToStringCallFrame` | Por `ToString` de enum |
| `LLVM/Lowering/OopLlvmDebugInfo.cpp:865` | `debugEnumType` | Por tipo enum en debug, `O(E × M)` |
| `Frontend/ProjectGenericConstraintModel.cpp:867` | `satisfiesThreadSafetyConstraint` | Por restricción |

**Otros de Bodies:**

- **R-13.1.** `memberCandidatesForType` (`AssignmentAnalyzer.cpp:2522-2600`): el bloque final recorre **todos** los `interfaceModel_->tablePlans()` (2566-2585) para quedarse con los de `plan.classTypeOrdinal == typeOrdinal`. Añadir en `ProjectInterfaceModel` un índice `classTypeOrdinal → [plan ordinals]`, construido donde se llenan los `tablePlans`. **Además:** `std::ranges::find(defaults, *implementation)` (2580) es lineal por elemento. `defaults` suele ser pequeño; se puede dejar.
- **R-13.2.** `memberCandidatesForType` (2551-2552): `std::vector<std::size_t> candidates(found.begin(), found.end())` copia el span solo para comprobar si está vacío y devolverlo. Es correcto porque la firma devuelve un vector. Se deja.
- **R-13.3.** `recordEventAssignmentBinding` (`AssignmentAnalyzer.cpp:790`): busca el accesor recorriendo todos los miembros. Usar `memberModel_->findMembers(declaringType, accessorName)` **solo si** el accesor está en ese tipo; leer el contexto, porque si el filtro no incluye el tipo, no se aplica.
- **R-13.4.** Los tipos de Core reconocidos por FQN en cada llamada:
  - `CallAnalyzer.cpp:1855` (`ThreadStart`) y `:3073` (`StringView`);
  - `MemberAccessAnalyzer.cpp:412`;
  - `StatementAnalyzer.cpp:2112` (`Exception`) y `:2188` (`IDisposable`).

  Con R-01 cada candidato cuesta una comparación de `const&`, pero sigue siendo `O(T)` por consulta. Se puede resolver una vez:
  - en el coordinador (`ProjectMethodBodyModel::build`), calcular un `struct CoreTypeOrdinals { std::optional<std::size_t> exception, disposable, threadStart, stringView; }`;
  - copiarlo a cada shard en `createCallableShard` (`MethodBodyCallableLifecycle.cpp:250-270`);
  - que las funciones devuelvan el valor precalculado.

  **Equivalente:** `namedTypes()` no cambia durante el análisis de cuerpos.
- **R-13.5.** `exceptionRootTypeOrdinal` se consulta por cada `throw`/`catch` (`StatementAnalyzer.cpp:2130`, `:2386`, `:2676`) y en `MethodBodyBuildCoordinator.cpp:104`. Se resuelve con lo mismo que R-13.4.
- **R-13.6.** `resolveTypeIdentifier` (`AssignmentAnalyzer.cpp:2634-2660`) recorre todos los tipos comparando `identity.name()`. Un índice `name → [ordinales]` en `ProjectNameLookup`, construido junto al de R-02, lo deja en `O(k)`. El cuerpo filtra por `kind` y prefiere el contexto; se conserva igual iterando solo los candidatos con ese nombre, en orden ascendente.
- **R-13.7.** Los shards por callable (`createCallableShard`, `MethodBodyCallableLifecycle.cpp:250-270`) construyen un `ProjectMethodBodyModel` completo, con **47** contenedores hash miembro, por callable. INFERENCIA: con MSVC eso son unas 94 asignaciones por shard antes de analizar nada. Si la medición lo señala, cambiar los mapas de uso raro a `std::optional<std::unordered_map<...>>` creado bajo demanda. Toca miembros que usan las pruebas, así que hay que revisarlas primero. Prioridad baja.

- **R-13.8.** `stage38OperatorCandidates` (`MemberAccessAnalyzer.cpp:1208-1270`) recorre por **cada uso de operador** **todas** las funciones de namespace (1258-1268) buscando extensiones con ese nombre.
  - **Cambio exacto:** índice `name → [ordinales de funciones con isExtension]` en `ProjectNamespaceFunctionModel`, construido al terminar su `build`, en orden ascendente.
  - El bucle pasa a iterar solo esa lista y conserva el filtro `seen`.
- **R-13.9.** `resolveOptionalCarrier` (`CarrierResolver.cpp:153-224`) hace, por cada `Optional<T>`:
  - un `find_if` sobre todas las plantillas (168-171), que se sustituye por un índice `canonicalKey → template` en `ProjectGenericModel`;
  - un recorrido de todos los miembros (189-…), que se sustituye por `membersDeclaredByType`.

---

### R-14 · Enlace de identidades precompiladas · P1

#### R-14.1 · `bindPrecompiledPhysicalNames`: canonicalización `O(Mp × A)`

**HECHO.** `MemberModel/BuildSupport.cpp:316-390`:

- Por **cada** miembro precompilado (método o ctor) llama dos veces a `canonicalize(...)`: `declaringTypeKey` en 366-367 y `signatureKey` en 368.
- `canonicalize` (337-349) recibe el string **por valor** y recorre **todos** los alias haciendo `value.find(localTypeKey)` en bucle.
- `"type:" + ...identity.key()` se construye por miembro.

**Cambio exacto.**

1. Filtrar `aliases` **una vez**, después del `sort` de 333-335: eliminar los de `localTypeKey.empty()` o `localTypeKey == publishedTypeKey`. Hoy esos alias hacen `continue` en cada llamada. El orden relativo de los restantes no cambia.
2. Memoizar `declaringTypeKey` por `member.declaringTypeOrdinal` en un `std::vector<std::optional<std::string>>` de tamaño `namedTypes().size()`. Todos los miembros de un tipo comparten la misma clave canonicalizada.
3. Si `aliases` queda vacío después del filtro, saltar `canonicalize` y usar el valor directo.

**Equivalencia.** Mismos reemplazos, en el mismo orden y sobre las mismas cadenas.

#### R-14.2 · `bindPrecompiledEscapeSummaries`: `O(F × I)` con cuatro comparaciones de string

**HECHO.** `ProjectNamespaceFunctionModel.cpp:1593-1625`. Por cada función precompilada hace un `std::ranges::find_if` lineal sobre todas las `identities`, comparando `moduleName`, `namespaceName`, `name` y `signatureKey`.

**Cambio exacto.** El mismo patrón que ya usa `PrecompiledMemberIdentityIndex` (`Frontend/PrecompiledMemberIdentityIndex.cpp`, hash combinado más `equal_range` y verificación de los cuatro campos):

- crear `PrecompiledNamespaceFunctionIdentityIndex`, una clase en su propio archivo;
- recordar que hoy `find_if` devuelve el **primero**: el índice debe devolver el de menor ordinal entre los iguales, recorriendo `equal_range` y quedándose con el mínimo.

---

### R-15 · Varios (P2)

1. **Checksum de debug info por unidad y por archivo.**
   - **HECHO:** `LLVM/Lowering/Support/DebugInfoEmission.cpp:111-149` (`createDebugFile`) calcula `sha256` del buffer completo de cada archivo fuente la primera vez que **cada unidad de código** lo referencia. El mapa `debugFiles` es por unidad.
   - Con N unidades y la superficie de Core referenciada, son N hashes de MB.
   - **Cambio:** caché compartida y segura entre hilos de `fileId → checksum hex`, por ejemplo un `std::vector<std::string>` dimensionado al número de archivos y calculado bajo un `std::mutex` o con `std::call_once` por entrada, creada en `ProjectOopObjectGenerator` y pasada a cada implementación. Mismo checksum.
2. **`ProjectOopMirModel::interfaceSlotForMember`.**
   - **HECHO:** `MirBuildCoordinator.cpp:771-785` hace un bucle sobre `interfaceBindings_` y un `std::find` sobre los miembros de cada uno, por llamada de interfaz.
   - `interfaceBindingForTypes` (~760-769) es lineal.
   - **Cambio:** índices `interfaceMemberOrdinal → (binding, slot)` y `(impl, iface) → binding`, construidos al terminar `buildDispatchTables`, con fallback lineal. Debe ganar el primer binding encontrado, como hoy: usar `try_emplace` en orden.
3. **Poda de la caché en cada invocación.**
   - **HECHO:** `Driver/CompilerSession.cpp:77-96` llama `artifactCache.prune(...)` en cada invocación. `collectPrunableEntries` (`CompilationArtifactCache.cpp:143-184`) enumera recursivamente el directorio de la caché.
   - **Cambio:** podar como mucho una vez cada N minutos, con un sello `last-prune` en la raíz de la caché. Alternativa: solo cuando el tamaño acumulado escrito en esta invocación supere un umbral. No cambia lo que se compila; solo cuándo se poda.
4. **`ProjectOopMirModel` por función HIR.** `lowerFunctions:864-875` construye un modelo con 9 contenedores hash por función (INFERENCIA, asignaciones de MSVC). Con R-04.1 la copia grande desaparece. Si la medición lo señala, reutilizar un lowerer por hilo y resetear su estado. Es un cambio de diseño.
5. **`typeIsVisibleTo`.**
   - **HECHO:** `NameLookup/TypeVisibility.cpp:37-100` pasa `fold(...)` a minúsculas sobre el nombre de paquete y de módulo en cada nivel. En la rama `Internal` lo hace **siempre** sobre `modules_[accessing]->moduleName()` y cada `friend`.
   - **Cambio:** precalcular las versiones en minúsculas al construir `packageByProject_`, `friendsByProject_` y `modules_`, en `initializeModules`.
6. **`PartialTypeAggregator::finalize`.** `Compilation/PartialTypeAggregator.cpp:57` compara `key()` en el comparador. Con R-01 queda resuelto.
7. **`ProjectGenericModel::specializationForTypeKey`.**
   - **HECHO:** `ProjectGenericModel.cpp:630-641` crea `std::string(terminal)` en cada búsqueda en `specializationBySpecializedName_`. Tiene 8 llamadores, entre ellos `CarrierResolver`.
   - **Cambio:** hash transparente en ese mapa y `find(terminal)`.
   - `closedTypeKeyForTemplate` (643-…) busca la plantilla por nombre de forma lineal; usar un índice `name → primer template`.
8. **`materializeStaticFieldConstant`.**
   - **HECHO:** `MirBuildCoordinator.cpp:439-…` recorre **todos** los `hirModel.fieldInitializers()` por cada campo estático (446-448), así que es `O(SF × FI)`.
   - **Cambio:** en `buildStaticFields` (548), construir una vez un índice `memberOrdinal → primer initializer con value != nullptr`, conservando el orden.
9. **`appendExternalImport`.**
   - **HECHO:** `LibraryReferenceValidation.cpp:9-21`, por cada símbolo:
     - hace `find_if` sobre los módulos llamando `foldedName()` dos veces por comparación (dos strings en minúsculas);
     - hace `std::find` lineal sobre los símbolos.
   - **Cambio:** el llamador (`DriverArtifactGeneration.cpp:189`, `:367`, `:720` y `LibraryReferenceValidation.cpp:165`) mantiene un índice local `foldedDll → módulo` y `unordered_set` de símbolos por módulo. Mismo orden de inserción.
10. **`closureContextDestroy` y `findCallbackMirFunction`.** `OopLlvmClosuresDelegatesEvents.cpp:251-258` y `OopLlvmThunksAndWrappers.cpp:144-…` recorren todas las funciones MIR por nombre la primera vez. `ProjectOopFunctionReachabilityAnalysis` ya construye `functionsByName_` (`ProjectOopFunctionAnalysis.cpp:31-34`); exponerlo y usarlo.
11. **`ProjectGraph`.** `collectUniquePrecompiledDescriptors` (`Compilation/ProjectGraph.cpp:198-240`) llama `canonicalWindowsModuleIdentity` dentro de bucles y ejecuta un `find_if` por elemento. Son pocos elementos; se deja salvo que la medición diga lo contrario.


---

## 4. Plan de medición

Sin medir no se puede afirmar que algo mejoró. Este plan es parte de la entrega.

### 4.1 Condiciones

1. El **host en `/O2`** (o `RelWithDebInfo`) para medir. En `/Od` las proporciones cambian mucho (ver §1.1).
2. **Escenarios fijos:**
   - a) hola mundo con Core;
   - b) un proyecto mediano de los ejemplos (`examples/`);
   - c) build repetido sin cambios (caliente).

   Cada uno se ejecuta 5 veces; reportar la mediana.
3. **Herramientas existentes:**
   - `--time` del compilador (`Driver/CompilerOptionParser.cpp:60-65`, `invocation.reportTiming`);
   - el `phaseTimingSink` de `ProjectCompilation` (ya emite `method-bodies-wave-N-merge`, `MethodBodyBuildCoordinator.cpp:345-352`);
   - `ColdLlvmPreambleTelemetry` en el generador LLVM.

   NO VERIFICADO: si `nsc build` reenvía `--time` al driver anidado. Si no lo hace, añadir el reenvío o medir invocando el driver directamente sobre el proyecto generado.

### 4.2 Contadores que conviene añadir (temporales, detrás de una constante como R-09)

| Contador | Dónde | Para qué |
|---|---|---|
| Número de tipos `T` y miembros `M` (de ellos, precompilados) | Final de `ProjectMemberModel::build` | Confirmar la escala de los `O(T)` y `O(M²)` |
| Trabajos HIR lanzados frente a funciones con cuerpo | `buildRegularFunctionsInParallel` | Ver el efecto de R-03.1 |
| Llamadas a `typeOrdinalForIdentityKey` | Accesor nuevo | Cuántas búsquedas `O(T)` se evitaron |
| Llamadas a `llvmType` y aciertos de la memo | `OopLlvmImplementation::llvmType` | R-05 |
| Bytes hasheados por `sha256` y `sha256File` | `Basic/Sha256.cpp` | R-07 |
| Procesos NSLink lanzados | `LldLinker` | R-08.2 |
| Tiempo de `PublicSurfaceBuilder` por módulo | `materializePrecompiledModules` | R-10 |

### 4.3 Criterio de aceptación por lote

- Pruebas en verde: suite completa en la otra máquina.
- **Diff de salida idéntico** en los escenarios a) y b), en tres niveles:
  - IR (`--dump` de MIR y LLVM si existe);
  - `.obj` por unidad: hash igual con la misma caché desactivada;
  - `.exe` final: hash igual, salvo marcas de tiempo del PE si las hay.
- Tiempos anotados antes y después en el documento de seguimiento.

---

## 5. Apéndices

### Apéndice A · Todos los bucles sobre colecciones del proyecto en código caliente, clasificados

**Cómo se generó.** Se buscaron bucles `for` (por rango y por índice) y `std::find/find_if/any_of` sobre colecciones de todo el proyecto en `Frontend/`, `LLVM/`, `Interop/`, `Driver/`, `Linker/`, `Compilation/`, `Parser/` y `Lexer/`. Se excluyeron los dumpers, el LSP, los verificadores y los comandos de instalación y migración. Cada bucle se asignó a su función contenedora con libclang, y se contaron sus llamadores en el grafo de llamadas. Entre paréntesis va el número de llamadas que ocurren dentro de un bucle del llamador.

**Clasificación.**

- `R-xx`: tratado arriba con edición exacta.
- `POR CONSULTA`: clasificado a mano; incluye la propuesta.
- `pasada única`: se ejecuta una vez por build y recorre la colección una sola vez, así que es aceptable.
- `(por nombre; revisado en lote)`: la función es de construcción, validación, codificación o emisión. Revisé la lista completa (173 filas) y solo cambié la clasificación de 4 casos (`appendExternalImport`, `materializeStaticFieldConstant`, `emitValueDropAddress` y `verifyWindowsRuntimeLibrary`), que llevan su nota.
- `falso positivo`: el bucle recorre una lista de candidatos, no la colección completa.

| Archivo:inicio | Función | Bucles (líneas) | Colección | Llamadores (en bucle) | Clasificación |
|---|---|---|---|---|---|
| `Driver/PrecompiledSurface/ContractMaterialization.cpp:57` | `canMaterializeContractType` | 142 | `fields` | 11 (2) | R-10.4 |
| `Driver/PrecompiledSurface/ContractMaterialization.cpp:198` | `canMaterializeConstructor` | 221 | `dispatchTables` | 1 (0) | R-10.4 |
| `Driver/PrecompiledSurface/ContractMaterialization.cpp:499` | `publishedNativeSymbols` | 502, 542 | `dispatchTables, members` | 2 (0) | R-10.4/R-07.5 |
| `Driver/PublicSurfaceBuilder.cpp:1144` | `PublicSurfaceBuilder::orderedPublishedMethods` | 1150 | `dispatchTables` | 1 (0) | R-10.2 |
| `Driver/PublicSurfaceBuilder.cpp:1204` | `PublicSurfaceBuilder::dispatchRank` | 1223 | `dispatchTables` | 2 (0) | R-10.3 |
| `Driver/PublicSurfaceBuilder.cpp:1290` | `PublicSurfaceBuilder::collectPublishedMembers` | 1292, 1298 | `members, types` | 1 (0) | R-10.2 |
| `Driver/PublicSurfaceBuilder.cpp:1343` | `PublicSurfaceBuilder::publishContractTypes` | 1345, 1347 | `types` | 1 (0) | R-10.2 |
| `Frontend/Bodies/AssignmentAnalyzer.cpp:762` | `ProjectMethodBodyModel::recordEventAssignmentBinding` | 790 | `members()` | 1 (0) | R-13.3 |
| `Frontend/Bodies/AssignmentAnalyzer.cpp:2522` | `ProjectMethodBodyModel::memberCandidatesForType` | 2539, 2571 | `members(), tablePlans()` | 5 (4) | R-13.1 |
| `Frontend/Bodies/AssignmentAnalyzer.cpp:2634` | `ProjectMethodBodyModel::resolveTypeIdentifier` | 2647 | `namedTypes()` | 1 (0) | R-13.6 |
| `Frontend/Bodies/CallAnalyzer.cpp:1811` | `ProjectMethodBodyModel::recognizeCoreThreadOperations` | 1855 | `namedTypes()` | 1 (0) | R-02 |
| `Frontend/Bodies/CallAnalyzer.cpp:2217` | `ProjectMethodBodyModel::bindAtomicOperation` | 2232 | `members()` | 1 (0) | R-13 |
| `Frontend/Bodies/CallAnalyzer.cpp:2395` | `ProjectMethodBodyModel::recognizeCoreMutexMonitorOperations` | 2444 | `members()` | 1 (0) | R-13 |
| `Frontend/Bodies/CallAnalyzer.cpp:3054` | `ProjectMethodBodyModel::analyzeNativeStringPatternRestCall` | 3073 | `namedTypes()` | 1 (0) | R-02 |
| `Frontend/Bodies/CallAnalyzer.cpp:4911` | `ProjectMethodBodyModel::callTypeIsSend` | 4921 | `members()` | 3 (2) | R-13 |
| `Frontend/Bodies/CallAnalyzer.cpp:4931` | `ProjectMethodBodyModel::callTypeIsSync` | 4942 | `members()` | 3 (2) | R-13 |
| `Frontend/Bodies/ConstructionAnalyzer.cpp:41` | `ProjectMethodBodyModel::aggregateIsUnion` | 45 | `namedTypes()` | 4 (0) | R-02 |
| `Frontend/Bodies/ConstructionAnalyzer.cpp:53` | `ProjectMethodBodyModel::aggregateRequiresActiveMember` | 57 | `namedTypes()` | 4 (0) | R-02 |
| `Frontend/Bodies/ConstructionAnalyzer.cpp:65` | `ProjectMethodBodyModel::aggregateAllowsSynthesizedMember` | 77 | `namedTypes()` | 3 (0) | R-02 |
| `Frontend/Bodies/ConstructionAnalyzer.cpp:178` | `ProjectMethodBodyModel::aggregateSupportsSynthesizedMemberVisit` | 232 | `members()` | 4 (2) | R-13 |
| `Frontend/Bodies/ConstructionAnalyzer.cpp:295` | `ProjectMethodBodyModel::aggregateIsBorrowedView` | 299 | `namedTypes()` | 1 (0) | R-02 |
| `Frontend/Bodies/ConstructionAnalyzer.cpp:307` | `ProjectMethodBodyModel::aggregateElementTypeKeys` | 329 | `namedTypes()` | 2 (0) | R-02 |
| `Frontend/Bodies/ConstructionAnalyzer.cpp:349` | `ProjectMethodBodyModel::aggregateElementNames` | 376 | `namedTypes()` | 2 (0) | R-02 |
| `Frontend/Bodies/ConversionAndPassKindChecks.cpp:119` | `ProjectMethodBodyModel::classifyCVariadicPromotion` | 147 | `namedTypes()` | 3 (1) | R-02 |
| `Frontend/Bodies/ConversionAndPassKindChecks.cpp:282` | `ProjectMethodBodyModel::isAnyDelegateTypeKey` | 287 | `namedTypes()` | 8 (0) | R-02 |
| `Frontend/Bodies/LambdaDelegateBinder.cpp:59` | `ProjectMethodBodyModel::delegateTypeOrdinalForTypeKey` | 64 | `namedTypes()` | 5 (0) | R-02 |
| `Frontend/Bodies/MemberAccessAnalyzer.cpp:391` | `ProjectMethodBodyModel::analyzeNativeStringFacadeMember` | 412 | `namedTypes()` | 1 (0) | R-02 |
| `Frontend/Bodies/MethodBodyCallableLifecycle.cpp:369` | `ProjectMethodBodyModel::verifyRequiredFieldAssignments` | 376, 384 | `members()` | 1 (0) | R-13 |
| `Frontend/Bodies/MethodBodyTypeQueries.cpp:32` | `ProjectMethodBodyModel::typeKeyHasDefinedDefaultStorage` | 48 | `namedTypes()` | 2 (0) | R-02 |
| `Frontend/Bodies/MethodBodyTypeQueries.cpp:60` | `ProjectMethodBodyModel::typeKeyIsStruct` | 65 | `namedTypes()` | 6 (0) | R-02 |
| `Frontend/Bodies/StatementAnalyzer.cpp:2106` | `ProjectMethodBodyModel::exceptionRootTypeOrdinal` | 2112 | `namedTypes()` | 4 (0) | R-13.4 |
| `Frontend/Bodies/StatementAnalyzer.cpp:2182` | `ProjectMethodBodyModel::disposableTypeOrdinal` | 2188 | `namedTypes()` | 1 (0) | R-13.4 |
| `Frontend/Bodies/TypeKeyPredicates.cpp:57` | `ProjectMethodBodyModel::isEnumTypeKey` | 61 | `namedTypes()` | 13 (0) | R-02 |
| `Frontend/Bodies/TypeKeyPredicates.cpp:81` | `ProjectMethodBodyModel::elementTypeHasTrivialLifetime` | 97, 113 | `members(), namedTypes()` | 10 (0) | R-02/R-13 |
| `Frontend/Bodies/TypeKeyPredicates.cpp:126` | `ProjectMethodBodyModel::isExternCDelegateTypeKey` | 133 | `namedTypes()` | 9 (0) | R-02 |
| `Frontend/Generics/TypeSpellingAndVisibility.cpp:219` | `ProjectGenericModel::resolveModuleQualifiedDeclaredTypeIdentity` | 233 | `declaredTypeIdentities_` | 1 (0) | R-06.1 |
| `Frontend/Generics/TypeSpellingAndVisibility.cpp:245` | `ProjectGenericModel::resolveDeclaredTypeIdentity` | 293, 295 | `declaredTypeIdentities_` | 2 (1) | R-06.1 |
| `Frontend/HIR/ExpressionStatementMachine/HirCoreCallFrameLowering.cpp:561` | `ProjectOopHirModel::initializeEnumToStringCallFrame` | 589, 604 | `members(), namedTypes()` | 1 (0) | R-02/R-13 |
| `Frontend/HIR/HirBuildCoordinator.cpp:736` | `ProjectOopHirModel::buildRegularFunctionsInParallel` | 743 | `members()` | 1 (0) | R-03.1 |
| `Frontend/HIR/HirBuildCoordinator.cpp:967` | `ProjectOopHirModel::initializeRegularFunction` | 1015 | `locals()` | 1 (0) | R-03.2 |
| `Frontend/HIR/HirBuildCoordinator.cpp:1024` | `ProjectOopHirModel::appendRegularFunctionParameterCleanups` | 1031 | `cleanupRegions()` | 1 (0) | R-03.2 |
| `Frontend/HIR/HirBuildCoordinator.cpp:1327` | `ProjectOopHirModel::promoteEligibleStackClassLocals` | 1364 | `members()` | 1 (0) | R-13 |
| `Frontend/HIR/HirBuildCoordinator.cpp:1538` | `ProjectOopHirModel::buildRegularFunctionsSequentially` | 1542 | `members()` | 1 (0) | R-03.1 |
| `Frontend/HIR/HirBuildCoordinator.cpp:1582` | `ProjectOopHirModel::appendNamespaceFunctionParameterCleanups` | 1590 | `cleanupRegions()` | 1 (0) | R-03.2 |
| `Frontend/HIR/HirBuildCoordinator.cpp:1622` | `ProjectOopHirModel::buildNamespaceFunction` | 1666 | `locals()` | 1 (1) | R-03.2 |
| `Frontend/HIR/HirBuildCoordinator.cpp:1700` | `ProjectOopHirModel::buildNamespaceFunctions` | 1703 | `functions()` | 1 (0) | R-03.2 |
| `Frontend/HIR/HirBuildCoordinator.cpp:1759` | `ProjectOopHirModel::buildLambdaFunction` | 1796 | `locals()` | 1 (1) | R-03.2 |
| `Frontend/HIR/HirBuildCoordinator.cpp:1815` | `ProjectOopHirModel::buildLambdaFunctions` | 1820 | `lambdaBindings()` | 1 (0) | R-03.2 |
| `Frontend/HIR/HirBuildCoordinator.cpp:1827` | `ProjectOopHirModel::markLambdaClosureCells` | 1832, 1838 | `functions_, lambdaBindings()` | 1 (0) | R-03.3 |
| `Frontend/HIR/HirBuildCoordinator.cpp:2107` | `ProjectOopHirModel::isStructValueType` | 2112 | `namedTypes()` | 26 (1) | R-02 |
| `Frontend/HIR/HirBuildCoordinator.cpp:2119` | `ProjectOopHirModel::isClassReferenceType` | 2126 | `namedTypes()` | 0 (0) | R-02/R-05 |
| `Frontend/HIR/HirBuildCoordinator.cpp:2145` | `ProjectOopHirModel::isReferenceType` | 2155 | `namedTypes()` | 11 (4) | R-02 |
| `Frontend/HIR/HirCallLowering.cpp:1592` | `ProjectOopHirModel::lowerEnumToStringCoreCall` | 1646 | `namedTypes()` | 1 (0) | R-02 |
| `Frontend/MIR/Internal/MirNodeMachine.cpp:1534` | `ProjectOopMirModel::MirNodeMachine::lowerGroup16` | 1593 | `structLayouts_` | 1 (0) | R-04.1 |
| `Frontend/MIR/Internal/MirNodeMachine.cpp:1620` | `ProjectOopMirModel::MirNodeMachine::lowerAwaitFuture` | 1626 | `structLayouts_` | 1 (0) | R-04.1 |
| `Frontend/MIR/Internal/MirNodeMachine.cpp:1950` | `ProjectOopMirModel::MirNodeMachine::lowerGroup21` | 1976 | `namedTypes()` | 1 (0) | R-02 |
| `Frontend/MemberModel/BuildAndDeclaration.cpp:97` | `namedTypeForKey` | 103 | `namedTypes()` | 7 (0) | R-02 |
| `Frontend/MemberModel/TypeIdentity.cpp:263` | `ProjectMemberModel::canonicalGenericArgumentKey` | 294 | `namedTypes()` | 5 (1) | R-02 |
| `Frontend/ProjectGenericConstraintModel.cpp:63` | `declarationForTypeKey` | 71 | `types` | 4 (0) | R-02 |
| `Frontend/ProjectGenericConstraintModel.cpp:857` | `ProjectGenericConstraintModel::satisfiesThreadSafetyConstraint` | 867 | `members()` | 1 (0) | R-13 |
| `Frontend/ProjectInheritanceModel.cpp:264` | `ProjectInheritanceModel::typeOrdinalForTypeKey` | 272 | `namedTypes()` | 40 (3) | R-02 |
| `Frontend/ProjectInterfaceModel.cpp:327` | `ProjectInterfaceModel::typeOrdinalForTypeKey` | 335 | `namedTypes()` | 3 (0) | R-02 |
| `Frontend/ProjectNamespaceFunctionModel.cpp:1593` | `ProjectNamespaceFunctionModel::bindPrecompiledEscapeSummaries` | 1603 | `functions_` | 1 (0) | R-14.2 |
| `Frontend/ProjectObjectLayoutModel.cpp:161` | `ProjectObjectLayoutModel::fieldLayoutForMember` | 163, 165, 170, 172 | `fields, structLayouts_` | 11 (8) | R-04.2 |
| `Frontend/ProjectObjectLayoutModel.cpp:189` | `ProjectObjectLayoutModel::valueLayoutForTypeKey` | 222 | `namedTypes()` | 20 (9) | R-02 |
| `Frontend/ProjectObjectLayoutModel.cpp:605` | `ProjectObjectLayoutModel::hasTrivialStructLifetime` | 615 | `namedTypes()` | 2 (1) | R-02 |
| `Frontend/ProjectObjectLayoutModel.cpp:640` | `ProjectObjectLayoutModel::structBitFieldWidth` | 653 | `namedTypes()` | 1 (1) | R-02 |
| `Frontend/ProjectObjectLayoutModel.cpp:1059` | `ProjectObjectLayoutModel::isClassBitFieldBase` | 1065 | `namedTypes()` | 1 (0) | R-02 |
| `LLVM/LlvmTypeClassification.cpp:13` | `isDelegateReferenceType` | 21 | `namedTypes()` | 7 (0) | R-05 |
| `LLVM/LlvmTypeClassification.cpp:29` | `isClassReferenceType` | 34 | `namedTypes()` | 8 (0) | R-02/R-05 |
| `LLVM/LlvmTypeLayout.cpp:5` | `findStructLayout` | 8 | `structLayouts()` | 19 (1) | R-04.3 |
| `LLVM/LlvmTypeLayout.cpp:15` | `findClassLayout` | 18 | `classLayouts()` | 3 (0) | R-04.3 |
| `LLVM/LlvmTypeLayout.cpp:25` | `findEnumUnderlyingInfo` | 30 | `namedTypes()` | 11 (0) | R-05 |
| `LLVM/Lowering/OopLlvmDebugInfo.cpp:341` | `OopLlvmImplementation::debugNamedType` | 345 | `namedTypes()` | 4 (0) | R-02 |
| `LLVM/Lowering/OopLlvmDebugInfo.cpp:856` | `OopLlvmImplementation::debugEnumType` | 865 | `members()` | 1 (0) | R-13 |
| `LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:571` | `OopLlvmImplementation::isExternCDelegateType` | 578 | `namedTypes()` | 6 (0) | R-02 |
| `LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:587` | `OopLlvmImplementation::isVectorCallDelegateType` | 594 | `namedTypes()` | 4 (0) | R-02 |
| `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:262` | `OopLlvmImplementation::isFloat32x4TypeKey` | 266 | `namedTypes()` | 3 (1) | R-02 |
| `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:275` | `OopLlvmImplementation::compactFloatVectorLaneCount` | 279 | `namedTypes()` | 1 (0) | R-02 |
| `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:293` | `OopLlvmImplementation::integerSimd128LaneBits` | 297 | `namedTypes()` | 1 (0) | R-02 |
| `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:310` | `OopLlvmImplementation::floatingSimd256LaneBits` | 314 | `namedTypes()` | 1 (0) | R-02 |
| `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:327` | `OopLlvmImplementation::integerSimd256LaneBits` | 331 | `namedTypes()` | 1 (0) | R-02 |
| `Driver/CompatibilityPolicy.cpp:113` | `currentCompatibilityPolicy` | 116 | `fields()` | 2 (0) | pasada única (tabla fija pequeña) |
| `Driver/DriverArtifactGeneration.cpp:164` | `prepareExecutableBuilder` | 187, 188 | `symbols` | 1 (0) | pasada única por build (símbolos del archivo; ver R-07.5) |
| `Driver/DriverArtifactGeneration.cpp:700` | `prepareDynamicLibraryBuilder` | 718, 719 | `symbols` | 1 (0) | solo biblioteca; pasada única |
| `Driver/PrecompiledModuleConfiguration.cpp:7` | `precompiledMemberIdentities` | 11 | `members` | 1 (1) | pasada única por módulo |
| `Driver/ProjectCommands.cpp:1089` | `dependencyProvenanceJson` | 1093 | `records` | 1 (0) | solo con --dependency-report (R-07.2) |
| `Frontend/Bodies/CarrierResolver.cpp:153` | `ProjectMethodBodyModel::resolveOptionalCarrier` | 168, 190 | `members, templates` | 1 (0) | POR CONSULTA (por Optional<T>): find_if sobre templates() + recorrido de members() → R-13 (membersDeclaredByType) y índice canonicalKey→template; P2 |
| `Frontend/Bodies/ExpressionCore/PrimaryExpressions.cpp:578` | `ProjectMethodBodyModel::resolveNameOfTemplateGenericMethod` | 602 | `templates()` | 1 (0) | por nameof sobre plantilla (raro); P2 |
| `Frontend/Bodies/ExpressionCore/PrimaryExpressions.cpp:820` | `ProjectMethodBodyModel::interpolationHandlerMember` | 824 | `members()` | 3 (0) | falso positivo: itera candidatos de findMembers, no todos los miembros |
| `Frontend/Bodies/MemberAccessAnalyzer.cpp:905` | `ProjectMethodBodyModel::analyzeListPatternLengthMember` | 924 | `methods` | 1 (0) | falso positivo: itera candidatos locales |
| `Frontend/Bodies/MemberAccessAnalyzer.cpp:1208` | `ProjectMethodBodyModel::stage38OperatorCandidates` | 1259 | `functions()` | 2 (0) | POR CONSULTA (por operador): recorre TODAS las funciones de namespace (1259-1268) buscando extensiones con ese nombre → índice name→[ordinales de extensión] en ProjectNamespaceFunctionModel, orden ascendente; P1 |
| `Frontend/Bodies/MethodBodyBuildCoordinator.cpp:536` | `ProjectMethodBodyModel::analyzeFieldInitializers` | 543 | `members()` | 1 (0) | pasada única |
| `Frontend/Bodies/MethodBodyCallableLifecycle.cpp:205` | `ProjectMethodBodyModel::analyzeCallable` | 231 | `locals()` | 1 (0) | por callable, sobre locals del shard (local al callable) y solo si async con awaits; aceptable |
| `Frontend/Bodies/OwnershipAnalyzer.cpp:623` | `ProjectMethodBodyModel::conflictingMutableReference` | 630 | `locals_` | 4 (0) | por acceso, sobre locals_ del SHARD (solo el callable): O(L_callable) por acceso; aceptable salvo funciones enormes; P2 |
| `Frontend/Bodies/OwnershipAnalyzer.cpp:745` | `ProjectMethodBodyModel::hasActiveBorrowOfOwnerLocal` | 789 | `locals_` | 6 (0) | igual que conflictingMutableReference; además construye "L"+to_string por llamada (OwnershipAnalyzer.cpp:785); P2 |
| `Frontend/ComptimeEngine.cpp:351` | `resolveCapabilityCalls` | 368 | `calls` | 1 (0) | comptime; por expresión comptime; P2 |
| `Frontend/Generics/TraitImplementationMerger.cpp:152` | `ProjectGenericModel::findTraitImplementationOwner` | 156 | `templates` | 1 (0) | por trait impl; P2 |
| `Frontend/HIR/ExpressionStatementMachine/HirAggregateCrossFrameLowering.cpp:126` | `ProjectOopHirModel::prepareStructAggregateCrossStructuralFrame` | 145 | `fields` | 1 (0) | campos del propio layout (local); aceptable |
| `Frontend/HIR/HirAggregateLowering.cpp:111` | `ProjectOopHirModel::populateStructAggregateSlots` | 128 | `fields` | 1 (0) | campos del propio layout (local); aceptable |
| `Frontend/HIR/HirAsyncLowering.cpp:853` | `ProjectOopHirModel::awaitProducerCanSuspend` | 866 | `members()` | 1 (0) | falso positivo: recorre una lista de pendientes |
| `Frontend/HIR/HirModelFacade.cpp:27` | `ProjectOopHirModel::~ProjectOopHirModel` | 29 | `functions_` | 0 (0) | destructor: libera funciones; ver R-15.4 (un modelo por worker) |
| `Frontend/MIR/MirBuildCoordinator.cpp:807` | `ProjectOopMirModel::executableEntryPoint` | 813 | `functions_` | 1 (0) | una vez |
| `Frontend/MIR/MirCache.cpp:1069` | `restoreMirFunctionCore` | 1086 | `cleanupRegions` | 1 (1) | regiones de la propia función; aceptable |
| `Frontend/MIR/MirFunctionLowerer.cpp:256` | `abiPassingForType` | 260 | `layouts` | 2 (1) | POR CONSULTA: configureFunctionAbi (MirFunctionLowerer.cpp:266-282) la llama para el retorno y CADA parámetro de CADA función (incluidas las importadas de Core) y recorre TODAS las struct layouts → O(F×P×S). Usar el índice de R-04.3 (construido antes de lowerFunctions y pasado por const& a lowerOneFunction/configureFunctionAbi); P0 junto a R-04 |
| `Frontend/MIR/MirFunctionLowerer.cpp:290` | `sumAwaitedFrameSizeNodes` | 303 | `layouts` | 2 (0) | por nodo await: recorre todas las layouts → índice R-04.3; P2 |
| `Frontend/MIR/MirFunctionLowerer.cpp:646` | `inheritMoveNextDropFrames` | 648, 653 | `functions` | 1 (0) | O(G×F) con G = funciones $moveNext (pocas) → índice mangledName→ordinal una vez; P2 |
| `Frontend/MIR/MirFunctionLowerer.cpp:676` | `inheritConstructorFrames` | 682, 688 | `functions` | 3 (0) | igual (3 llamadas, MirFunctionLowerer.cpp:911-913); P2 |
| `Frontend/MemberModel/MemberBodyScan.cpp:43` | `ProjectMemberModel::completeThisMemberAccess` | 54 | `members_` | 1 (1) | falso positivo: candidatos de findMembers |
| `Frontend/MemberModel/TypeIdentity.cpp:138` | `ProjectMemberModel::canonicalNamedTypeKey` | 180 | `namedTypes()` | 1 (0) | fallback raro (R-02 #38) |
| `Frontend/NameLookup/BuiltinTypeValidation.cpp:109` | `ProjectNameLookup::appendObjectTypeRoot` | 120 | `namedTypes_` | 1 (0) | solo `object` sin resolver (R-02, excluidos) |
| `Frontend/ProjectCompilation.cpp:602` | `ProjectCompilation::compileProjectMembers` | 622 | `specializations()` | 2 (0) | pasada única |
| `Frontend/ProjectGenericModel.cpp:643` | `ProjectGenericModel::closedTypeKeyForTemplate` | 648 | `templates()` | 2 (0) | POR CONSULTA (2 llamadores): lineal sobre templates() por nombre → índice name→primer template; P2 |
| `Frontend/ProjectInheritanceModel.cpp:107` | `ProjectInheritanceModel::checkProtectedNestedTypeUses` | 131 | `bindings()` | 1 (0) | pasada única |
| `Frontend/ProjectInheritanceModel.cpp:296` | `ProjectInheritanceModel::objectRootTypeOrdinal` | 307 | `namedTypes()` | 1 (0) | una vez (llamador único :462) |
| `Frontend/ProjectMetaprogrammingModel.cpp:186` | `ProjectMetaprogrammingModel::findAttributeTarget` | 195, 211 | `members(), namedTypes()` | 1 (0) | por atributo; recorre tipos y miembros; P2 |
| `Frontend/ProjectMetaprogrammingModel.cpp:565` | `ProjectMetaprogrammingModel::derivedMemberCollides` | 569 | `members` | 1 (1) | por derive; miembros del propio tipo; aceptable |
| `Frontend/ProjectMetaprogrammingModel.cpp:702` | `ProjectMetaprogrammingModel::findType` | 706 | `types_` | 0 (0) | comptime/metaprogramación; P2 |
| `Frontend/ProjectMetaprogrammingModel.cpp:712` | `ProjectMetaprogrammingModel::visibleMembers` | 718 | `members` | 0 (0) | comptime/metaprogramación; P2 |
| `Interop/CHeaderGenerator.cpp:130` | `namedTypeForCoreKey` | 136 | `namedTypes()` | 5 (1) | solo generación de header C (R-02 #51) |
| `Interop/CHeaderGenerator.cpp:586` | `emitStructAssertions` | 597 | `fields` | 1 (0) | solo header C |
| `Interop/CHeaderGenerator.cpp:785` | `emitOrderedDefinitions` | 793, 840 | `types` | 1 (0) | solo header C |
| `Interop/CHeaderGenerator.cpp:908` | `emitForwardDeclarations` | 914 | `namedTypes` | 1 (0) | solo header C |
| `Interop/CHeaderGenerator.cpp:933` | `emitEnumDeclarations` | 940 | `namedTypes` | 1 (0) | solo header C |
| `Interop/CHeaderGenerator.cpp:968` | `emitExports` | 976, 984 | `functions(), members()` | 1 (0) | solo header C |
| `Interop/LibraryCapabilityAudit.cpp:21` | `hasType` | 23, 24 | `types` | 1 (1) | auditoría de capacidades (biblioteca) |
| `Interop/LibraryCapabilityAudit.cpp:78` | `auditLibraryCapabilitySurfaces` | 88 | `types` | 1 (1) | auditoría de capacidades (biblioteca) |
| `Interop/LibraryMetadataCodec.cpp:124` | `normalizeWritableDelegateContracts` | 128, 139 | `types` | 1 (0) | escritura de NSMETA (al producir biblioteca); pasada única |
| `Interop/LibraryMetadataCodec.cpp:171` | `normalizeWritableEscapeSummaries` | 176 | `members` | 1 (0) | escritura de NSMETA; pasada única |
| `Interop/LibraryMetadataCodec.cpp:268` | `downgradeMetadataBefore18` | 273, 281, 283, 284, 288, 291, 294, 295 | `fields, members, types` | 1 (0) | solo metadatos antiguos |
| `Interop/LibraryMetadataCodec.cpp:420` | `typeUsesDynamicTrait` | 427, 429 | `fields` | 1 (1) | campos del propio tipo; aceptable |
| `Interop/LibraryMetadataCodec.cpp:481` | `typeHasLifetime` | 492, 494 | `fields` | 1 (1) | campos del propio tipo; aceptable |
| `Interop/LibraryMetadataConstruction.cpp:948` | `finalizeExternalMetadataTypes` | 954 | `dispatchTables` | 1 (0) | producción de NSMETA; revisar si es por tipo × dispatchTables (P2, solo al construir Core) |
| `Interop/LibraryMetadataConstruction.cpp:2329` | `resolveMemberPhysicalIdentity` | 2343 | `functions()` | 1 (0) | producción de NSMETA: recorre mirModel.functions() por miembro → O(M×F) al construir Core.lib; P2 (no afecta a hola mundo) |
| `Interop/LibraryMetadataMerger.cpp:233` | `canonicalize` | 246 | `records` | 11 (1) | merge de metadatos; P2 |
| `Interop/LibraryMetadataMerger.cpp:270` | `canonicalizeNestedRecords` | 272, 285, 287 | `dispatchTables, types` | 1 (0) | merge de metadatos; P2 |
| `Interop/LibraryMetadataPayload.cpp:152` | `encodeTypeFields` | 158 | `fields` | 1 (1) | codificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:177` | `encodeTypes` | 180 | `types` | 1 (0) | codificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:250` | `encodeMembersAndTraitImpls` | 256, 268 | `members, methods` | 1 (0) | codificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:302` | `encodeDispatchRecords` | 305 | `dispatchTables` | 1 (0) | codificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:424` | `encodeLifetimeContracts` | 428, 439, 442 | `fields, members, types` | 1 (0) | codificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:471` | `encodeEscapeContracts` | 475 | `members` | 1 (0) | codificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:482` | `encodeErrorContracts` | 486, 489 | `members, types` | 1 (0) | codificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:496` | `encodeGenericContracts` | 518 | `members` | 1 (0) | codificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:1485` | `decodeTypeLifetimeContract` | 1531 | `fields` | 1 (1) | decodificación; campos del propio tipo |
| `Interop/LibraryMetadataPayload.cpp:1542` | `decodeTypeLifetimeCatalogue` | 1553 | `types` | 1 (0) | decodificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:1589` | `decodeMemberLifetimeCatalogue` | 1600 | `members` | 1 (0) | decodificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:1742` | `decodeErrorContracts` | 1755, 1766 | `members, types` | 1 (0) | decodificación; pasada única |
| `Interop/LibraryMetadataPayload.cpp:1898` | `decodeFfiMemberContracts` | 1909 | `members` | 1 (0) | decodificación; pasada única |
| `Interop/LibraryMetadataVerification.cpp:98` | `typeDirectlyOwnsStorage` | 100 | `fields` | 1 (0) | campos del propio tipo |
| `Interop/LibraryMetadataVerification.cpp:595` | `findMetadataDispatch` | 599 | `dispatchTables` | 1 (0) | R-10.6 |
| `LLVM/Internal/ImplementationStatements.cpp:60` | `LlvmLowerer::Implementation::lowerFunction` | 106 | `locals()` | 1 (1) | backend legado (sema), no la ruta OOP de nsc build |
| `LLVM/Internal/ImplementationTypes.cpp:226` | `LlvmLowerer::Implementation::layoutOf` | 279 | `members()` | 4 (1) | backend legado (sema) |
| `LLVM/Lowering/OopLlvmClosuresDelegatesEvents.cpp:241` | `OopLlvmImplementation::closureContextDestroy` | 251 | `functions()` | 1 (0) | por callee de closure (memoizado por nombre en closureContextDestroyFunctions_), pero la primera vez recorre todas las funciones MIR → usar un índice mangledName→función compartido (ProjectOopFunctionReachabilityAnalysis ya tiene functionsByName_); P2 |
| `LLVM/Lowering/OopLlvmDebugInfo.cpp:455` | `OopLlvmImplementation::appendDebugClassMembers` | 462 | `fields` | 1 (0) | campos del propio layout |
| `LLVM/Lowering/OopLlvmDebugInfo.cpp:890` | `OopLlvmImplementation::debugStructMembers` | 899 | `fields` | 1 (0) | campos del propio layout |
| `LLVM/Lowering/OopLlvmExpressionLeaves.cpp:436` | `OopLlvmImplementation::futureFieldOffset` | 441 | `fields` | 3 (0) | campos del propio layout |
| `LLVM/Lowering/OopLlvmExpressionMachine.cpp:862` | `OopLlvmImplementation::lowerExpressionFrame016IteratorMoveNext` | 913 | `fields` | 1 (0) | campos del propio layout |
| `LLVM/Lowering/OopLlvmFunctionDefinition.cpp:233` | `OopLlvmImplementation::prepareFunctionCleanupState` | 239 | `cleanupRegions` | 1 (0) | regiones de la propia función |
| `LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:144` | `OopLlvmImplementation::findCallbackMirFunction` | 147 | `functions()` | 1 (0) | por thunk de callback; recorre todas las funciones MIR → mismo índice por nombre; P2 |
| `LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:344` | `OopLlvmImplementation::vectorCallHvaArity` | 354 | `fields` | 3 (2) | campos del propio layout |
| `LLVM/Lowering/Support/DispatchGlobals.cpp:292` | `emitInterfaceMapGlobals` | 304 | `interfaceTables` | 1 (1) | pasada única |
| `LLVM/ProjectOopFunctionAnalysis.cpp:23` | `…ectOopFunctionReachabilityAnalysis::ProjectOopFunctionReachabilityAnalysis` | 33, 49, 57, 66 | `cleanupRegions, fields, functions` | 1 (0) | pasada única (construcción del análisis) |
| `LLVM/ProjectOopFunctionAnalysis.cpp:260` | `ProjectOopFunctionReachabilityAnalysis::coordinatorFunctionDeclarations` | 265, 274, 282, 289 | `dispatchTables(), fieldInitializers(), functions, interfaceT` | 1 (0) | pasada única |
| `LLVM/ProjectOopFunctionAnalysis.cpp:302` | `ProjectOopFunctionReachabilityAnalysis::staticFieldDeclarationMask` | 318, 320, 338 | `cleanupRegions, fieldInitializers()` | 1 (0) | pasada única |
| `LLVM/ProjectOopMirCacheIdentity.cpp:426` | `ProjectOopMirCacheIdentityIndex::ProjectOopMirCacheIdentityIndex` | 436 | `functions()` | 2 (0) | pasada única |
| `LLVM/ProjectOopObjectGenerator.cpp:484` | `requiresWholeModuleInlining` | 488 | `functions()` | 1 (0) | pasada única |
| `Linker/WindowsExecutableBuilder.cpp:115` | `incrementalLinkKey` | 138 | `symbols` | 1 (0) | solo --incremental-link |
| `Linker/WindowsRuntimeImports.cpp:22` | `addSymbols` | 42 | `symbols` | 6 (1) | construcción de listas de import: std::find por símbolo (WindowsRuntimeImports.cpp:46/75) → O(S²) con S pequeño; con la caché de R-08.2 desaparece |
| `Linker/WindowsRuntimeImports.cpp:53` | `addSymbols` | 72 | `symbols` | 6 (1) | construcción de listas de import: std::find por símbolo (WindowsRuntimeImports.cpp:46/75) → O(S²) con S pequeño; con la caché de R-08.2 desaparece |
| `Compilation/PartialTypeAggregator.cpp:45` | `PartialTypeAggregator::finalize` | 85 | `types_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/CompatibilityPolicy.cpp:159` | `verifyInstalledCompatibilityCapabilities` | 165 | `fields()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/DriverArtifactGeneration.cpp:11` | `collectExternalImports` | 17 | `functions()` | 3 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/DriverArtifactGeneration.cpp:354` | `publishStaticLibraryArtifact` | 365, 366 | `symbols` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/DriverArtifactGeneration.cpp:523` | `collectDynamicMethodExports` | 528 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/DriverArtifactGeneration.cpp:564` | `collectDynamicNamespaceExports` | 570 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/DriverArtifactGeneration.cpp:586` | `collectDynamicLayoutExports` | 590, 599 | `dispatchTables(), structLayouts()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/DriverArtifactGeneration.cpp:610` | `collectDynamicStateExports` | 615 | `staticFields()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/LibraryReferenceValidation.cpp:9` | `appendExternalImport` | 19 | `symbols` | 5 (4) | POR SÍMBOLO: find_if sobre módulos con foldedName() (2 strings en minúsculas por comparación) + std::find lineal sobre símbolos → O(S²) con asignaciones; índice local (unordered_set por DLL) en el llamador; P2 |
| `Driver/LibraryReferenceValidation.cpp:236` | `collectProjectRuntimeReferences` | 240, 242 | `fieldInitializers(), functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/LibraryReferenceValidation.cpp:331` | `appendReferencedRuntimeLibraries` | 342, 346 | `functions(), staticFields()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/PrecompiledModuleConfiguration.cpp:61` | `materializePrecompiledModules` | 70 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/PrecompiledSurface/PrecompiledSourceGeneration.cpp:97` | `verifyPrecompiledGenericTemplateSource` | 120 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/PublicSurfaceBuilder.cpp:264` | `PublicSurfaceBuilder::collectAccessorContracts` | 268 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/PublicSurfaceBuilder.cpp:494` | `PublicSurfaceBuilder::emitEnumType` | 508 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/PublicSurfaceBuilder.cpp:1094` | `PublicSurfaceBuilder::emitNominalType` | 1116 | `fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/PublicSurfaceBuilder.cpp:1265` | `PublicSurfaceBuilder::collectPublishedTypes` | 1267, 1280, 1282 | `fields, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/PublicSurfaceBuilder.cpp:1359` | `PublicSurfaceBuilder::emitAliases` | 1372 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Driver/PublicSurfaceBuilder.cpp:1667` | `PublicSurfaceBuilder::emitTraitImpl` | 1678 | `methods` | 1 (1) | pasada única (por nombre; revisado en lote) |
| `Driver/PublicSurfaceEmitter.cpp:38` | `PublicSurfaceEmitter::verify` | 64, 68, 69, 73, 75 | `fields, members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/AssignmentAnalyzer.cpp:1145` | `ProjectMethodBodyModel::validateBorrowedAssignmentOwnerScopes` | 1159 | `locals_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/EscapeAnalyzer.cpp:202` | `ProjectMethodBodyModel::buildEscapeDependencies` | 206 | `calls_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/EscapeAnalyzer.cpp:312` | `ProjectMethodBodyModel::validateEscapeCalls` | 315 | `calls_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/MethodBodyBuildCoordinator.cpp:28` | `ProjectMethodBodyModel::initializeNamespaceCallableAdapters` | 45 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/MethodBodyBuildCoordinator.cpp:96` | `ProjectMethodBodyModel::validateDeclaredErrorTypes` | 131 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/MethodBodyBuildCoordinator.cpp:213` | `ProjectMethodBodyModel::seedDemandedGenericMethods` | 227, 250 | `members(), tablePlans()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/MethodBodyBuildCoordinator.cpp:263` | `ProjectMethodBodyModel::createCallableWorklist` | 270 | `members()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/MethodBodyBuildCoordinator.cpp:558` | `ProjectMethodBodyModel::validateConstantFieldInitializers` | 565 | `members()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/MethodBodyBuildCoordinator.cpp:581` | `ProjectMethodBodyModel::validateRequiredInliningCycles` | 591, 605 | `calls_, members()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/MethodBodyCallableLifecycle.cpp:297` | `ProjectMethodBodyModel::collectCallableDiscoveries` | 301 | `calls_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Bodies/MethodBodyCallableLifecycle.cpp:345` | `ProjectMethodBodyModel::verifyOutParameterAssignments` | 349 | `members()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/Generics/TraitImplementationMerger.cpp:248` | `ProjectGenericModel::mergeTraitImplementations` | 259 | `implementations` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/HIR/HirBuildCoordinator.cpp:781` | `ProjectOopHirModel::mergeParallelRegularFunctions` | 798 | `functions` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/HIR/HirBuildCoordinator.cpp:1942` | `ProjectOopHirModel::buildFieldInitializers` | 1947 | `members()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/HIR/HirBuildCoordinator.cpp:2022` | `ProjectOopHirModel::buildImplicitConstructors` | 2028 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirBuildCoordinator.cpp:84` | `ProjectOopMirModel::initializeBuild` | 116 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirBuildCoordinator.cpp:153` | `ProjectOopMirModel::appendDispatchDestructors` | 170 | `members()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirBuildCoordinator.cpp:383` | `ProjectOopMirModel::buildOwnedFieldDestructionPlans` | 402, 404 | `dispatchTables_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirBuildCoordinator.cpp:414` | `ProjectOopMirModel::lowerFieldInitializers` | 418 | `fieldInitializers()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirBuildCoordinator.cpp:439` | `ProjectOopMirModel::materializeStaticFieldConstant` | 446 | `fieldInitializers()` | 1 (0) | POR CAMPO ESTÁTICO: recorre todos los fieldInitializers() → O(SF×FI); índice memberOrdinal→initializer una vez en buildStaticFields; P2 |
| `Frontend/MIR/MirBuildCoordinator.cpp:548` | `ProjectOopMirModel::buildStaticFields` | 554 | `members()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirCache.cpp:709` | `appendMirFunctionCleanupRegions` | 713 | `cleanupRegions` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirCache.cpp:1105` | `ProjectOopMirModel::storeMirFunctions` | 1108 | `functions_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirCache.cpp:1118` | `ProjectOopMirModel::restoreMirFunctions` | 1121 | `functions_` | 0 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirLayoutImporter.cpp:74` | `ProjectOopMirModel::importNamedStructLayouts` | 79 | `structLayouts()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirLayoutImporter.cpp:186` | `ProjectOopMirModel::importTupleLayouts` | 198 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirLayoutImporter.cpp:217` | `ProjectOopMirModel::importClassLayouts` | 219 | `classLayouts()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirLayoutImporter.cpp:314` | `ProjectOopMirModel::importFutureLayouts` | 328 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MIR/MirLayoutImporter.cpp:430` | `ProjectOopMirModel::importIteratorLayouts` | 445 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/AnonymousMemberPromotion.cpp:205` | `ProjectMemberModel::collectAnonymousMemberPromotions` | 211 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildAndDeclaration.cpp:673` | `ProjectMemberModel::validateCAbiExportNames` | 677 | `members_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildAndDeclaration.cpp:695` | `ProjectMemberModel::collectImportedCLibraries` | 702 | `members_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildAndDeclaration.cpp:734` | `ProjectMemberModel::buildModuleInitializationPlan` | 787 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildSupport.cpp:41` | `ProjectMemberModel::initializeBuildState` | 53 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildSupport.cpp:58` | `ProjectMemberModel::collectDeclaredTypeMembers` | 62 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildSupport.cpp:83` | `ProjectMemberModel::collectExternalTraitImplMembers` | 95 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildSupport.cpp:146` | `ProjectMemberModel::validateCopyTypes` | 153 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildSupport.cpp:189` | `ProjectMemberModel::validateRawUnions` | 199 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildSupport.cpp:223` | `ProjectMemberModel::markRuntimeBoundImports` | 227 | `members_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildSupport.cpp:284` | `ProjectMemberModel::scanMemberBodies` | 287 | `members_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/BuildSupport.cpp:316` | `ProjectMemberModel::bindPrecompiledPhysicalNames` | 352 | `members_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/MemberModel/OwnershipQueries.cpp:262` | `ProjectMemberModel::buildMemberNameIndex` | 279 | `members_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/NameLookup/ProjectSetup.cpp:96` | `ProjectNameLookup::collectDeclarationIndexes` | 115 | `namespaces` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/NameLookup/TypeAggregation.cpp:444` | `ProjectNameLookup::finalizeContainingTypeOrdinals` | 450 | `namedTypes_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectCallableResultMerger.cpp:288` | `ProjectCallableResultMerger::indexMergedBindings` | 315 | `calls_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectConstructionModel.cpp:14` | `ProjectConstructionModel::build` | 58 | `members()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectGenericConstraintModel.cpp:485` | `ProjectGenericConstraintModel::validateTemplateClauses` | 498 | `templates` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectGenericConstraintModel.cpp:634` | `ProjectGenericConstraintModel::validateControlledSpecializations` | 640 | `templates` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectGenericConstraintModel.cpp:1152` | `ProjectGenericConstraintModel::build` | 1171 | `specializations()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectGenericModel.cpp:544` | `ProjectGenericModel::finalizeBuildState` | 550 | `specializations_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectInheritanceModel.cpp:13` | `ProjectInheritanceModel::build` | 28 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectInterfaceModel.cpp:85` | `ProjectInterfaceModel::build` | 107 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectInterfaceModel.cpp:235` | `ProjectInterfaceModel::buildTraitObjectContracts` | 295 | `bindings()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectInterfaceModel.cpp:1243` | `ProjectInterfaceModel::buildClassPlans` | 1253 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectMetaprogrammingModel.cpp:117` | `ProjectMetaprogrammingModel::collectStaticTypes` | 123 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectMetaprogrammingModel.cpp:505` | `ProjectMetaprogrammingModel::validateAvailabilityBindings` | 514, 531 | `bindings(), calls()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectMetaprogrammingModel.cpp:610` | `ProjectMetaprogrammingModel::collectDerives` | 612, 623 | `members, types_` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectObjectLayoutModel.cpp:65` | `ProjectObjectLayoutModel::build` | 97, 105 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectObjectLayoutModel.cpp:814` | `ProjectObjectLayoutModel::validateStructFieldOverlaps` | 819, 821 | `fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectObjectLayoutModel.cpp:1235` | `ProjectObjectLayoutModel::buildPromotedFieldLayouts` | 1245 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Frontend/ProjectObjectLayoutModel.cpp:1309` | `ProjectObjectLayoutModel::buildAllocationPlans` | 1315 | `constructions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/CHeaderGenerator.cpp:150` | `buildHostTypeNames` | 153, 161 | `namedTypes()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/CHeaderGenerator.cpp:253` | `collectReachableHostTypes` | 257, 268, 289 | `functions(), members(), types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryAbiReport.cpp:189` | `writeTypeFields` | 192, 198 | `fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryAbiReport.cpp:242` | `writeTypes` | 245, 251 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryAbiReport.cpp:254` | `writeMembers` | 257, 264 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryAbiReport.cpp:286` | `writeDispatchTables` | 289 | `dispatchTables` | 2 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryApiDiff.cpp:160` | `appendSourceTypes` | 162, 199 | `fields, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryApiDiff.cpp:226` | `appendSourceMembers` | 228 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryApiDiff.cpp:362` | `appendAbiTypes` | 364, 404 | `fields, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryApiDiff.cpp:419` | `appendAbiMembers` | 421 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryApiDiff.cpp:490` | `appendAbiDispatchTables` | 492 | `dispatchTables` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataCodec.cpp:299` | `validateMetadataBefore21` | 305, 306, 315, 324 | `fields, members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataCodec.cpp:342` | `validateMetadataBefore31` | 348, 365, 374, 383, 393, 402 | `members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataCodec.cpp:434` | `validateDynamicMetadataBefore31` | 439, 445 | `members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataCodec.cpp:499` | `validateMetadataBefore35` | 511, 518, 524 | `members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataCodec.cpp:539` | `validateMetadataBefore38` | 549, 555, 562, 568 | `members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:794` | `appendPlatformImports` | 799, 805 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:855` | `appendMetadataDispatchTables` | 862, 882 | `dispatchTables(), interfaceTables` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:906` | `appendMetadataExports` | 913 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:992` | `appendSyntheticImplicitConstructors` | 998 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:1245` | `collectReferencedMetadataTypes` | 1249, 1252, 1254, 1265 | `fields, members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:1283` | `collectRequiredSpecializationKeys` | 1288, 1300 | `specializations` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:1314` | `appendGenericSpecializations` | 1322 | `specializations` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:1355` | `collectExternalGenericTypes` | 1362 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:1592` | `appendPublishedLayoutFields` | 1598 | `fields` | 2 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:1630` | `appendPublishedMemberEdges` | 1637 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:1712` | `collectPublishedRootTypeOrdinals` | 1721 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:1977` | `appendStructLayoutMetadata` | 1988, 1993 | `fields, structLayouts()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:2002` | `appendClassLayoutMetadata` | 2013 | `fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:2271` | `collectMetadataMemberOrdinals` | 2277 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:2557` | `appendExternalTraitImplMetadata` | 2579, 2591 | `members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:2628` | `buildMemberMetadata` | 2641 | `staticFields()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataConstruction.cpp:2820` | `appendNamespaceFunctionMetadata` | 2829 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataPayload.cpp:563` | `appendVariantProjection` | 580 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataPayload.cpp:593` | `appendDropGlueProjection` | 606 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataPayload.cpp:614` | `appendTraitProjection` | 627 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataPayload.cpp:640` | `appendAssociatedItemProjection` | 650 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataPayload.cpp:674` | `appendEscapeProjection` | 681 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataPayload.cpp:689` | `appendErrorTypeProjection` | 696, 702 | `members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:393` | `verifyMetadataTypeDeclarations` | 403 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:417` | `verifyMetadataReferenceAndDelegateContracts` | 421 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:675` | `verifyMetadataDispatchContracts` | 681, 684, 695, 703 | `dispatchTables, members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:976` | `verifyGenericAndExternalTypeCatalogues` | 985 | `types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:1080` | `verifyMetadataTypeReferences` | 1087, 1099 | `fields, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:1131` | `verifyMetadataCallableTypeReferences` | 1138 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:1352` | `verifyInlineAndVolatileMemberMetadata` | 1360, 1380 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:1394` | `verifyReferenceReturnMemberMetadata` | 1404 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:1422` | `verifyNoHeapAndVariadicMemberMetadata` | 1430, 1445 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:1485` | `verifyParameterPackContracts` | 1490 | `members` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Interop/LibraryMetadataVerification.cpp:1553` | `verifyLifetimeMetadataContracts` | 1558, 1573 | `members, types` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Internal/Implementation.cpp:361` | `LlvmLowerer::Implementation::defineStructBodies` | 369 | `members()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Internal/Implementation.cpp:419` | `LlvmLowerer::Implementation::declareFunctions` | 421 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Internal/ImplementationStatements.cpp:44` | `LlvmLowerer::Implementation::lowerFunctions` | 46 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmCleanup.cpp:675` | `OopLlvmImplementation::collectVariantDropCases` | 682 | `fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmCleanup.cpp:752` | `OopLlvmImplementation::emitVariantDropAddress` | 762 | `fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmCleanup.cpp:880` | `OopLlvmImplementation::emitValueDropAddress` | 909 | `fields` | 9 (5) | campos del propio layout; :911 indexa members() por ordinal (no recorre); aceptable |
| `LLVM/Lowering/OopLlvmDebugInfo.cpp:11` | `OopLlvmImplementation::initializeDebugInformation` | 23 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmDeclarations.cpp:178` | `OopLlvmImplementation::declareValueDropEntries` | 187 | `structLayouts()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmDeclarations.cpp:202` | `OopLlvmImplementation::declareDestructionEntries` | 213 | `dispatchTables()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmDeclarations.cpp:282` | `OopLlvmImplementation::declarePartitionedDispatchMetadata` | 287 | `dispatchTables()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmDeclarations.cpp:316` | `OopLlvmImplementation::declareDispatchMetadataStorage` | 321 | `dispatchTables()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmDeclarations.cpp:385` | `OopLlvmImplementation::defineDispatchMetadataAccessors` | 390 | `dispatchTables()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmDeclarations.cpp:655` | `OopLlvmImplementation::defineTypedCatchFilters` | 665, 679 | `dispatchTables(), functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmInlineAssembly.cpp:11` | `OopLlvmImplementation::collectInlineAssemblyRecords` | 41 | `records` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmStaticState.cpp:29` | `OopLlvmImplementation::declareStaticFields` | 32 | `fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmStaticState.cpp:132` | `OopLlvmImplementation::defineStaticFieldAccessors` | 135 | `fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmStaticState.cpp:262` | `OopLlvmImplementation::collectTypeInitializerRequirements` | 267, 274 | `fieldInitializers(), functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmStaticState.cpp:417` | `OopLlvmImplementation::emitTypeInitializerBody` | 436 | `fieldInitializers()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:51` | `OopLlvmImplementation::defineCallbackGuardThunks` | 69, 85, 90 | `cleanupRegions, fieldInitializers(), functions` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:604` | `OopLlvmImplementation::defineCAbiWrappers` | 606 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:742` | `OopLlvmImplementation::defineValueDropEntries` | 744 | `structLayouts()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:962` | `OopLlvmImplementation::defineDestructionEntries` | 964 | `dispatchTables()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/Lowering/Support/DispatchGlobals.cpp:149` | `computeRuntimeDescriptorAccessorTypes` | 153, 161 | `dispatchTables()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopCodePartition.cpp:121` | `collectPartitionCandidates` | 128 | `functions` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopCodePartition.cpp:266` | `collectPartitionNodeDependencies` | 332 | `fields` | 2 (2) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopCodePartition.cpp:341` | `populateUnitDeclarations` | 350, 358, 371 | `cleanupRegions, functions, structLayouts()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopCodePartition.cpp:453` | `ProjectOopCodePartition::persistentPlanKey` | 467 | `functions` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopFunctionReachability.cpp:57` | `projectOopReachableDefinitionMask` | 66, 84, 113 | `cleanupRegions, functions` | 4 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopFunctionReachability.cpp:119` | `projectOopStaticFieldDeclarationMask` | 135, 170, 172, 188 | `cleanupRegions, fieldInitializers(), fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopFunctionReachability.cpp:196` | `projectOopCoordinatorFunctionDeclarations` | 207, 238, 240, 247, 251, 252 | `dispatchTables(), fieldInitializers(), fields, functions, in` | 0 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopMirCacheIdentity.cpp:229` | `writeFunctionCleanupRegions` | 234 | `cleanupRegions` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopMirCacheIdentity.cpp:283` | `writeStructLayouts` | 286, 295 | `fields, structLayouts()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopMirCacheIdentity.cpp:299` | `writeClassLayouts` | 302, 309 | `classLayouts(), fields` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopMirCacheIdentity.cpp:313` | `writeStaticFields` | 316 | `staticFields()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopMirCacheIdentity.cpp:332` | `writeDispatchTables` | 335, 358 | `dispatchTables(), interfaceTables` | 2 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopMirCacheIdentity.cpp:394` | `writeFieldInitializers` | 397 | `fieldInitializers()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopObjectGenerator.cpp:92` | `collectLineTableSourceRanges` | 98, 100, 102, 104, 106, 109 | `cleanupRegions, fieldInitializers(), functions(), members(),` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `LLVM/ProjectOopObjectGenerator.cpp:499` | `collectCacheFunctionOrdinals` | 505 | `functions()` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Linker/LldLinker.cpp:557` | `LldLinker::createImportLibrary` | 591 | `symbols` | 1 (1) | pasada única (por nombre; revisado en lote) |
| `Linker/WindowsRuntimeImports.cpp:269` | `materializeWindowsImportLibraries` | 284 | `symbols` | 1 (0) | pasada única (por nombre; revisado en lote) |
| `Linker/WindowsRuntimeLibrary.cpp:85` | `verifyWindowsRuntimeLibrary` | 96, 98 | `symbols` | 4 (0) | std::find por símbolo (S pequeño); aceptable |
| `Parser/PatternParser.cpp:1056` | `Parser::collectUniquePatternBindings` | 1062 | `bindings` | 1 (0) | pasada única (por nombre; revisado en lote) |


### Apéndice B · Ranking por función de copias, strings, búsquedas lineales y E/S dentro de bucles

**Cómo se generó.** A partir de la extracción con libclang (8515 funciones), solo en directorios del compilador. Columnas:

- **copias en bucle**: invocaciones a constructor o asignación **de copia** de tipos no triviales (strings, vectores, mapas, registros grandes) con profundidad de bucle ≥ 1, detectadas por AST;
- **strings en bucle**: `operator+`, `append`, `substr`, `to_string` y `ostringstream` con profundidad ≥ 1;
- **búsquedas lineales en bucle**: `std::find`, `find_if`, `any_of`, `all_of` y `none_of` (y sus niebloids `ranges::`) con profundidad ≥ 1;
- **key()/FQN en bucle**: llamadas a `TypeIdentity::key()` o `fullyQualifiedName()`, que con R-01 pasan a costar cero;
- **E/S en bucle**: `weakly_canonical`, `exists`, `is_regular_file`, `ifstream`, `sha256File`, etc.

Ordenado por un puntaje ponderado: E/S ×5, lineal ×3, key/FQN ×3, copia ×2, string ×1. Es un mapa de calor para revisar, **no** una lista de cambios obligatorios. Los cambios obligatorios son los de §2 y §3.

**Fuera del build normal.** Las primeras filas de `Driver/ProjectCommands.cpp` (`runClean`, `copyContentEntries`, `resolvePackage`, `inferProject`, `looksLikeConventionPackage`) y `ProjectCostExplorer` corresponden a comandos que no son `build` (clean, contenido, explorador de costos) o a la inferencia de proyecto. Se listan por completitud.

| # | Archivo:línea | Función | copias en bucle | strings en bucle | búsquedas lineales en bucle | key()/FQN en bucle | E/S en bucle |
|---|---|---|---|---|---|---|---|
| 1 | `Driver/ProjectCommands.cpp:1578` | `runClean` | 0 | 3 | 0 | 0 | 10 |
| 2 | `Driver/ProjectCommands.cpp:739` | `copyContentEntries` | 0 | 10 | 0 | 0 | 8 |
| 3 | `Driver/ProjectCommands.cpp:393` | `resolvePackage` | 0 | 0 | 0 | 0 | 9 |
| 4 | `Compilation/ProjectGraphLoader.cpp:188` | `preferExactSourceModuleArtifacts` | 3 | 0 | 1 | 0 | 7 |
| 5 | `Frontend/ProjectCostExplorer.cpp:250` | `projectCostReportText` | 0 | 44 | 0 | 0 | 0 |
| 6 | `Compilation/ProjectLoader.cpp:860` | `collectSourceCandidates` | 0 | 0 | 1 | 0 | 7 |
| 7 | `Interop/LibraryCapabilityAudit.cpp:112` | `auditLibraryCapabilityDirectory` | 0 | 3 | 0 | 0 | 7 |
| 8 | `Driver/ProjectCommands.cpp:228` | `inferProject` | 0 | 0 | 0 | 0 | 7 |
| 9 | `Driver/ProjectCommands.cpp:379` | `looksLikeConventionPackage` | 0 | 0 | 0 | 0 | 7 |
| 10 | `Compilation/ProjectGraph.cpp:198` | `collectUniquePrecompiledDescriptors` | 0 | 12 | 1 | 0 | 3 |
| 11 | `Parser/StatementParser.cpp:781` | `Parser::validateInlineAssemblyControlFlow` | 0 | 14 | 0 | 0 | 3 |
| 12 | `Driver/RuntimeClosurePackaging.cpp:55` | `packageRuntimeClosure` | 0 | 12 | 0 | 0 | 3 |
| 13 | `LLVM/CompilationArtifactCache.cpp:143` | `collectPrunableEntries` | 1 | 0 | 0 | 0 | 5 |
| 14 | `Driver/ToolchainContract.cpp:73` | `parseToolchainFields` | 0 | 21 | 0 | 0 | 1 |
| 15 | `Frontend/TemplateParseCache.cpp:206` | `TemplateParseCache::prune` | 0 | 0 | 0 | 0 | 5 |
| 16 | `Frontend/MemberModel/BuildSupport.cpp:316` | `ProjectMemberModel::bindPrecompiledPhysicalNames` | 3 | 15 | 0 | 1 | 0 |
| 17 | `Compilation/ProjectGraphLoader.cpp:262` | `ProjectGraphLoader::load` | 3 | 7 | 0 | 0 | 2 |
| 18 | `LLVM/CompilationArtifactCache.cpp:522` | `CompilationArtifactCache::prune` | 0 | 7 | 0 | 0 | 3 |
| 19 | `Interop/LibraryMetadataVerification.cpp:417` | `verifyMetadataReferenceAndDelegateContracts` | 0 | 21 | 0 | 0 | 0 |
| 20 | `Frontend/Bodies/LoopExpressionAnalyzer.cpp:109` | `ProjectMethodBodyModel::loopExpressionCommonType` | 3 | 4 | 0 | 0 | 2 |
| 21 | `Frontend/Bodies/MethodBodyBuildCoordinator.cpp:28` | `ProjectMethodBodyModel::initializeNamespaceCallableAdapters` | 10 | 0 | 0 | 0 | 0 |
| 22 | `Frontend/Bodies/PatternAnalyzer.cpp:626` | `ProjectMethodBodyModel::resolveMatchResultType` | 3 | 4 | 0 | 0 | 2 |
| 23 | `Compilation/ProjectLoader.cpp:927` | `loadProjectReferences` | 0 | 6 | 1 | 0 | 2 |
| 24 | `Frontend/MemberModel/TypeIdentity.cpp:263` | `ProjectMemberModel::canonicalGenericArgumentKey` | 1 | 3 | 0 | 3 | 1 |
| 25 | `Interop/LibraryMetadataVerification.cpp:475` | `verifyDispatchInterfaceMetadata` | 0 | 19 | 0 | 0 | 0 |
| 26 | `Compilation/ProjectLoader.cpp:1104` | `loadSdkReferences` | 1 | 8 | 1 | 0 | 1 |
| 27 | `Driver/LibraryReferenceValidation.cpp:55` | `validateLibraryReferenceSymbols` | 0 | 18 | 0 | 0 | 0 |
| 28 | `Driver/PrecompiledSurface/MetadataSourceMapping.cpp:396` | `validatePrecompiledExternalTypeClosure` | 0 | 18 | 0 | 0 | 0 |
| 29 | `Driver/ProjectCommands.cpp:534` | `materialize` | 0 | 3 | 0 | 0 | 3 |
| 30 | `Driver/ProjectCommands.cpp:1027` | `fingerprintFiles` | 0 | 3 | 0 | 0 | 3 |
| 31 | `Frontend/Bodies/StatementAnalyzer.cpp:909` | `ProjectMethodBodyModel::parseBlockMatchCoverage` | 0 | 3 | 0 | 0 | 3 |
| 32 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionTerminalFrames.cpp:112` | `ProjectOopHirModel::completeScheduledResultPropagationFrame` | 9 | 0 | 0 | 0 | 0 |
| 33 | `Interop/LibraryMetadataConstruction.cpp:855` | `appendMetadataDispatchTables` | 9 | 0 | 0 | 0 | 0 |
| 34 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionValueFrames.cpp:473` | `ProjectOopHirModel::completeScheduledNullCoalescingFrame` | 4 | 4 | 0 | 0 | 1 |
| 35 | `Frontend/ProjectNamespaceFunctionModel.cpp:1593` | `ProjectNamespaceFunctionModel::bindPrecompiledEscapeSummaries` | 1 | 12 | 1 | 0 | 0 |
| 36 | `Driver/ProjectCommands.cpp:1046` | `dependencyProvenance` | 0 | 11 | 0 | 0 | 1 |
| 37 | `Driver/PublicSurfaceBuilder.cpp:1359` | `PublicSurfaceBuilder::emitAliases` | 1 | 14 | 0 | 0 | 0 |
| 38 | `Interop/LibraryMetadataMerger.cpp:408` | `LibraryMetadataMerger::merge` | 0 | 16 | 0 | 0 | 0 |
| 39 | `Interop/LibraryMetadataVerification.cpp:1131` | `verifyMetadataCallableTypeReferences` | 0 | 16 | 0 | 0 | 0 |
| 40 | `LLVM/Lowering/OopLlvmInlineAssembly.cpp:188` | `OopLlvmImplementation::buildInlineAssemblyText` | 0 | 16 | 0 | 0 | 0 |
| 41 | `Compilation/SdkManifest.cpp:253` | `parseLibraries` | 0 | 2 | 1 | 0 | 2 |
| 42 | `Driver/PrecompiledModuleConfiguration.cpp:140` | `resolveBinaryLibraryReference` | 0 | 0 | 0 | 0 | 3 |
| 43 | `Driver/PrecompiledSurface/PrecompiledSourceGeneration.cpp:17` | `buildPrecompiledGenericTemplateSource` | 1 | 13 | 0 | 0 | 0 |
| 44 | `Driver/ProjectCommands.cpp:681` | `makeGeneratedProject` | 0 | 0 | 0 | 0 | 3 |
| 45 | `Frontend/Bodies/CallAnalyzer.cpp:5320` | `ProjectMethodBodyModel::configureMethodGroupLambdaTargets` | 2 | 0 | 2 | 0 | 1 |
| 46 | `Frontend/Generics/TraitImplementationMerger.cpp:130` | `ProjectGenericModel::genericConstraintShape` | 0 | 15 | 0 | 0 | 0 |
| 47 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionTerminalFrames.cpp:254` | `ProjectOopHirModel::completeScheduledStackAllocationFrame` | 2 | 11 | 0 | 0 | 0 |
| 48 | `Frontend/MIR/MirBuildCoordinator.cpp:564` | `ProjectOopMirModel::buildNamespaceStateFields` | 6 | 3 | 0 | 0 | 0 |
| 49 | `Interop/LibraryMetadataConstruction.cpp:992` | `appendSyntheticImplicitConstructors` | 3 | 3 | 0 | 2 | 0 |
| 50 | `Interop/LibraryMetadataMerger.cpp:270` | `canonicalizeNestedRecords` | 0 | 0 | 0 | 0 | 3 |
| 51 | `Interop/LibraryMetadataVerification.cpp:515` | `verifyDispatchSlots` | 0 | 15 | 0 | 0 | 0 |
| 52 | `LLVM/ProjectOopMirCacheIdentity.cpp:479` | `projectOopLineTableSourceIdentity` | 0 | 0 | 0 | 0 | 3 |
| 53 | `Linker/WindowsLibraryBuilder.cpp:717` | `verifyDynamicLibraryWithFindExports` | 0 | 2 | 1 | 0 | 2 |
| 54 | `Parser/Internal/StructuredControlParser.cpp:403` | `Parser::StructuredControlParser::finishDefer` | 5 | 5 | 0 | 0 | 0 |
| 55 | `Frontend/Bodies/ConversionAndPassKindChecks.cpp:208` | `ProjectMethodBodyModel::checkArgumentPassKinds` | 0 | 14 | 0 | 0 | 0 |
| 56 | `Frontend/ConstructionModel/ConstructorCandidateEvaluation.cpp:302` | `ProjectConstructionModel::explainOwnershipRejection` | 0 | 9 | 0 | 0 | 1 |
| 57 | `Frontend/Generics/GenericNodeProcessor.cpp:94` | `ProjectGenericModel::prepareNodeScope` | 4 | 3 | 1 | 0 | 0 |
| 58 | `Frontend/Generics/TemplateDependencyCollector.cpp:137` | `ProjectGenericModel::discoverTemplateDependencies` | 7 | 0 | 0 | 0 | 0 |
| 59 | `Frontend/MemberModel/BuildSupport.cpp:110` | `ProjectMemberModel::validateCopyType` | 0 | 8 | 0 | 2 | 0 |
| 60 | `Interop/LibraryMetadataVerification.cpp:228` | `verifyMetadataAssociatedItemContracts` | 0 | 14 | 0 | 0 | 0 |
| 61 | `Interop/LibraryMetadataVerification.cpp:1249` | `verifyNamespaceAndExportCatalogues` | 0 | 14 | 0 | 0 | 0 |
| 62 | `Frontend/NameLookup/TypeVisibility.cpp:37` | `ProjectNameLookup::typeIsVisibleTo` | 5 | 0 | 1 | 0 | 0 |
| 63 | `Interop/CHeaderGenerator.cpp:150` | `buildHostTypeNames` | 1 | 5 | 0 | 2 | 0 |
| 64 | `Interop/CHeaderGenerator.cpp:253` | `collectReachableHostTypes` | 0 | 0 | 0 | 1 | 2 |
| 65 | `Interop/LibraryMetadataConstruction.cpp:1027` | `includeGenericTemplateClosure` | 0 | 13 | 0 | 0 | 0 |
| 66 | `Compilation/ProjectGraphLoader.cpp:140` | `normalizeTransitiveSourceOwnership` | 1 | 0 | 0 | 0 | 2 |
| 67 | `Compilation/SdkManifest.cpp:99` | `parseMappings` | 0 | 12 | 0 | 0 | 0 |
| 68 | `Driver/PrecompiledModuleConfiguration.cpp:203` | `expandPrecompiledBinaryClosure` | 3 | 6 | 0 | 0 | 0 |
| 69 | `Frontend/Bodies/ConversionAndPassKindChecks.cpp:83` | `ProjectMethodBodyModel::reportTupleConversionFailure` | 0 | 7 | 0 | 0 | 1 |
| 70 | `Frontend/Bodies/IdentifierAnalyzer.cpp:405` | `ProjectMethodBodyModel::applyBorrowedParameterReassignment` | 0 | 0 | 4 | 0 | 0 |
| 71 | `Frontend/Bodies/PatternAnalyzer.cpp:185` | `ProjectMethodBodyModel::decodeMatchCoverageArm` | 0 | 2 | 0 | 0 | 2 |
| 72 | `Frontend/ComptimeEngine.cpp:424` | `reproducibilityDigestOf` | 0 | 12 | 0 | 0 | 0 |
| 73 | `Frontend/Generics/GenericCloner.cpp:252` | `ProjectGenericModel::cloneProjectionType` | 1 | 10 | 0 | 0 | 0 |
| 74 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionAssignmentFrames.cpp:722` | `ProjectOopHirModel::completeScheduledCompoundAssignmentFrame` | 1 | 5 | 0 | 0 | 1 |
| 75 | `Frontend/HIR/HirExpressionMachine.cpp:2591` | `ProjectOopHirModel::lowerLambdaCreationExpressionImplCase` | 6 | 0 | 0 | 0 | 0 |
| 76 | `Frontend/MacroExpansionEngine.cpp:208` | `expandMacroInvocations` | 1 | 10 | 0 | 0 | 0 |
| 77 | `Frontend/ProjectNamespaceFunctionModel.cpp:1382` | `ProjectNamespaceFunctionModel::walkNamespaceScope` | 4 | 1 | 1 | 0 | 0 |
| 78 | `Interop/LibraryCapabilityAudit.cpp:97` | `libraryCapabilitySurfaceReport` | 0 | 12 | 0 | 0 | 0 |
| 79 | `Interop/LibraryMetadataVerification.cpp:1553` | `verifyLifetimeMetadataContracts` | 0 | 12 | 0 | 0 | 0 |
| 80 | `LLVM/Lowering/OopLlvmDeclarations.cpp:202` | `OopLlvmImplementation::declareDestructionEntries` | 0 | 6 | 0 | 2 | 0 |
| 81 | `LLVM/Lowering/OopLlvmStaticState.cpp:184` | `OopLlvmImplementation::declareTypeInitializers` | 0 | 6 | 0 | 2 | 0 |
| 82 | `Linker/WindowsLibraryBuilder.cpp:341` | `verifyDynamicLibraryWithNestedExports` | 0 | 2 | 0 | 0 | 2 |
| 83 | `Driver/PrecompiledSurface/MetadataSourceMapping.cpp:490` | `sourceTypeForGenericSpecialization` | 1 | 6 | 1 | 0 | 0 |
| 84 | `Frontend/Bodies/ExpressionCore/CollectionExpressions.cpp:362` | `ProjectMethodBodyModel::validateCollectionSpreadSegments` | 0 | 6 | 0 | 0 | 1 |
| 85 | `Frontend/Bodies/PatternAnalyzer.cpp:255` | `ProjectMethodBodyModel::collectMatchDomainNames` | 0 | 3 | 1 | 0 | 1 |
| 86 | `Frontend/Bodies/StatementAnalyzer.cpp:972` | `ProjectMethodBodyModel::blockMatchCoverageDomain` | 0 | 3 | 1 | 0 | 1 |
| 87 | `Frontend/Bodies/StatementAnalyzer.cpp:3839` | `ProjectMethodBodyModel::createVariableLengthArrayExtentLocals` | 3 | 5 | 0 | 0 | 0 |
| 88 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionValueFrames.cpp:553` | `ProjectOopHirModel::completeScheduledNullCoalescingAssignmentFrame` | 1 | 4 | 0 | 0 | 1 |
| 89 | `Frontend/MacroExpansionEngine.cpp:65` | `splitArguments` | 0 | 1 | 0 | 0 | 2 |
| 90 | `Frontend/MacroExpansionEngine.cpp:145` | `collectMacroDefinitions` | 0 | 5 | 2 | 0 | 0 |
| 91 | `Frontend/MemberModel/BuildAndDeclaration.cpp:695` | `ProjectMemberModel::collectImportedCLibraries` | 1 | 6 | 1 | 0 | 0 |
| 92 | `Interop/LibraryApiDiff.cpp:296` | `appendSourceGenericTemplates` | 0 | 1 | 0 | 0 | 2 |
| 93 | `Basic/ArtifactTransaction.cpp:374` | `ArtifactTransaction::rollbackCommit` | 0 | 0 | 0 | 0 | 2 |
| 94 | `Compilation/PartialTypeAggregator.cpp:108` | `PartialTypeAggregator::mergeGroup` | 1 | 8 | 0 | 0 | 0 |
| 95 | `Driver/ProjectCommands.cpp:517` | `verifyPlan` | 0 | 5 | 0 | 0 | 1 |
| 96 | `Frontend/Bodies/CallAnalyzer.cpp:3881` | `ProjectMethodBodyModel::bindDelegateInvocationArguments` | 0 | 0 | 0 | 0 | 2 |
| 97 | `Frontend/Bodies/ConstructionAnalyzer.cpp:178` | `ProjectMethodBodyModel::aggregateSupportsSynthesizedMemberVisit` | 2 | 6 | 0 | 0 | 0 |
| 98 | `Frontend/Bodies/OverloadCandidateRanking.cpp:57` | `ProjectMethodBodyModel::betterCandidate` | 0 | 0 | 0 | 0 | 2 |
| 99 | `Frontend/ComptimeEngine.cpp:42` | `trim` | 0 | 0 | 0 | 0 | 2 |
| 100 | `Frontend/ComptimeEngine.cpp:351` | `resolveCapabilityCalls` | 0 | 10 | 0 | 0 | 0 |
| 101 | `Frontend/Generics/TypeDemandScheduler.cpp:93` | `ProjectGenericModel::collectBodyTypeSpecializationDemand` | 0 | 10 | 0 | 0 | 0 |
| 102 | `Frontend/MacroExpansionEngine.cpp:288` | `macroExpansionBacktrace` | 0 | 10 | 0 | 0 | 0 |
| 103 | `Frontend/MemberModel/BuildAndDeclaration.cpp:187` | `collectDelegateInputLifetimes` | 1 | 8 | 0 | 0 | 0 |
| 104 | `Frontend/ProjectCompilation.cpp:1156` | `ProjectCompilation::precompiledProjectOrdinal` | 0 | 0 | 0 | 0 | 2 |
| 105 | `Frontend/ProjectCostExplorer.cpp:283` | `projectCostReportJson` | 0 | 10 | 0 | 0 | 0 |
| 106 | `Frontend/ProjectInterfaceModel.cpp:359` | `ProjectInterfaceModel::collectInterfaceBases` | 0 | 4 | 0 | 2 | 0 |
| 107 | `Frontend/ProjectMetaprogrammingModel.cpp:60` | `ProjectMetaprogrammingModel::appendStaticType` | 5 | 0 | 0 | 0 | 0 |
| 108 | `Frontend/ProjectTypeConversion.cpp:496` | `ProjectTypeConversion::commonNumericType` | 0 | 0 | 0 | 0 | 2 |
| 109 | `Frontend/TemplateParseCache.cpp:68` | `evictOldestEntries` | 0 | 0 | 0 | 0 | 2 |
| 110 | `Interop/LibraryMetadataVerification.cpp:944` | `collectVerifiedExternalTypes` | 1 | 8 | 0 | 0 | 0 |
| 111 | `Interop/LibraryMetadataVerification.cpp:1315` | `verifyBinaryClosureMetadata` | 1 | 8 | 0 | 0 | 0 |
| 112 | `LLVM/Lowering/OopLlvmCleanup.cpp:96` | `OopLlvmImplementation::emitExceptionalCleanupChain` | 4 | 2 | 0 | 0 | 0 |
| 113 | `LLVM/ProjectOopObjectGenerator.cpp:516` | `composeModuleCacheKeys` | 5 | 0 | 0 | 0 | 0 |
| 114 | `LLVM/ProjectOopObjectGenerator.cpp:701` | `thinLtoIndexDigest` | 0 | 5 | 0 | 0 | 1 |
| 115 | `Linker/Internal/ScopedTemporaryDirectory.cpp:10` | `ScopedTemporaryDirectory::create` | 1 | 8 | 0 | 0 | 0 |
| 116 | `Linker/WindowsLibraryBuilder.cpp:101` | `cachedFileSetIdentity` | 0 | 5 | 0 | 0 | 1 |
| 117 | `Parser/PatternParser.cpp:1308` | `Parser::PatternBindingEmitter::emitTuple` | 2 | 6 | 0 | 0 | 0 |
| 118 | `Driver/LibraryReferenceValidation.cpp:391` | `verifyLinkedRuntimeLibraries` | 2 | 5 | 0 | 0 | 0 |
| 119 | `Driver/PrecompiledModuleConfiguration.cpp:92` | `loadExplicitPrecompiledMetadata` | 0 | 9 | 0 | 0 | 0 |
| 120 | `Frontend/Bodies/ConstructionAnalyzer.cpp:434` | `ProjectMethodBodyModel::reserveNamedAggregateTargets` | 0 | 6 | 1 | 0 | 0 |
| 121 | `Frontend/Bodies/MethodBodyBuildCoordinator.cpp:96` | `ProjectMethodBodyModel::validateDeclaredErrorTypes` | 0 | 4 | 0 | 0 | 1 |
| 122 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionAssignmentFrames.cpp:617` | `ProjectOopHirModel::completeScheduledDirectAssignmentFrame` | 2 | 0 | 0 | 0 | 1 |
| 123 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionValueFrames.cpp:221` | `ProjectOopHirModel::completeScheduledRangeSliceFrame` | 2 | 0 | 0 | 0 | 1 |
| 124 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionValueFrames.cpp:617` | `ProjectOopHirModel::completeScheduledDeconstructionAssignmentFrame` | 2 | 0 | 0 | 0 | 1 |
| 125 | `Frontend/MacroExpansionEngine.cpp:101` | `instantiate` | 0 | 9 | 0 | 0 | 0 |
| 126 | `Frontend/NameLookup/TypeAggregation.cpp:444` | `ProjectNameLookup::finalizeContainingTypeOrdinals` | 3 | 0 | 0 | 1 | 0 |
| 127 | `Frontend/ProjectVirtualDispatchModel.cpp:404` | `ProjectVirtualDispatchModel::validateConcreteVirtualSlots` | 1 | 4 | 0 | 1 | 0 |
| 128 | `Interop/CHeaderGenerator.cpp:785` | `emitOrderedDefinitions` | 0 | 0 | 0 | 3 | 0 |
| 129 | `Interop/LibraryMetadataConstruction.cpp:538` | `collectTemplateSourceReplacements` | 2 | 5 | 0 | 0 | 0 |
| 130 | `Interop/LibraryMetadataVerification.cpp:829` | `verifyGenericTemplateClosure` | 0 | 9 | 0 | 0 | 0 |
| 131 | `Interop/LibraryMetadataVerification.cpp:1080` | `verifyMetadataTypeReferences` | 0 | 9 | 0 | 0 | 0 |
| 132 | `LLVM/LlvmOptimizationLevel.cpp:50` | `llvmOptimizationContractManifest` | 0 | 9 | 0 | 0 | 0 |
| 133 | `LLVM/LlvmOptimizationObservation.cpp:117` | `splitLlvmProcessTrace` | 0 | 9 | 0 | 0 | 0 |
| 134 | `Compilation/SdkManifest.cpp:398` | `parseManifestChildren` | 0 | 8 | 0 | 0 | 0 |
| 135 | `Driver/Internal/ProjectCommandSupport.cpp:7` | `jsonEscape` | 0 | 8 | 0 | 0 | 0 |
| 136 | `Frontend/Bodies/CallAnalyzer.cpp:4862` | `ProjectMethodBodyModel::validateThreadExecutorCaptures` | 0 | 8 | 0 | 0 | 0 |
| 137 | `Frontend/Bodies/EscapeAnalyzer.cpp:278` | `ProjectMethodBodyModel::validateEscapeCall` | 0 | 8 | 0 | 0 | 0 |
| 138 | `Frontend/Bodies/LambdaDelegateBinder.cpp:185` | `ProjectMethodBodyModel::explicitLambdaCaptureModes` | 0 | 3 | 0 | 0 | 1 |
| 139 | `Frontend/Bodies/MemberAccessAnalyzer.cpp:391` | `ProjectMethodBodyModel::analyzeNativeStringFacadeMember` | 0 | 0 | 0 | 1 | 1 |
| 140 | `Frontend/Bodies/StatementAnalyzer.cpp:1431` | `ProjectMethodBodyModel::analyzeIfStatement` | 4 | 0 | 0 | 0 | 0 |
| 141 | `Frontend/Generics/AssociatedItemCollector.cpp:352` | `ProjectGenericModel::validateAssociatedRequirements` | 0 | 8 | 0 | 0 | 0 |
| 142 | `Frontend/Generics/Support/GenericPatterns.cpp:69` | `genericTypePattern` | 0 | 8 | 0 | 0 | 0 |
| 143 | `Frontend/Generics/TemplateCollector.cpp:109` | `ProjectGenericModel::collectEnumTemplateData` | 1 | 6 | 0 | 0 | 0 |
| 144 | `Frontend/Generics/TemplateDependencyCollector.cpp:102` | `ProjectGenericModel::collectDependencyImports` | 3 | 2 | 0 | 0 | 0 |
| 145 | `Frontend/Generics/TemplateDependencyCollector.cpp:317` | `ProjectGenericModel::collectTemplateDependencies` | 4 | 0 | 0 | 0 | 0 |
| 146 | `Frontend/Generics/TypeSpellingAndVisibility.cpp:129` | `ProjectGenericModel::typeSpelling` | 0 | 8 | 0 | 0 | 0 |
| 147 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionAccessFrames.cpp:399` | `ProjectOopHirModel::finishArrayRawIndex` | 4 | 0 | 0 | 0 | 0 |
| 148 | `Frontend/HIR/ExpressionStatementMachine/HirCompletionAssignmentFrames.cpp:555` | `…jectOopHirModel::completeScheduledNullConditionalArrayAssignmentFrame` | 4 | 0 | 0 | 0 | 0 |
| 149 | `Frontend/HIR/ExpressionStatementMachine/HirDeclarationFrameLowering.cpp:558` | `ProjectOopHirModel::advanceInlineAssemblyFrame` | 4 | 0 | 0 | 0 | 0 |
| 150 | `Frontend/HIR/HirAggregateLowering.cpp:275` | `ProjectOopHirModel::appendAggregateLeaves` | 1 | 6 | 0 | 0 | 0 |


---

## 6. Snippet para el agente implementador (SUSTITUIDO por `NSC-TRACKING-RENDIMIENTO.md`; se conserva como referencia histórica)

```text
Implementa las optimizaciones de NSC-ANALISIS-RENDIMIENTO-POR-FUNCION.md sobre development (con el refactor
modular ya aplicado). Reglas no negociables:
- Equivalencia funcional EXACTA: mismo IR, mismos .obj, mismo orden de funciones y de diagnosticos. Cuando un
  indice sustituye una busqueda lineal, conserva "gana el primero por ordinal" (try_emplace en orden).
- No renombres ni cambies el tipo de miembros que usan las pruebas (Parser::knownTypeNames_,
  ProjectOopMirModel::structLayouts_/functions_, campos HIR...). AÑADE miembros nuevos.
- Una clase por archivo, Allman, 120 columnas, <=60 lineas ejecutables por funcion, sin recursion nueva, sin
  lambdas nuevas, NUNCA borres comentarios (si un comentario describe codigo que cambias, conservalo y anade
  debajo "// PERF (NSC-PERF-xx): ..." bilingue ES/EN).
- Antes de cambiar una firma o un miembro existente: grep en tests/.
- Lineas citadas = copia del 2026-10-19; si no cuadran, busca por nombre de funcion.

Orden por lotes (compila y corre la suite completa al final de CADA lote):
LOTE 1
  R-01  TypeIdentity: cachear fullyQualifiedName_/key_ en el ctor; key()/fullyQualifiedName() devuelven
        const std::string& (TypeIdentity.h:106-107, TypeIdentity.cpp:76-89,126-158).
  R-09  Constantes kColdLlvmTraceEnabled/kLoweringPhaseTraceEnabled/kRuntimeDebugTraceEnabled=false
        (nuevo LLVM/Internal/LlvmInstrumentationSwitches.h); ColdLlvmTrace.cpp:22-23, OopLlvmImplementation.cpp:
        166-189, OopLlvmRuntimeDebugTrace.cpp:13.
  R-05.1 llvmType (OopLlvmTypesAbiAttributes.cpp:11-19) -> return mapLlvmType(...) sin pruebas duplicadas.
  R-07.1 Sha256.cpp: sha256 sin copiar la entrada (bloques directos + cola de 64/128) y sha256File por bloques
        de 64 KiB. Prueba nueva: igualdad con la version vieja para longitudes 0..300 y 1 MiB aleatorio.
  R-08.3 PeExecutableVerifier.cpp:121-122 lectura en bloque.
LOTE 2
  R-02  ProjectNameLookup::typeOrdinalForIdentityKey/typeOrdinalForTypeKey (indice construido tras
        finalizePredeclarations, ProjectNameLookup.cpp:94; fallback lineal si no se construyo). Sustituir los 52
        sitios de la tabla R-02 (patron A o B).
  R-04.2 ProjectObjectLayoutModel::fieldLayoutByMember_ (misma precedencia struct>class>promoted), construido
        justo antes de built_=true (linea 122); fallback lineal.
  R-04.3 ProjectOopMirModel::structLayoutForTypeKey/classLayoutForTypeKey (indice antes de lowerFunctions,
        MirBuildCoordinator.cpp:657); findStructLayout/findClassLayout y abiPassingForType/
        sumAwaitedFrameSizeNodes (MirFunctionLowerer.cpp:256-310) lo usan.
  R-05.2 memo llvmTypeCache_ en OopLlvmImplementation (solo resultados no nulos).
LOTE 3
  R-03.1 HIR: resolver en el coordinador skip/precompilados/importedC sin worker (extraer
        makePrecompiledRegularFunction/makeImportedCRegularFunction de HirBuildCoordinator.cpp:878-966);
        buildImpl no dimensiona functionByMember_/functionByNamespaceFunction_ en modo worker (2068-2070);
        recordFunctionForMember en 921/963/1479; buildRegularFunctionsSequentially con filtro directo (1538-1548).
  R-03.2 Indices localOrdinalsByCallable_/localOrdinalsByLambda_/parameterCleanupOrdinalsByCallable_ en
        indexBuildBindings; sustituir 1015-1020, 1029-1036, 1590-1600, 1666-1671, 1796-1802.
  R-04.1 MIR: sharedStructLayouts_ + activeStructLayouts(); MirFunctionLowerer.cpp:844 y MirNodeMachine.cpp:
        1593/1626.
LOTE 4
  R-07.2 Procedencia solo con options.dependencyReport (ProjectCommands.cpp:1455-1456).
  R-07.3/7.4/7.5 Restore, cargas de proyecto y NSMETA una sola vez por proceso (ver documento).
  R-08.1 Cache de runtime.obj (patron de WindowsLibraryBuilder.cpp:490-585).
  R-08.2 Cache por contenido de import libs (sin lanzar NSLink en regimen estable).
LOTE 5
  R-06.1 declaredTypeOrdinalsBySourceName_ (TemplateCollector.cpp:165; TypeSpellingAndVisibility.cpp:233,295).
  R-06.2 Cow perezoso (ProjectGenericModel.h:384-395).
  R-06.3 No copiar children() (6 sitios) salvo donde el bucle muta el padre.
  R-06.6/6.7/6.8, R-11.1..11.4.
LOTE 6
  R-10 (PublicSurfaceBuilder/ContractMaterialization/LibraryMetadataVerification), R-13 (membersDeclaredByType,
  tipos de Core precalculados por modelo), R-14 (bindPrecompiledPhysicalNames, escape summaries).
LOTE 7 (diseno; pedir confirmacion a Alberto antes)
  R-06.4 (no clonar tipos hoja), R-06.5 (banderas por unidad / cache de superficie), R-10.1 (cache del texto de
  superficie), R-11.5 (partir superficie de Core), R-12 (arena real y nodo AST compacto), R-08.5.

Al terminar cada lote: medir segun §4 (host en /O2) y anotar antes/despues.
```