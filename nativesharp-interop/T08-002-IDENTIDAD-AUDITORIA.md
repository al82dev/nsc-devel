# T08-002: identidad de las declaraciones C importadas

Auditoría del carril de identidad (rama `interop-t08-claude-identidad`, base
`ceac3f83b`). Recorre cómo NSC obtiene y representa la identidad de funciones,
variables globales, alias, enumeraciones, `struct` y `union` C importadas desde
Clang, y cómo la conserva en importación, análisis, metadata, biblioteca
precompilada y consumo. Separa lo comprobado con evidencia de lo incompleto o no
verificado. **T08-002 permanece abierto; la tanda 08 no se cierra.**

Clasificación de afirmaciones: HECHO COMPROBADO (código o ejecución citados),
INFERENCIA (derivada y declarada como tal) y NO VERIFICADO.

## SCOPE LOCK

- **Objetivo:** auditar la identidad de declaraciones C importadas (T08-002),
  preparar pruebas dirigidas de identidad y de su conservación al generar y
  consumir una biblioteca precompilada, y registrar resultados y pendientes.
- **Naturaleza:** pruebas, validador y documentación. Producción solo ante un
  defecto demostrado y, si la función es compartida, con autorización explícita
  de alcance transversal.
- **Comportamiento que debe permanecer idéntico:** todo el compilador, el
  formato NSMETA, los fixtures y las pruebas existentes.
- **Revisión conocida:** `ceac3f83b` (`interop-t08-corregir-entrada-crt-control-cache`).
- **Rango evaluado:** `ceac3f83b..interop-t08-claude-identidad`.
- **Archivos modificables:** `tests/unit/CppCIdentityTests.cpp` (nuevo);
  `CMakeLists.txt` (solo el alta del objetivo nuevo `nsc_cpp_c_identity_tests`);
  `nativesharp-interop/pruebas-tanda-08-identidad-c/validar.cmd` (nuevo); este
  documento; `nativesharp-interop/registro-commits/t08-002-*.md` (nuevos); una
  sección en `SCOPE-LOCK-EJECUCION.md`; viñetas bajo T08-002 en `TRACKING.md`.
- **Protegidos:** todo `src/**` e `include/**`; NSLink; fuentes de LLVM/Clang;
  `tests/unit/CppClangSessionTests.cpp`; los fixtures existentes, incluido
  `pruebas-tanda-08-meta-c/*` (solo se copian, no se modifican);
  `validar-tanda-08.cmd`; el formato NSMETA; la unidad C:.
- **Pruebas que deben pasar sin modificación:** todas las existentes. Este carril
  no ejecuta la batería completa: la integración y la validación de la tanda
  corresponden a Codex.
- **Acciones prohibidas:** avanzar a la tanda 9; declarar cerrada la tanda 8;
  marcar T08-002 sin evidencia completa; trabajar en Daikon, .NET o C++ que
  consume NativeSharp; desplegar en C:; reconstruir todo por cada ítem; modificar
  NSLink o LLVM/Clang; usar PowerShell o shells distintos de CMD.
- **Toolchain:** Clang 23.1.0 de `A:\al82dev-clang` para NSC y para el C de
  referencia; build propio en `A:\nsc-devel\build-claude-t08\build-ON`.
- **Criterio de terminación del carril:** informe entregado con rama, commits,
  archivos, pruebas, resultados, conflictos y dependencias; T08-002 abierto.

## Forma de la identidad

HECHO COMPROBADO: la identidad estable que NSC captura es el USR de Clang de la
declaración canónica, opcional y opaco. Su ausencia no se sustituye por una
identidad inventada. Campos que lo transportan:

| Etapa | Estructura | Campo |
|---|---|---|
| Captura | `CppFunctionDeclaration` | `clangUsr` (`CppFunctionDeclaration.h:44`) |
| Captura | `CppGlobalDeclaration` | `clangUsr` (`CppGlobalDeclaration.h:25`) |
| Captura | `CppTypeAliasDeclaration` | `clangUsr` (`CppTypeAliasDeclaration.h:20`) |
| Captura | `CppRecordDeclaration`, `CppRecordField` | `clangUsr` (`CppRecordDeclaration.h:24,117`) |
| Captura | `CppEnumDeclaration`, `CppEnumeratorDeclaration` | `clangUsr` (`CppEnumDeclaration.h:23,39`) |
| Captura | `CppType` (identidad de record/enum referido) | `clangUsr` (`CppType.h:27,35`) |
| Análisis | `NamespaceFunctionSymbol` | `cppClangUsr` |
| Análisis | `NamespaceStateSymbol` (global y enumerador) | `cppClangUsr` |
| Análisis | `ClassMemberSymbol` (campo) | `cppClangUsr` (`ProjectMemberModel.cpp:460`) |
| Análisis | `CppImportedTypeDeclaration` (struct, union, enum) | `clangUsr` |
| Metadata | `CppCRecordSnapshot`, `CppCEnumSnapshot` | `clangUsr` del record, campos, enum y enumeradores |

