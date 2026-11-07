# T08-002: enum declarado dentro de un campo de record C (hallazgo H4)

Estado: **PROPUESTA NO IMPLEMENTADA**. Las partes A y B no cambian el formato de
metadata; la parte C sí y necesita autorización. No se modificó código del
compilador.

## Reproducción de extremo a extremo

HECHO COMPROBADO con el fixture `pruebas-tanda-08-identidad-c/enum-campo`
(tabla `casos.txt`), O0, compilador de este carril y Clang 23.1.0:

```c
typedef struct { int raw; enum { CNestLow = 1, CNestHigh = 2 } level; } CNestAnonField;
typedef struct { int raw; enum CNestNamed { CNestNamedA = 3, CNestNamedB = 4 } kind; } CNestNamedField;
```

| Proyecto | Ruta | Uso | Resultado actual |
|---|---|---|---|
| `HRaw` | encabezado | `v->raw` | compila y ejecuta |
| `HLevel` | encabezado | `v->level != CNestHigh` | `has no compatible member named 'level'` y `unknown identifier 'CNestHigh'` |
| `HLevelInt` | encabezado | `(int)v->level` | `has no compatible member named 'level'` |
| `HConst` | encabezado | `(int)CNestHigh` | `unknown identifier 'CNestHigh'` |
| `HNamed` | encabezado | `v->kind != CNestNamedB` | `has no compatible member named 'kind'` |
| `HNamedType` | encabezado | `CNestNamed k = v->kind` | `unknown type name 'CNestNamed'` |
| `PAnon` → `CAnon` | biblioteca | publica `CNestAnonField*` | productor: `C enum field dependency has no importable identity` |
| `PAnon` → `CAnonHeader` | biblioteca + encabezado propio | igual | mismo error del productor |
| `PNamed` → `CNamed` | biblioteca | publica `CNestNamedField*` | productor: `required C enum is unavailable` |
| `PInternal` → `CInternal` | biblioteca | cuerpo interno lee `v->level` | productor: `has no compatible member named 'level'` |

El fallo no se limita a la metadata: con el encabezado directo, el campo se
omite en silencio y los enumeradores no existen.

## Identidad que da Clang

HECHO COMPROBADO por la sonda 6 de `nsc_cpp_c_identity_tests` (afirmada), que lee
el mismo texto como `nested.h` dos veces y como `nested-copy.h`:

| Declaración | USR | Nombre calificado | Depende del encabezado |
|---|---|---|---|
| enum con nombre `CNestNamed` | `c:@E@CNestNamed` | `CNestNamed` | no |
| enum anónimo del campo `level` | `c:@SA@CNestAnonField@E@nested.h@26` | `CNestAnonField::enum (unnamed at <ruta absoluta>\nested.h:1:27)` | sí: nombre de archivo y desplazamiento |
| campo `level` | `c:@SA@CNestAnonField@FI@level` | | no |
| campo `kind` | `c:@SA@CNestNamedField@FI@kind` | | no |

Consecuencias:

- El enum con nombre declarado dentro del struct ya tiene en C una identidad de
  ámbito de archivo correcta: el problema no es su identidad, es que no se
  captura.
- El USR del enum anónimo cambia si se renombra el encabezado o se edita texto
  anterior. Su nombre calificado contiene una ruta absoluta de la máquina. Ninguno
  de los dos sirve como identidad ni puede escribirse en NSMETA: el codec actual
  serializa `enumIdentity.qualifiedName` del tipo del campo
  (`CppCRecordMetadataCodec.cpp:108-114`), así que bastaría con relajar el
  rechazo del productor para filtrar la ruta a la biblioteca.
- La identidad estable disponible es la del campo que lo usa, que no depende del
  encabezado.

## Causas, con ubicación

1. `ClangSession.cpp:189-247` (`collectEnums`) solo recorre `NamespaceDecl` y
   `LinkageSpecDecl`. En C una etiqueta declarada dentro de un struct pertenece
   al ámbito que contiene el struct (C11 6.2.1), pero NSC no la ve: la sonda 6
   informa que el snapshot de enums no contiene `CNestLow` ni `CNestNamedA`. De
   ahí `HConst`, `HNamedType`, `HNamed` y `CNamed` (`required C enum is
   unavailable`, `captureCppCEnumCatalog`, `CppSemanticInventory.cpp:604-674`).
2. `CppSemanticEnumType.h` (`projectCppSemanticEnumType`) exige
   `cppAliasEnumType`, que rechaza `anonymousContext`. El campo queda sin tipo
   proyectado y `projectCppSemanticInstanceField`
   (`CppSemanticRecord.cpp:118`, rama de enum en la línea 262) lo omite. De ahí
   `HLevel`, `HLevelInt` y `PInternal`.
3. El cierre del productor (`CppSemanticRecord.cpp:928-936`) rechaza el enum
   anónimo: `C enum field dependency has no importable identity` (`PAnon`).
4. El catálogo solo admite enums con nombre: `encodeCTypeCatalog` exige
   `value.name` (`CppCRecordMetadataCodec.cpp:500`) y la clave es
   `TypeIdentity(módulo, nombre)`.

## Propuesta

### Parte A. Enums declarados dentro de records C (sin cambio de metadata)

`collectEnums` también recorre los `RecordDecl` cuando el lenguaje es C. En C++
no cambia nada: un enum anidado en una clase tiene ámbito de clase.

