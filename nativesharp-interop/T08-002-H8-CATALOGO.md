# T08-002: catálogo C heredado 1.42 confundido con la envoltura 4 (hallazgo H8)

Estado: **REPRODUCIDO; PROPUESTA NO IMPLEMENTADA**. El codec y el formato no
cambian.

## Fixture mínimo

`pruebas-tanda-08-identidad-c/catalogo/legacy-enum-only.hex` contiene los 240
bytes del catálogo heredado (NSMETA 1.42, `encodeCTypeCatalog`) con un solo enum
sintético: `CEnvOnly`, con `CEnvOne = 1` de tipo `int`, en el módulo
`CIdentityEnvelope`. El modo `--catalogue-version-controls` de
`nsc_cpp_c_identity_tests` comprueba que el codificador sigue produciendo
exactamente esos bytes y aplica a ellos el lector actual y la regla propuesta.

```text
04 00 00 00 00 00 00 00   tamaño u64 del bloque de records: 4 (bloque vacío)
00 00 00 00               bloque de records: 0 records
01 00 00 00               1 enum
1b 00 00 00 "CIdentityEnvelope::CEnvOnly" ...
```

Una envoltura versionada empieza por la versión u32 y el tamaño u64 de la base.
Por ejemplo, el productor real `PCatFloatOpaque` empieza por
`04 00 00 00 97 01 00 00`. Los cuatro primeros bytes coinciden con los del
catálogo heredado sin records.

## Reproducción en NSMETA real

La misma sección construye seis productores reales
(`catalogo/PCat*.nsproj`) y reescribe su catálogo en cada minor que puede
representarlo. Usa `LibraryMetadataCodec::writeModel`, la vía existente para
escribir versiones anteriores. Después relee cada archivo con el lector actual
(`read` y `verifyCurrentCompiler`) y con la regla propuesta.

HECHO COMPROBADO (validador, sección `catalogo`):

| Productor | Minor escrito hoy | Envoltura | 1.42 | 1.43 | 1.44 | 1.45 |
|---|---|---|---|---|---|---|
| `PCatEnumOnly` (solo enum) | 1.43 | 1 | **falla**: `metadata before 1.43 cannot represent C source provenance` | lee | lee | lee |
| `PCatRecord` | 1.43 | 1 | lee | lee | lee | lee |
| `PCatRecordEnum` | 1.43 | 1 | lee | lee | lee | lee |
| `PCatFloat` (`long double`) | 1.44 | 2 | no representable | no representable | lee | lee |
| `PCatOpaque` (struct incompleto) | 1.45 | 3 | no representable | no representable | no representable | lee |
| `PCatFloatOpaque` | 1.45 | 4 | no representable | no representable | no representable | lee |

La regla propuesta decodifica las dieciséis variantes, incluida la 1.42 solo de
enums.

Otros hechos:

- El escritor actual no siempre escribe 1.45: usa el minor más bajo que
  representa la envoltura (`LibraryMetadataConstruction.cpp:3061-3068`). Los
  archivos 1.43 y 1.44 son, por tanto, los que se producen hoy.
- Un `double` no activa el formato flotante (envoltura 1); un `long double`, sí
  (envoltura 2).
- El lector actual rechaza la 1.42 solo de enums con un mensaje engañoso: la toma
  por una envoltura con procedencia en un archivo demasiado antiguo.
- Las pruebas existentes de `CppClangSessionTests.cpp` cubren el catálogo
  heredado con records (líneas 3710-3737) y el 1.41 sin catálogo (3772-3775),
  pero no el catálogo heredado solo de enums.

## Qué bytes identifica cada lector

