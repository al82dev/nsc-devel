# T08-002: declaraciones C usadas por plantillas públicas y por cuerpos de biblioteca (H3, H7, H8)

Estado: **PROPUESTA NO IMPLEMENTADA**. NSMETA no se modifica; cualquier cambio de
formato requiere autorización.

Corrección respecto a la versión anterior de este documento: proponía la
"versión de envoltura 4" del catálogo C. Ese valor ya existe.
`encodeCTypeCatalogWithProvenance` (`CppCRecordMetadataCodec.cpp:578-590`) usa
1 = procedencia, 2 = procedencia + formato flotante, 3 = procedencia incompleta
y 4 = procedencia incompleta + formato flotante.

## Casos con biblioteca precompilada

HECHO COMPROBADO con `pruebas-tanda-08-identidad-c/plantillas` (tabla
`casos.txt`), O0, compilador de este carril y Clang 23.1.0. El consumidor no
tiene encabezado salvo en `ConsumerGenericHeader`.

| Consumidor | Qué usa la biblioteca | ACTUAL | PROP |
|---|---|---|---|
| `ConsumerPlain` | métodos no genéricos con función, global y typedef C | ejecuta | ejecuta |
| `ConsumerRecordParam` | plantilla con parámetro `GenRecord*` (record en la superficie) que lee un campo | ejecuta | ejecuta |
| `ConsumerInlinePlain` | método no genérico que llama a `static inline` | ejecuta | ejecuta |
| `ConsumerGeneric` | plantilla que llama a `gen_c_number()` y lee `gen_c_counter` | `unknown identifier` | ejecuta |
| `ConsumerGenericHeader` | la misma, con el `Gen.h` propio del consumidor | `unknown identifier` | ejecuta |
| `ConsumerAlias` | plantilla con un local `GenCount` | `unknown type name 'GenCount' in module 'CGenProducer'` | ejecuta |
| `ConsumerRecordLocal` | plantilla que llama a `gen_c_record()` y lee `->value`; el record no está en la superficie | `unknown identifier 'gen_c_record'` | ejecuta |
| `ConsumerGlobalWrite` | plantilla que escribe `gen_c_counter` | `unknown identifier` | ejecuta |
| `ConsumerInline` | plantilla que llama a `static inline gen_c_twice` | `unknown identifier 'gen_c_twice'` | falla en el productor con diagnóstico propio |
| `ConsumerEnumConstantPlain` | método **no genérico** que usa `GenModeB` | productor: `metadata contains an incomplete or duplicate namespace state '.GenModeB'` | ejecuta |
| `ConsumerEnumConstant` | plantilla que usa `GenModeB` | mismo error del productor | ejecuta |
| `ConsumerAnonymousConstantPlain` | método **no genérico** que usa `GenLoose` (enum anónimo) | productor: `... namespace state '.GenLoose'` | ejecuta |
| `ConsumerAnonymousConstant` | plantilla que usa `GenLoose` | mismo error del productor | ejecuta |

## Qué necesita exactamente el consumidor

HECHO COMPROBADO:

- El cuerpo de una plantilla pública viaja como fuente y el consumidor lo vuelve
  a analizar en el ámbito del módulo productor. En ese ámbito no hay sesiones
  Clang; las del consumidor pertenecen a su propio proyecto
  (`ConsumerGenericHeader` falla igual).
- Un record que la plantilla recibe como parámetro está en la superficie, viaja
  en el catálogo C y basta (`ConsumerRecordParam`).

HECHO COMPROBADO por lectura del código: cada símbolo que el productor obtiene de
C es una función pura de un snapshot de declaración Clang, del ordinal y nombre
del proyecto propietario y de la búsqueda de tipos.

- Funciones: `projectFunction` (`CppSemanticFunction.cpp:225-310`).
- Globales: `projectGlobalWithLookup` (`CppSemanticInventory.cpp:180` y
  siguientes).
- Alias: `provideCppTypeAliases`.
- Constantes de enum: `projectCppEnumConstant`.