- El enum con nombre pasa a ser un enum C ordinario (`c:@E@CNestNamed`, nombre
  calificado igual al nombre). Tipo, constantes, cierre y catálogo existentes lo
  tratan sin cambios de formato.
- Los enumeradores del enum anónimo se importan como constantes, como ya ocurre
  con un enum anónimo de ámbito de archivo (`CIdLoose`).

Previsto, no verificado: `HConst`, `HNamed`, `HNamedType` y `CNamed` pasan a
ejecutar.

### Parte B. Tipo del campo con enum anónimo (sin cambio de metadata)

Un campo C cuyo tipo es un enum anónimo se proyecta con su tipo entero
subyacente (`enumUnderlying`, `int` en x64 MSVC), que es el tipo compatible
según C11 6.7.2.2. Calificadores y tamaño se toman del campo, como en los demás
escalares. La identidad Clang del tipo sigue en el `CppType` del campo y no se
inventa un nombre NativeSharp.

Alternativa descartada: un tipo enum NativeSharp con nombre sintético (por
ejemplo `CNestAnonField.level`). Inventaría una identidad que C no tiene, y no
cubre `enum { A, B } x, y;`, donde dos campos comparten un mismo enum anónimo.

Previsto, no verificado: `HLevel`, `HLevelInt` y `CInternal` pasan a ejecutar.

### Parte C. Biblioteca precompilada (cambia la metadata: no implementar todavía)

El consumidor sin encabezado necesita el tipo del campo y los enumeradores
(`CAnon` compara con `CNestHigh`). Propuesta:

1. Antes de capturar el snapshot, el productor normaliza el tipo del campo: se
   eliminan el USR del enum anónimo y su nombre calificado, y se conservan la
   marca anónima y el tipo subyacente. Así ninguna ruta ni desplazamiento entra
   en NSMETA.
2. Una entrada nueva del catálogo, "enum anónimo de campo", con clave en la
   identidad del campo propietario (clave del record, nombre del campo y USR del
   campo) y la lista de enumeradores (nombre, valor decimal, bits, signo, tipo).
   Un mismo enum usado por varios campos se emite una sola vez y lo referencian
   todos.
3. El consumidor restaura los enumeradores como constantes del módulo productor
   con `reconstructCEnumConstants` (`CppSemanticRecord.cpp:1181`).
4. Conciliación con un encabezado propio del consumidor (`CAnonHeader`): por USR
   del campo, nombre y valor de cada enumerador; nunca por el USR del enum
   anónimo. Discrepancias → error.

El cambio de formato es el mismo tipo de decisión que el de
[T08-002-PROPUESTA-METADATA-C.md](T08-002-PROPUESTA-METADATA-C.md). La
recomendación es una sola subida de versión para los dos cambios.

Sin la parte C, la alternativa mínima es mantener el rechazo del productor con
un diagnóstico que nombre el campo y sugiera dar nombre al enum (con `typedef`
ya funciona: `CAnonKind`, restaurado en O0–O3).

## Casos

Preparados y ejecutados con su resultado ACTUAL: los diez de
`enum-campo/casos.txt`. La columna PROP fija el resultado esperado: todos
ejecutan. `validar.cmd` con `NSC_T08_PROPUESTAS=1` exige PROP.

Para añadir al implementar:

- `enum { A, B } x, y;`: dos campos y un solo enum.
- Arreglo y puntero de enum anónimo como campos.
- Consumidor con una copia del encabezado de otro nombre: prueba que la
  conciliación no depende del USR del enum anónimo.
- Dos records cuyos enums anónimos repiten un nombre de enumerador. Debe dar el
  conflicto existente `conflicting C enumerator declarations` (sonda 4).
- Comprobación de que el NSMETA no contiene la ruta absoluta del encabezado.
- O0–O3 de `CAnon`, `CAnonHeader` y `CNamed` con restauración por la ruta del
  driver (`--verify-driver-restored-c-usr`).

## Archivos afectados

| Parte | Archivo | Cambio | Compartido con |
|---|---|---|---|
| A | `src/Interop/Cpp/ClangSession.cpp` (`collectEnums`) | recorrer `RecordDecl` en C | captura Clang |
| B | `src/Interop/Cpp/CppSemanticRecord.cpp` (proyección de campos, línea 262) | entero subyacente para enum anónimo C | **layout de records** |
| B | `include/nativesharp/Interop/CppSemanticEnumType.h` | posible auxiliar para enum anónimo | tipos C |
| C | `src/Interop/Cpp/CppSemanticRecord.cpp` (cierre 928-936, comprobación 1062-1080, restauración 1181) | entradas de enum anónimo de campo | **layout de records** |
| C | `src/Interop/Internal/CppCRecordMetadataCodec.cpp/.h` | entrada nueva y normalización | **metadata** |
| C | `src/Interop/LibraryMetadataVerification.cpp`, `LibraryMetadataConstruction.cpp`, `include/nativesharp/Interop/LibraryMetadata.h` | versión | **metadata** |
| — | `tests/unit/CppCIdentityTests.cpp`, `pruebas-tanda-08-identidad-c/enum-campo` | pruebas | este carril |

`CppSemanticRecord.cpp` y el codec también los tocan el trabajo de layout de
records y el de TLS/globales. Antes de integrar hay que comparar las ramas; no
se modificaron aquí.