## Recorrido por etapa

### Importación (sesión Clang)

HECHO COMPROBADO: `ClangSession.cpp` pide el USR a la API oficial de Clang al
crear cada snapshot (registro `t08-captura-usr-clang.md`). Las formas observadas
con Clang 23.1.0 están en [resultados](registro-commits/t08-002-resultados-identidad-c.md).

### Análisis (selección y agrupación)

HECHO COMPROBADO por lectura de código:

- Funciones: `CppSemanticFunctionGroups.cpp` agrupa por símbolo externo, rechaza
  USR contradictorios en un grupo y detecta declaraciones del mismo USR con
  símbolo distinto (líneas 52-129, 191-219).
- Globales: `CppSemanticInventory.cpp` recorre alias por símbolo o USR y rechaza
  USR conocidos distintos (líneas 376-448); el USR llega al `NamespaceStateSymbol`.
- Enums y enumeradores: conciliación de USR al unir observaciones
  (`CppSemanticInventory.cpp`, líneas 617-747 y 872-917).
- Records: el agrupador marca conflicto con USR conocidos distintos y transporta
  el USR al tipo importado (registro `t08-usr-grupos-record.md`).
- Alias: `QualifiedTypeNames.cpp` (líneas 208-236) exige el mismo nombre, origen,
  destino y tipo canónico, y desde `847a71ccb` también trata USR conocidos
  distintos como ambigüedad.

### Metadata y biblioteca precompilada

HECHO COMPROBADO por lectura de código: el catálogo C del NSMETA
(`CppCRecordMetadataCodec.cpp`) guarda el USR del record (línea 329), de cada
campo (343), del enum (509), de cada enumerador (520) y de las identidades de
tipos anidados (106, 114). El productor publica solo records y enums alcanzables
desde la superficie pública NativeSharp y su cierre transitivo
(`provideCppCRecordMetadataCatalog`, `CppSemanticRecord.cpp:960-1107`).

HECHO COMPROBADO: el catálogo **no** contiene funciones, globales ni alias C.
Un consumidor que necesita esas declaraciones las obtiene de su propia sesión
Clang con su propio `CppLibraryReference`.

### Consumo

HECHO COMPROBADO por lectura de código: con un catálogo precompilado, el driver
instala el selector de tipos y los proveedores de campos y valores
(`PrecompiledModuleConfiguration.cpp:545-563`). `reconstructCImportedTypes` copia
el USR al tipo importado (codec, líneas 811 y 842); `reconstructCImportedFields`
exige que el tipo instalado tenga el mismo USR que el catálogo
(`CppSemanticRecord.cpp:1148`); `reconstructCEnumConstants` copia el USR de cada
enumerador al símbolo (`CppSemanticRecord.cpp:1212`).

## Estado por categoría

Estado de la conservación al generar y consumir una biblioteca precompilada.
La columna de evidencia remite al [registro de resultados](registro-commits/t08-002-resultados-identidad-c.md).

