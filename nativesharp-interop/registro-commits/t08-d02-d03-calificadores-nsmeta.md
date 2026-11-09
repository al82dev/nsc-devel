# T08-D02 y T08-D03: calificadores del dato apuntado en firmas públicas a través de NSMeta

Fecha: 2026-11-09. Casillas: T08-D02, T08-D03 y las decisiones pendientes de T08-002 sobre la superficie
materializada desde metadata (`const`/`volatile` de `T const*` y de alias C, y firma pública con callback
`volatile`).

## Problema

Un productor precompilado publica su API como metadata NSMeta. El consumidor sin encabezado la materializa como
fuente NativeSharp (`.nsmeta.public-api.ns`). La clave de tipo de una posición de firma conserva el portador
(`type:Pub::Payload*`), pero no los calificadores del dato apuntado. Así se perdía el contrato en tres casos:

| Caso | Antes, en el consumidor sin encabezado |
|---|---|
| Método público con `int const*` o con un alias C `CPayload` (`typedef const Payload *CPayload`) | el parámetro y el resultado llegaban como puntero mutable: se aceptaba escribir por el resultado y el método no se podía convertir a un callback C con `const` |
| Delegate con nombre declarado con alias C (`delegate int VHandler(VPayload p)`) | el consumidor aplicaba las reglas sin calificador: aceptaba métodos sin `volatile`, rechazaba invocar con un `VPayload` y leía sin acceso volatile |
| Miembro cuyo tipo es un callback C con `volatile` (`typedef int (*TakeV)(volatile Payload *)`) | no se publicaba: la materialización no podía escribir el tipo y el consumidor veía «no compatible member» |

## Solución

### Contrato NSMeta (único, sin versiones)

Cada posición de firma de tipo puntero lleva un byte de calificadores del dato apuntado: `kMetadataPointeeConst`
(bit 0) y `kMetadataPointeeVolatile` (bit 1). Las posiciones son:

- de cada tipo: resultado y parámetros de delegate, y campos;
- de cada miembro y de cada función de namespace: tipo declarado y parámetros;
- de cada estado de namespace: tipo declarado.

El catálogo se escribe en una sección de la carga útil del contrato vigente (1.49) y se lee antes de la
comprobación de fin. El lector valida que cada cuenta coincida con su catálogo y que solo se usen los bits
definidos. No hay lectura alternativa para formatos anteriores. El productor sube el minor escrito a 49
cuando la biblioteca tiene algún calificador; si no lo hace, el archivo saldría con un minor inferior y sin la
sección.

La fusión de metadata, el contrato de fuente de `LibraryApiDiff` y el informe ABI tienen en cuenta los
calificadores. El informe muestra `pointee=const`, `pointee=volatile` o `pointee=const+volatile` en
parámetros y resultados.

### Materialización

`PublicSurfaceBuilder` escribe el calificador en la fuente de API pública, delante del primer `*`:
`Payload const*`, `Payload volatile*` y `Payload const volatile*`. Se aplica en parámetros, resultados y
parámetros de delegate, métodos de interfaz, campos de instancia y estáticos, métodos, funciones y estados de
namespace, y accesores.

Para los callbacks C (`fn:`), `MetadataSourceMapping` ya escribía `const`. Ahora escribe también `volatile`
(`spellVolatilePointee`), en lugar de rechazar el miembro.

### Escritura interna `T volatile*` (T08-D03)

NativeSharp no tiene sintaxis pública para un dato apuntado `volatile`. Siguiendo la decisión del usuario, se usa
una representación interna que no añade sintaxis al lenguaje:

- El parser tiene un modo `allowMetadataSpellings()`. Solo en ese modo `parsePointeeConst` acepta `volatile`
  después de un `const` opcional, y marca el tipo con `setPointeeVolatile`. Los parsers hijos heredan el modo.
- `ProjectSyntaxUnit` activa el modo solo para las unidades `precompiled` (fuentes materializadas desde metadata).
- El código de usuario no cambia: `int volatile* p` sigue siendo un error de sintaxis. La prueba lo comprueba.

