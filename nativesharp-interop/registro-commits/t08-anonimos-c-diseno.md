# T08-010/T08-011: diseño de miembros anónimos C11 con encabezado y con biblioteca precompilada

## Estado

DISEÑO PREPARADO, NO IMPLEMENTADO. Requiere cambiar el contenido y la versión del catálogo C de NSMETA, que es
superficie protegida. Implementarlo sólo para el camino con encabezado haría distintos el consumo directo y el
precompilado, cosa que está prohibida; por eso no se implementa ninguna parte.

## Problema (HECHO COMPROBADO)

`ProbeAnonymous` (`pruebas-tanda-08-layout-clang`) rechaza `value->i`, `value->x` y `value->y` de

```c
typedef struct LAnonymous { int tag; union { int i; float f; }; struct { short x; short y; }; } LAnonymous;
```

Causas, por lectura de código:

1. `ClangSession.cpp`, `collectRecords`: recorre `record->fields()`; el miembro anónimo llega como campo sin
   nombre cuyo tipo es un record sin nombre; los `IndirectFieldDecl` promovidos no se leen y los records
   anónimos se descartan (`record->getName().empty() && !anonymousAlias`).
2. `CppSemanticRecord.cpp`, `projectCppSemanticInstanceField`: rechaza todo campo con nombre vacío.
3. `CppCRecordMetadataCodec.cpp`, `validateRecords`: cada record del catálogo necesita una `TypeIdentity`
   válida, con nombre; un record anónimo no se puede guardar.

## Modelo de destino: el que NativeSharp ya tiene (puntos 324–326)

- Tipo anidado sintetizado por cada agregado anónimo, con identidad interna determinista.
- Campo carrier oculto en el contenedor, con el tipo sintetizado.
- Aliases promovidos no físicos en `ProjectMemberModel` (`isAnonymousMemberPromotion`,
  `promotedSourceMemberOrdinal`, `anonymousCarrierMemberOrdinals`).
- `ProjectObjectLayoutModel::buildPromotedFieldLayouts` suma el offset del carrier y el del campo fuente
  después de los layouts físicos; los aliases no ocupan almacenamiento ni se inicializan ni destruyen.
- Conflictos de nombre directo o entre promociones ya se diagnostican.

El diseño reutiliza ese modelo para C; no crea otro mecanismo de promoción.

## Diseño

### 1. Captura en la sesión Clang (`ClangSession.cpp`)

- Para un record C completo, cada `FieldDecl` con `isAnonymousStructOrUnion()` produce:
  - un `CppRecordField` carrier con nombre sintetizado `__nsc_c_anonymous_<índice de campo>`, offset de
    `getFieldOffset`, tipo del record anónimo y una marca nueva `isAnonymousCarrier`;
  - un `CppRecordDeclaration` para el record anónimo, con `containingRecords` = cadena del contenedor,
    nombre igual al del carrier, `kind` struct o union, layout de `getASTRecordLayout` y sus campos;
    se repite para anidamientos sin límite fijo, con una pila explícita (sin recursión).
- La identidad depende sólo del contenedor y del índice del campo, no de offsets de archivo; por eso es igual
  con encabezado y con metadata, y estable entre unidades que incluyen el mismo encabezado.
- Los `IndirectFieldDecl` no se guardan: la promoción se deriva del carrier, igual que en NativeSharp.

### 2. Proyección (`CppSemanticRecord.cpp` y proveedores de tipos)

- El record anónimo se importa como tipo C anidado del contenedor (`CppImportedTypeOrigin::CStruct/CUnion`) con
  el layout de Clang, igual que cualquier record C completo.
- El carrier se proyecta como campo ordinario de ese tipo, no visible por nombre fuente.
- Por cada campo del record anónimo (y recursivamente de carriers anidados) se crea un alias promovido con la
  misma regla de conflictos del punto 326. Los campos de bits y los arreglos interiores siguen las reglas ya
  vigentes para campos C.
- Un struct con una union anónima tiene miembros superpuestos sólo a través del alias; el layout físico del
  contenedor no cambia, así que no hace falta permitir campos físicos superpuestos en un struct.

### 3. Catálogo C de NSMETA (cambio protegido)

- Nueva versión de sobre (5) que añade, como las extensiones de procedencia y formato flotante, un bloque con:
  los índices de campo que son carrier y las identidades de los records anónimos sintetizados.
- `validateRecords` acepta nombres `__nsc_c_anonymous_<n>` sólo para records listados en ese bloque.
- Las versiones 1–4 se siguen leyendo sin cambios. Una biblioteca vieja no promueve: su consumidor conserva el
  comportamiento actual.
- Conciliación entre una observación nueva (con carrier) y un catálogo viejo (campo sin nombre): el carrier
  sintetizado se compara con el campo sin nombre en tipo, offset y tamaño; no debe producir conflicto, para que
  un programa con encabezado y biblioteca vieja siga compilando (invariante de ausencia).
- Impacto: cambia el contenido del catálogo sólo de records con miembros anónimos; el resto de los catálogos
  queda byte por byte igual, comprobable con la canonicalización que el decodificador ya exige.

## Pruebas previstas (archivos nuevos)

1. Struct con union anónima y struct anónimo: lectura y escritura cruzadas con C, `sizeof` y `offsetof` del
   contenedor; directo, precompilado con encabezado y sólo-metadata, O0–O3.
2. Anónimo anidado dos niveles; anónimo dentro de union; anónimo en record `pack(1)` (alineación de acceso
   comparada con Clang); campos de bits dentro del anónimo.
3. Conflicto de nombre entre un campo directo y uno promovido: diagnóstico.
4. Biblioteca precompilada con catálogo versión 4 consumida junto con el encabezado: sigue compilando y sin
   promoción desde metadata.
5. Comparación del catálogo de un record sin anónimos antes y después: idéntico.
6. Alineación IR de cada acceso promovido contra Clang con `verificar_alineacion_clang.py`.

## Autorización necesaria

Cambio del catálogo C de NSMETA a la versión 5 descrita y uso de nombres sintetizados en identidades C. Sin esa
autorización no se implementa ninguna parte.