La información exacta que necesita el consumidor son esos **snapshots de
declaración**, no los símbolos NativeSharp ya proyectados. Con ellos vuelve a
ejecutar la misma proyección con el ordinal y el nombre del módulo productor. Así
obtiene el mismo `signatureKey`, el mismo `callableIdentityKey`
(`cpp:<módulo>::<símbolo>|<firma>`) y el mismo símbolo de enlace.

| Uso en la plantilla | Datos necesarios (campos que lee la proyección) |
|---|---|
| Llamada a función C | `CppFunctionDeclaration`: `name`, `qualifiedName`, `externalLinkage`, `externalSymbol`, `callingConvention`, `languageLinkage`, `variadic`, `hasPrototype`, `dllImport`, `returnType` y `parameters[].{name,type}` como `CppType`, `clangUsr`; de la entrada: triple de destino |
| Lectura o escritura de global C | `CppGlobalDeclaration`: `name`, `qualifiedName`, `type`, `externalLinkage`, `threadLocal`, `dllImport`, `externalSymbol`, `declarationAlignmentBits`, `clangUsr`; de la entrada: lenguaje C, semántica volatile de MS y almacenamiento atómico (`CppSemanticInventory.cpp:555-565`) |
| Typedef C | `CppTypeAliasDeclaration`: `name`, `qualifiedName`, `underlyingType`, `insideAnonymousNamespace`, `clangUsr`; triple de destino |
| Record o enum de esas firmas o de un local | entrada existente del catálogo: semilla adicional del cierre `provideCppCRecordMetadataCatalog` (`ConsumerRecordLocal`) |
| Constante de enum con nombre | entrada de enum existente en el catálogo; el consumidor la restaura con `reconstructCEnumConstants` |
| Constante de enum anónimo de archivo | entrada nueva de "enum anónimo"; clave: USR del primer enumerador `c:@Ea@<primero>@<primero>`, que no depende del encabezado (sonda 1) |
| Función `static inline` | no se transporta: su definición se compila en la biblioteca con el símbolo interno `cppCInlineLinkSymbol(proyecto, referencia, nombre)`. Propuesta mínima: diagnóstico del productor |

`location` solo alimenta `sourcePath` y diagnósticos. No se publica como ruta
absoluta; si se conserva, es relativa al proyecto productor.

Todos los `CppType` de esas entradas ya los serializa el codec del catálogo, con
profundidad acotada.

Selección en el productor: solo las declaraciones C que demandan los cuerpos de
plantillas publicados, mediante la captura de demanda existente
(`CppFunctionDemand`) sobre la fuente de plantillas antes de escribir la
metadata. Si no hay ninguna, la sección no existe.

Conciliación en el consumidor que también tiene el encabezado: se usan el USR,
el símbolo externo y la firma, con las reglas actuales de agrupación de funciones
y globales. Una discrepancia es un error y nunca se une por nombre solo.

## H7: constantes de enum C en cualquier cuerpo de biblioteca

HECHO COMPROBADO: un método **no genérico** de biblioteca que usa una constante de
enum C, con nombre o anónimo, impide construir la biblioteca:
`metadata producer read-back verification failed: metadata contains an
incomplete or duplicate namespace state '.GenModeB'`. No depende de plantillas
ni de este carril.

Causa (por lectura del código):

- El proveedor crea la constante como estado público con espacio de nombres
  vacío (`CppSemanticInventory.cpp:900-920`).
- `appendNamespaceStateMetadata` (`LibraryMetadataConstruction.cpp:2961-2976`)
  excluye `isCppExternal`, pero no `isCppEnumConstant`.
- La verificación de lectura (`LibraryMetadataVerification.cpp:1371-1384`)
  rechaza el espacio de nombres vacío.

Corrección propuesta, sin cambio de formato: excluir también
`state.isCppEnumConstant` en esa condición. El valor ya está plegado en el cuerpo
compilado. Un consumidor que necesite la constante la obtiene del catálogo
(enum en la superficie) o, para plantillas, de esta propuesta. No se aplicó:
toca la publicación de estados de espacio de nombres, que también usa el carril
de variables globales, y se pidió evidencia antes de cualquier cambio de
metadata.