| Lector | Ubicación | Qué mira | Efecto con la 1.42 solo de enums |
|---|---|---|---|
| Decodificación de la carga | `LibraryMetadataPayload.cpp:2014-2021` | ninguno: con minor ≥ 42 lee el tamaño u64 y un bloque opaco | correcto |
| Proyección ABI | `LibraryMetadataPayload.cpp:760-783` | `isCTypeCatalogWithProvenance` (bytes 0–3) y `abiCatalog[0] == 2 \|\| == 4` | la decodificación como envoltura falla y se usa el bloque crudo; la huella es coherente en escritura y lectura, así que no falla aquí |
| Detección de envoltura | `CppCRecordMetadataCodec.cpp:671-676` | `size >= 4`, `bytes[0]` en 1–4 y `bytes[1..3] == 0` | **la clasifica como envoltura 4** |
| Decodificación con versión | `CppCRecordMetadataCodec.cpp:678-684` | la detección anterior | va a `decodeCTypeCatalogWithProvenance`: `invalid C record catalogue size` |
| Puertas de verificación | `LibraryMetadataVerification.cpp:1420-1455` | la detección, `bytes[0]` y el minor | `metadata before 1.43 cannot represent C source provenance` |
| Inclusión de tipos C | `LibraryMetadataVerification.cpp:1066-1071` | `decodeVersionedCTypeCatalog` | no se alcanza |
| Mapeo de la superficie | `Driver/PrecompiledSurface/MetadataSourceMapping.cpp:428-433` | `decodeVersionedCTypeCatalog` | fallaría igual |
| Módulos precompilados | `CppSemanticRecord.cpp:1242`, `1768`; `CppCRecordMetadataCodec.cpp:868` | `decodeVersionedCTypeCatalog` sobre `PrecompiledSyntaxModule::cppCTypeCatalog`, **sin minor** | fallaría igual |
| Configuración del driver | `PrecompiledModuleConfiguration.cpp:547` | solo si el catálogo está vacío | correcto |
| Escritor | `CppSemanticRecord.cpp:1091-1106`; `LibraryMetadataConstruction.cpp:3061-3068` | elige la envoltura por el minor (≥ 43) y el minor por `catalog[0]` | correcto |

Por qué solo colisiona este caso: el catálogo heredado empieza por el tamaño u64
del bloque de records, que vale 4 más el tamaño de los records. Para que los
bytes 1–3 sean nulos, ese tamaño tiene que ser menor que 256. Para que el byte 0
esté entre 1 y 4, tiene que ser exactamente 4: el bloque vacío.

## Propuesta: elegir el formato por la versión declarada

1. `decodeVersionedCTypeCatalog` recibe el minor del NSMETA, y la detección por
   bytes deja de decidir:
   - minor < 42: no puede haber catálogo (puerta existente);
   - 42: formato heredado (`decodeCTypeCatalog`);
   - 43: envoltura versión 1;
   - 44: envoltura versión 1 o 2;
   - 45: envoltura versión 1 a 4.

   Cualquier otra combinación es un error con un mensaje que nombre el minor y
   la versión. Es la réplica `proposedCatalogueDecode` de la prueba.
2. Las puertas de `verifyBinaryClosureMetadata` usan esa misma decisión en lugar
   de `isCTypeCatalogWithProvenance`.
3. `PrecompiledSyntaxModule` necesita el minor del archivo, o bien el catálogo ya
   decodificado. Hoy solo lleva los bytes (`PrecompiledSyntaxModule.h:46`). La
   alternativa más simple es un campo `cppCTypeCatalogFormatMinor`, que llena
   quien copia el catálogo (`PrecompiledProjectPreparation.cpp:458`).
4. La proyección ABI usa la misma decisión.

   **Compatibilidad de la huella:** para todo archivo que hoy se lee, la decisión
   por minor coincide con la detección actual (dieciséis variantes de la tabla). En
   la 1.42 solo de enums, la proyección actual ya usa el bloque crudo, y el bloque
   heredado sin procedencia es justamente lo que una decodificación correcta
   reproduciría. Por tanto la huella no cambia. Esto es una inferencia: no se
   comprobó porque el codec no se modificó.

Alternativa sin minor (descartada como regla principal): mirar también los
bytes 4–7. En la envoltura son la parte baja del tamaño de la base, que no es
cero. En el catálogo heredado son la parte alta de su tamaño, que es cero. Separa
los formatos actuales, pero sigue adivinando por bytes y no sirve para una
envoltura futura. Solo tiene sentido si algún lector no puede conocer el minor.

## Controles de compatibilidad incluidos

- El fixture mínimo 1.42 solo de enums, byte a byte frente al codificador.
- Las dieciséis variantes reales de la tabla: el lector actual debe fallar
  exactamente en la 1.42 solo de enums y leer las demás, y la regla propuesta
  debe decodificarlas todas.
- Fuera de esta prueba siguen vigentes las pruebas de codec de
  `CppClangSessionTests.cpp` (heredado con records, envolturas 1–4, 1.41 sin
  catálogo).

## Archivos que tocaría la propuesta

`src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp`,
`src/Interop/LibraryMetadataVerification.cpp`,
`src/Interop/LibraryMetadataPayload.cpp`, `src/Interop/Cpp/CppSemanticRecord.cpp`,
`src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp`,
`include/nativesharp/Frontend/PrecompiledSyntaxModule.h` y
`src/Compilation/PrecompiledProjectPreparation.cpp`. Todos son de metadata o
compartidos con el carril de layout de records. Ninguno se modificó.