`LambdaDelegateBinder` ya no ignora los calificadores de un delegate precompilado. Ahora que llegan desde NSMeta,
la conversión de métodos y lambdas los aplica igual que en el camino directo.

### Thunk de frontera de callback con enlace ODR

Al convertir el `ReadC` publicado a un callback C en el productor y en el consumidor a la vez, el enlace fallaba
con `duplicate symbol definition: '__nsc_method_Pub$3A$3APub$2EQ__ReadC__...__c_boundary'`. El nombre del thunk
depende solo del destino, así que cada imagen lo definía. Este camino solo se hizo alcanzable con D02, porque
antes el consumidor no podía convertir ese método.

Ahora el thunk definido recibe `weak_odr` con un COMDAT por símbolo (`applyOdrDefinition`, igual que el resto de
entidades ODR públicas). Al enlazar queda una sola copia, y por lo tanto una sola dirección de callback por
destino.

### Propuesta opcional: sintaxis pública `T volatile*`

No se implementa; se documenta según la decisión 3. Hacer pública la escritura `T volatile*` permitiría a código
NativeSharp declarar parámetros y locales con un dato apuntado volatile sin pasar por un alias C.

- **Interacción:** ya existen las reglas semánticas, porque el modo interno usa la misma marca que los
  alias C. La pérdida del calificador se rechaza, la adición es implícita y los accesos son volatile.
- **Efectos:** cambia el contrato público de la gramática (el parser aceptaría código que hoy rechaza). Habría que
  definir su orden respecto a `const` (`T const volatile*`) y documentarlo en la especificación.
- **Riesgo:** bajo para el código existente, porque hoy es un error de sintaxis y ningún programa válido cambia
  de significado.

## Cambios

| Archivo | Cambio |
|---|---|
| `include/nativesharp/Interop/LibraryMetadata*.h` | Constantes de calificadores y campos `pointeeQualifiers` en parámetros, miembros, campos, funciones y estados de namespace y delegates |
| `src/Interop/LibraryMetadataPayload.cpp` | Codificación y decodificación del catálogo de calificadores |
| `src/Interop/LibraryMetadataConstruction.cpp` | Llena los calificadores desde la sintaxis y desde los alias C, y sube el minor escrito a 49 cuando hay alguno |
| `src/Interop/LibraryMetadataMerger.cpp`, `LibraryApiDiff.cpp`, `LibraryAbiReport.cpp` | Igualdad, contrato de fuente e informe con calificadores |
| `src/Driver/PublicSurfaceBuilder.cpp` | Escribe `const`/`volatile` del dato apuntado en la fuente de API pública |
| `src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp` | Escribe `volatile` en los tipos `fn:`; se retira el rechazo |
| `include/nativesharp/Parser/Parser.h`, `src/Parser/TypeParser.cpp`, `src/Parser/ParserContext.cpp` | Modo de escritura de metadata: `T volatile*` solo en fuentes materializadas |
| `src/Frontend/ProjectSyntaxUnit.cpp` | Activa el modo para las unidades precompiladas |
| `src/Frontend/Bodies/LambdaDelegateBinder.cpp` | Aplica los calificadores de los delegates precompilados |
| `src/LLVM/Lowering/OopLlvmThunksAndWrappers.cpp` | Thunk de frontera de callback con `weak_odr` y COMDAT |

## Pruebas

**Nueva:** `pruebas-tanda-08-identidad-c/typedef-calificadores/validar-firma-publica-calificada.py`. Aprobada en
O0–O3, directo (con encabezado) y precompilado (consumidor sin encabezado).

- **Productor.**
  - Métodos con `int const*`, `CPayload`, `VPayload` y `CVPayload`, como resultado y como parámetro.
  - Un campo estático `int const*` y una función de namespace con `CPayload` y `VPayload`.
  - Delegates `VHandler(VPayload)` y `CHandler(CPayload)`.
  - Callbacks C `TakeV` (`volatile`) y `TakeC` (`const`), devueltos por métodos públicos.