## H8: el catálogo heredado y la envoltura se distinguen por los primeros bytes

HECHO COMPROBADO por la sonda 7. Un catálogo heredado (NSMETA 1.42) que solo
contiene enums empieza por el tamaño u64 del bloque de records vacío:
`04 00 00 00 00 00 00 00`. `isCTypeCatalogWithProvenance`
(`CppCRecordMetadataCodec.cpp:671-676`) solo mira los cuatro primeros bytes y lo
clasifica como envoltura 4. `decodeVersionedCTypeCatalog` falla entonces con
`invalid C record catalogue size`.

NO VERIFICADO: si algún productor 1.42 real llegó a escribir un catálogo solo de
enums. Basta con que exista uno para que el lector actual lo rechace.

La misma detección se repite en la proyección ABI
(`LibraryMetadataPayload.cpp:760-775`, que además fija `abiCatalog[0] == 2 || == 4`)
y en `reconstructPrecompiledCImportedTypes` (`CppSemanticRecord.cpp:1242`).

Consecuencia para el versionado: los números de envoltura no pueden crecer de
forma segura. Un tamaño heredado pequeño (bytes 1–3 nulos) puede coincidir con
cualquier versión nueva de 5 a 255. El discriminante fiable es el minor de NSMETA:
el escritor ya usa la envoltura en todo archivo 1.43 o posterior.

## Opciones de versionado

| Opción | Escritura | Lector actual (1.45) con un archivo nuevo | Coste y riesgos |
|---|---|---|---|
| A. Solo envoltura del catálogo (5 o más), sin subir el minor | catálogo con versión 5+ | decodifica la carga y falla al verificar el catálogo: `invalid C record catalogue: invalid C provenance catalogue envelope` | pocos archivos, pero agrava H8 (detección por bytes) y rompe el precedente 1.43–1.45. Las versiones 1–4 combinan dos indicadores; un tercero duplica las combinaciones. Hay que tocar también la proyección ABI. |
| B. Envoltura nueva dentro del catálogo + minor 46 solo si la sección existe | minor 46 únicamente con la sección, como hoy 43–45 (`LibraryMetadataConstruction.cpp:3061-3068`) | igual que A: falla al verificar el catálogo. NO VERIFICADO si algún punto anterior rechaza el minor 46: `verifyMetadataEnvelope` omite la comprobación de versión si el lector decodificó la carga (`LibraryMetadataVerification.cpp:59-68`) | mismo número de archivos que A más la constante del minor. El decodificador puede elegir el formato por el minor y no por bytes. Mantiene la puerta "metadata before 1.46 cannot represent …". |
| C. Sección nueva de primer nivel en `LibraryMetadata` + minor 46 | campo nuevo tras el catálogo | `metadata payload has trailing bytes` (`LibraryMetadataPayload.cpp:2023-2027`) | separa tipos y declaraciones invocables, pero toca `LibraryMetadata.h`, carga, codec descendente, fusión (`LibraryMetadataMerger.cpp:483-522`), poda de superficie (`PrecompiledSurfaceReachability.cpp:443`), verificación, construcción y proyección ABI |
| D. Archivo adicional junto a la `.lib` | fuera de NSMETA | lo ignora | queda fuera del hash, de la huella ABI y de la verificación; puede desincronizarse de la biblioteca. Descartada. |

### Recomendación: B

1. Una disposición nueva del catálogo, elegida por `formatMinor >= 46`, con una
   máscara u32 de características explícita en lugar de más números de
   combinación:
   - bit 0: formato flotante;
   - bit 1: procedencia incompleta;
   - bit 2: declaraciones C de plantillas (esta propuesta);
   - bit 3: enum anónimo de campo (T08-002-PROPUESTA-ENUM-CAMPO.md).
2. `decodeVersionedCTypeCatalog` recibe el minor del archivo. Para minor 46 o
   posterior no adivina por los bytes. Para minor 43–45 mantiene la envoltura
   1–4. Para minor 42 lee el formato heredado. Esto corrige H8 para los archivos
   que declaran su minor.