| Categoría | Captura y análisis | NSMETA (productor real) | Restauración en el consumidor | Estado |
|---|---|---|---|---|
| struct C con etiqueta | Probado (suite previa y sonda 1) | Probado previamente (`t08-usr-productor-consumidor-real.md`) | Probado O0–O3 (este carril) | PENDIENTE: validación integral |
| union C | Probado (sonda 1) | Probado O0–O3 (este carril) | Probado O0–O3 (este carril) | PENDIENTE: validación integral |
| campos de struct y union | Probado | Probado | Probado O0–O3 (este carril) | PENDIENTE: validación integral |
| enum C con etiqueta | Probado | Probado O0–O3 (este carril) | Probado O0–O3 (este carril) | PENDIENTE: validación integral |
| enumerador | Probado | Probado O0–O3 (este carril) | Probado O0–O3 (este carril) | PENDIENTE: validación integral |
| struct nombrado solo por typedef | Probado (sonda 1, `c:@SA@`) | Probado O0–O3 | Probado O0–O3, también por el driver | PENDIENTE: validación integral |
| enum anónimo nombrado por typedef | Probado (`c:@EA@`) | Probado O0–O3 | Probado O0–O3, también por el driver | PENDIENTE: validación integral |
| enum anónimo sin typedef | Probado (sonda 1, `c:@Ea@`) | No publicable: no tiene nombre de tipo para la superficie | No aplica | PENDIENTE: el consumidor lo obtiene de su sesión |
| enum anónimo como tipo de campo publicado | NO VERIFICADO (USR) | El productor falla (H4) | No aplica | PENDIENTE: H4 |
| función externa | Probado (sondas 1 y 2) | No forma parte del catálogo | Falla si la usa una plantilla pública | PENDIENTE: propuesta H3 |
| función `static inline` | Probado (sondas 1, 2 y 5) | No forma parte del catálogo | Falla si la usa una plantilla pública | PENDIENTE: propuesta H3 |
| global externa | Probado (sondas 1 y 2) | No forma parte del catálogo | Falla si la usa una plantilla pública | PENDIENTE: propuesta H3 |
| global `static` | Probado (sondas 1 y 5); NSC no la importa | No forma parte del catálogo | No aplica | PENDIENTE (T08-004) |
| alias `typedef` | Probado (sondas 1, 3 y 5) | No forma parte del catálogo | Falla si lo usa una plantilla pública | PENDIENTE: propuestas H1 y H3 |

La restauración se prueba de dos formas para cada productor: con el módulo
precompilado armado por la prueba y por la ruta completa del driver
(`LibraryReference` real y `driver::configurePrecompiledModules`). Esta última no
ejecuta la generación de código ni el enlace del consumidor; esos pasos los
cubre `pruebas-tanda-08-meta-c/validar.cmd`.

## Colisiones y declaraciones equivalentes

HECHO COMPROBADO con Clang 23.1.0 (sonda 1): según la categoría, el USR depende
o no del nombre de archivo del encabezado.

- **No dependen del encabezado:** función y global externas (`c:@F@f`, `c:@g`),
  struct, union y enum con etiqueta (`c:@S@R`, `c:@U@U`, `c:@E@E`), campos
  (`c:@S@R@FI@f`), enumeradores (`c:@E@E@X`), struct nombrado solo por typedef
  (`c:@SA@T`) y enum anónimo, que toma el nombre de su primer enumerador
  (`c:@Ea@X@X`).
- **Dependen del nombre del encabezado:** alias `typedef` (`c:a.h@T@T`),
  función `static inline` (`c:a.h@F@f`) y global `static` (`c:a.h@g`).

Consecuencias:

1. HECHO COMPROBADO: dos encabezados distintos con la misma declaración externa
   o con la misma etiqueta producen el mismo USR. NSC une la función externa en
   un grupo, selecciona la global externa y agrupa el struct sin conflicto
   (sonda 2). INFERENCIA: un USR igual no prueba que la declaración proceda del
   mismo encabezado; para records, la procedencia (archivo, offset y huella) ya
   rechaza una copia en otra ruta o un encabezado modificado (registros
   `prueba-opacos-header-compartido.md` y `redeclaraciones-opacas-c.md`).
2. HECHO COMPROBADO: dos enums anónimos distintos con el mismo primer
   enumerador comparten USR (`c:@Ea@CIdShared@CIdShared`) aunque sus valores
   difieran. NSC no los une: marca el valor con el error diferido
   `conflicting C enumerator declarations` (sonda 4).
3. HECHO COMPROBADO: las funciones `static inline` idénticas de dos encabezados
   forman dos grupos y la global `static` no produce candidato externo
   (sonda 2). Es coherente con entidades internas distintas. NO VERIFICADO: el
   diagnóstico al usar ese nombre desde NativeSharp.
4. HECHO COMPROBADO (sonda 5 y fixture `homonimos`): el USR dependiente del
   archivo usa solo el nombre base. `dirA/same.h`, `dirB/same.h` y
   `dirC/same.h` dan el mismo USR para su typedef, `static inline` y global
   `static`, aunque `dirC` declare otros tipos y valores. NSC no une por ello
   declaraciones distintas: el typedef de otro tipo se rechaza por su tipo
   canónico y las funciones `static inline` siguen siendo dos definiciones
   (llamada ambigua). El typedef del mismo tipo en `dirA` y `dirB` enlaza.