- **Positivos:**
  - lecturas y adición implícita de calificadores (`Payload*` → `CPayload`/`VPayload`, `VPayload` → `CVPayload`);
  - escritura y lectura por `VPayload`;
  - delegates enlazados a los métodos publicados y una lambda `CHandler`;
  - los callbacks devueltos se invocan y se pasan de vuelta;
  - el `ReadC`/`ReadV` publicado se convierte a `TakeC`/`TakeV` en el consumidor.
- **IR (O1):** los accesos por `VPayload` del consumidor son `load volatile` y `store volatile`.
- **Once rechazos en O0:**
  - escribir por `int const*`, por `CPayload` y por el campo `int const*`;
  - descartar `const`, `volatile` o ambos al inicializar un `Payload*`;
  - pasar un `VPayload` a un parámetro `Payload*`;
  - enlazar un método con `Payload*` a `VHandler`, a `TakeC` y a `TakeV`;
  - escribir en una lambda `CHandler`.
- **Rechazo del modo interno:** `int volatile* p` en código de usuario es un error de sintaxis.

**Actualizadas** (fijaban el comportamiento anterior; autorización del usuario, punto 6):

- `typedef-calificadores/validar-delegate-alias-calificado.py`: el camino precompilado ya no se limita a los casos
  sin calificador. Ejecuta los seis positivos y los siete rechazos, igual que el directo.
- `typedef-calificadores/validar-lambda-calificada.py`: el caso `CaseMethodConst` (método público `ReadConst(Payload
  const*)` convertido a `ConstFn`) se ejecuta también en el camino precompilado.

## Regresión

Todo con el compilador final, incluido el thunk ODR:

- `A:\nsc-interop-build\claude\scripts\regresion-d02.cmd`, resultado 0 (29 suites):
  - `typedef-calificadores`, las siete;
  - `typedef-funcion-record`, las cuatro;
  - `cache-c-directa`, `global-externa-variantes` (dos) y `funcion-direccion-c`;
  - calificadores, callbacks, punteros, miembros de record, typedef (formas y reconciliación), globales
    (escalares calificados y consumo), colisiones y `void`;
  - los cuatro validadores de plantillas (`record`, `global`, `alias` y `funcion`).
- `regresion-callbacks.cmd`, resultado 0: callbacks con record y enum precompilados, `bool`, punteros `volatile` y
  typedef de función con record, en O0–O3.
- `regresion-d01.cmd`, resultado 0: DLL y TLS. Se repite porque el thunk ODR también se emite en DLL.
- Pruebas unitarias (`unit-d02.cmd`, construcción incremental), con el entorno de enlace fijado. Pasan
  `nsc_library_metadata_version_tolerance_tests`, `nsc_library_metadata_merger_tests`,
  `nsc_ns3_t13_pointer_qualifier_tests`, `nsc_ns3_t12_api_fingerprint_tests` y `nsc_public_surface_emitter_tests`.
  `nsc_stage48_2_library_metadata_tests` pasa 442 de 444 comprobaciones. Las cuatro que fallan se describen en
  la sección siguiente.

## Fallos previos y ajenos registrados

- `nsc_stage48_2_library_metadata_tests`: dos comprobaciones leen `tests/fixtures/nsmeta/historical-1.46.nsmeta`,
  que no está versionado (el repositorio tiene de 1.0 a 1.45, 1.47 y 1.48). No depende de este cambio.
- `nsc_stage42_delegates_events_tests` no enlaza: le falta `PrecompiledSemanticSnapshotCodec::decode`
  (dependencia de `nsc_frontend` que el objetivo no declara). No depende de este cambio.
- `nsc_parser_tests`: «invalid ternary conditional syntax is rejected by NativeSharp 1.0.1». El operador
  condicional ya forma parte del lenguaje. La expectativa es histórica y ajena a los calificadores.
- `nsc_namespace_function_metadata_tests` y `nsc_stage50_9_metadata_host_compatibility_tests` esperan que toda
  biblioteca se escriba con `kMetadataFormatMinor`. El productor escribe el minor mínimo según su catálogo desde los
  commits del 2026-11-06 al 2026-11-08 (anteriores a este cambio). Esa lógica de minor mínimo y la decisión del
  contrato único se revisarán juntas; queda registrado en el tracking como T08-D24.
