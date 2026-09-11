# 1.0.1 — Arreglos de tipos referencia en la superficie binaria

## Resultado

Los arreglos de clases e interfaces públicas se consideran tipos de contrato
materializables en parámetros, retornos y accessors públicos. NSMETA y la tabla
de exports PE describen ahora los mismos símbolos para esta categoría completa.

La corrección cubre los 16 símbolos informados por SaruGL: todos compartían una
firma con `PublicClass[]` o `PublicInterface[]`.

## Causa raíz

`metadataBaseTypeKey()` ya retiraba temporalmente el sufijo `[]` para resolver
el tipo elemento, y `sourceTypeForMetadataKey()` ya lo restauraba correctamente
al reconstruir la declaración NativeSharp. A pesar de ello,
`canMaterializeContractType()` contenía una prohibición independiente para
cualquier `[` cuando el elemento era una clase o interfaz.

Esa condición producía una asimetría:

- el frontend reconocía el miembro como API pública y el generador PE exportaba
  su símbolo;
- la publicación basada en NSMETA descartaba el mismo miembro únicamente por
  el envoltorio de arreglo;
- `--verify-library-artifacts` terminaba reportando el símbolo como export
  inesperado.

Los arreglos de builtins y structs no pasaban por esa prohibición específica,
lo cual explica que `byte[]`, `Vector2[]`, `Vector3[]` y `Color[]` ya fueran
aceptados.

## Archivos e implementación

- `src/Driver/Driver.cpp`, en `canMaterializeContractType`, elimina el rechazo
  especial de arreglos para clases e interfaces.
- La validación sigue resolviendo y comprobando el tipo elemento:
  - una interfaz debe ser `public` y públicamente alcanzable;
  - su carrier de referencia debe coincidir en tamaño y alineación con el ABI;
  - una clase conserva la misma validación de carrier usada por su forma sin
    arreglo;
  - los sufijos de arreglo se conservan al reconstruir la fuente pública.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp` añade una DLL que publica:
  - `IService[]` como parámetro y retorno;
  - `Item[]` como parámetro y retorno;
  - propiedades públicas con ambos tipos de arreglo.
- La regresión exige concordancia conjunta entre DLL, import library y NSMETA,
  por lo que cubre tanto métodos ordinarios como accessors de propiedades.

No se modificaron SaruGL, bindings, tutoriales ni repositorios externos.

## Por qué es un solo defecto

Los 16 exports no representan 16 reglas diferentes. Todos son rechazados por
la misma condición al clasificar su tipo de firma. Corregir el predicado central
evita excepciones por clase, interfaz, método o propiedad y mantiene una única
definición de materialización para toda la superficie precompilada.

## Verificación prevista

La regresión queda guardada para certificación posterior. En este carril sólo
se compilará e instalará NSC; no se ejecutarán SaruGL, bindings, tutoriales ni
baseline.

## Commit de referencia

### Español

**Título:** Publica arreglos de referencias en NSMETA

**Descripción:** Considera materializables los arreglos de clases e interfaces
públicas en parámetros, retornos y propiedades. Alinea el conjunto de símbolos
requeridos por NSMETA con los exports PE y añade una regresión DLL para toda la
categoría.

### English

**Title:** Publish reference arrays in NSMETA

**Description:** Treat arrays of public classes and interfaces as materializable
in parameters, returns, and properties. Align the NSMETA required symbol set
with PE exports and add a DLL regression for the complete category.

## English summary

Arrays now inherit the materializability of their public reference element
type. Public APIs using class or interface arrays are published consistently by
NSMETA and the PE export pipeline.