## Hallazgos

### H1. Alias `typedef` idéntico en dos encabezados rechazado por su USR

```text
HALLAZGO NO AUTORIZADO
Evidencia: sonda 3. Dos sesiones del mismo proyecto incluyen a.h y b.h con
  typedef int CIdInt; idéntico. Con a.h + a.h el nombre enlaza; con a.h + b.h
  se informa "conflicting C/C++ type alias 'CIdInt'".
Causa: el USR de un typedef C incluye el nombre del encabezado (c:a.h@T@CIdInt
  frente a c:b.h@T@CIdInt). La comprobación añadida en 847a71ccb
  (QualifiedTypeNames.cpp:230-236) trata USR conocidos distintos como
  ambigüedad, aunque el nombre y el tipo canónico coinciden.
Diferencial: antes de 847a71ccb solo se comparaban nombre, origen, destino y
  tipo canónico (git show 847a71ccb). INFERENCIA: ese caso enlazaba antes.
  NO VERIFICADO: no se construyó la revisión anterior.
Riesgo: un proyecto válido con dos bibliotecas C que repiten un typedef común
  deja de compilar.
Componentes afectados: resolución de nombres (ProjectNameLookup), que es una
  función compartida de alto impacto.
Posible solución (a decidir): (a) no usar el USR para decidir conflicto de
  alias cuando el tipo canónico coincide; esto contradice la prueba existente
  "conflicting alias USRs remain ambiguous" (USR sintéticos) y no se puede
  aplicar sin cambiar ese contrato. (b) Ignorar solo las diferencias entre USR
  de typedef calificados por archivo (c:<archivo>@T@<nombre>); conserva esa
  prueba, pero acopla NSC al formato de USR de Clang.
Estado: NO IMPLEMENTAR hasta decisión y autorización de alcance transversal.
Propuesta concreta y contraste con las pruebas: T08-002-PROPUESTA-TYPEDEF.md.
```

### H2. El USR no distingue encabezados homónimos

```text
HALLAZGO NO AUTORIZADO
Evidencia: sonda 5 y fixture homonimos. Tres same.h de directorios distintos
  dan el mismo USR interno (c:same.h@...).
Riesgo: el USR no basta para distinguir declaraciones internas de encabezados
  homónimos. Comprobado que NSC no las une: typedef de otro tipo rechazado,
  static inline como dos definiciones (llamada ambigua). Un typedef del mismo
  tipo se acepta, lo que es compatible en C.
Estado: sin defecto demostrado; la procedencia de archivo solo existe para
  records.
```

### H3. Funciones, globales y alias no viajan en el NSMETA

```text
HALLAZGO NO AUTORIZADO
Evidencia: el catálogo C solo publica records y enums alcanzables desde la
  superficie NativeSharp (provideCppCRecordMetadataCatalog).
Pregunta: ¿T08-002 exige conservar en la biblioteca precompilada la identidad de
  funciones, globales y alias C, o basta la sesión propia del consumidor? Si lo
  exige, implica cambiar el formato NSMETA (protegido).
Estado: decisión pendiente; no se modifica la metadata. Lo que necesita el
  consumidor y la propuesta mínima: T08-002-PROPUESTA-METADATA-C.md.
```

### H4. Enum anónimo como tipo de campo de un record publicado

```text
HALLAZGO NO AUTORIZADO
Evidencia: pruebas-tanda-08-identidad-c/anonimos/ProducerAnonLevel.ns publica
  CAnonLevelRecord, cuyo campo level es un enum anónimo. El productor falla con
  "metadata producer read-back verification failed: C enum field dependency has
  no importable identity".
Alcance: catálogo de tipos C (T08-009/T08-010), no solo identidad.
Estado: NO IMPLEMENTAR; registrado.
```

## Pendientes de T08-002

1. Validación integral de la tanda 08 con las pruebas nuevas (Codex).
2. Decidir H1 (T08-002-PROPUESTA-TYPEDEF.md) y, si se autoriza, implementarlo
   con sus pruebas.
3. Decidir H3 (T08-002-PROPUESTA-METADATA-C.md) y, si se autoriza, implementarlo
   con sus pruebas.
4. Decidir H4 (enum anónimo como tipo de campo publicado).
5. Importación de globales `static` de encabezados (T08-004); hoy no se importan.