3. El minor sube a 46 solo cuando el catálogo usa una característica de 1.46. Si
   no, los bytes del NSMETA no cambian (invariante de ausencia).
4. Una sola subida de versión cubre esta propuesta y la parte C del enum de
   campo.

Consecuencias:

- Bibliotecas sin las características nuevas: NSMETA idéntico al actual. Se
  comprueba con los fixtures existentes.
- Lectores anteriores: rechazan el archivo nuevo (falla cerrada). El mensaje
  exacto está NO VERIFICADO.
- `kMetadataFormatMinor` (`LibraryMetadata.h:120`) es común a todos los carriles.
  Si el trabajo de TLS, globales o layout de records también necesita un minor,
  hay que asignar los números en orden al integrar. No se tocó.
- El catálogo entra en la proyección ABI (`LibraryMetadataPayload.cpp:760-775`).
  Las entradas nuevas cambian lo que compila el consumidor y deben entrar también
  en la huella ABI, sin la procedencia, como hoy los records.
- `PrecompiledSyntaxModule` necesita el minor del archivo para decodificar el
  catálogo (`CppSemanticRecord.cpp:1242`, `PrecompiledModuleConfiguration.cpp:547`).
- Riesgo de ABI para la llamada desde el consumidor: el símbolo externo de la
  función C debe resolverse con la biblioteca C del productor. Ya se registra
  como dependencia binaria (`LibraryMetadataCppBinaryDependency`). NO VERIFICADO
  que el enlazado la propague cuando solo la usa una plantilla instanciada en el
  consumidor; `ConsumerGeneric` en modo PROP lo comprobará.

## Pruebas

Preparadas y ejecutadas con su resultado ACTUAL: las trece filas de
`plantillas/casos.txt`. `validar.cmd` con `NSC_T08_PROPUESTAS=1` exige PROP.
Para añadir al implementar:

1. O0–O3 de `ConsumerGeneric`, `ConsumerAlias`, `ConsumerRecordLocal` y
   `ConsumerGlobalWrite`.
2. Encabezado propio del consumidor con una función del mismo USR y otra firma:
   error de conflicto.
3. Ausencia: una biblioteca sin plantillas con C produce NSMETA byte a byte igual
   al actual.
4. Compatibilidad: los NSMETA de los fixtures existentes (1.42–1.45) se leen
   igual, incluido un catálogo heredado solo de enums (H8).
5. Codec: ida y vuelta, truncamiento, duplicados, máscara con bits
   desconocidos y profundidad de tipos.
6. Identidad restaurada por la ruta del driver: USR, símbolo y firma de cada
   entrada comparados con Clang (`--verify-driver-restored-c-usr` extendido).

## Archivos que cambiarían

| Archivo | Cambio | Compartido con |
|---|---|---|
| `src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp` | disposición 1.46, máscara, decodificación por minor | **metadata**, layout de records |
| `src/Interop/Cpp/CppSemanticRecord.cpp` | selección en el productor, semillas, restauración | **layout de records** |
| `src/Interop/Cpp/CppSemanticInventory.cpp`, `CppSemanticFunction.cpp` | reproyección desde snapshots restaurados | **variables globales** (proyección de globales) |
| `src/Driver/PrecompiledModuleConfiguration.cpp` | proveedores de funciones, globales y alias precompilados | driver |
| `src/Interop/LibraryMetadataConstruction.cpp` | minor 46 condicional; exclusión de H7 | **metadata**, variables globales |
| `src/Interop/LibraryMetadataVerification.cpp` | puerta de 1.46 | **metadata** |
| `src/Interop/LibraryMetadataPayload.cpp` | proyección ABI sin detección por bytes | **metadata** |
| `include/nativesharp/Interop/LibraryMetadata.h`, `include/nativesharp/Frontend/PrecompiledSyntaxModule.h` | constante del minor; minor del módulo | todos |
| `tests/unit/CppCIdentityTests.cpp`, `pruebas-tanda-08-identidad-c/plantillas` | pruebas | este carril |
