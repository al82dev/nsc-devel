# 1.0.1 — Contratos de interfaz reconstruidos desde NSMETA

## Resultado

La fuente pública reconstruida desde NSMETA reconoce los miembros por el tipo
contenedor. Los métodos, propiedades e indexadores de una interfaz se emiten
siempre como contratos, aunque sus registros no conserven `kAbstract` y sí
contengan banderas de despacho como `virtual`.

## Causa raíz

La rama dedicada a interfaces ya emitía métodos sin cuerpo y los accessors de
propiedades e indexadores con `;`. Sin embargo, el emisor común de accessors
copiaba antes modificadores propios de implementaciones de clase: `static`,
`sealed`, `new`, `override`, `abstract` o `virtual`.

El parser Stage 38 sólo admite `public`/`abstract` y después el tipo del
contrato. Por eso una interfaz cuya metadata describía un accessor como
`virtual` fallaba con el diagnóstico de que las interfaces sólo contienen
contratos, aun cuando el accessor no tenía cuerpo.

## Archivos e implementación

- `src/Driver/Driver.cpp`, en `buildPrecompiledPublicSource`, conserva los
  modificadores existentes para clases y structs, pero los omite cuando
  `emitAccessorContracts` opera dentro de una interfaz.
- La terminación `get;`/`set;` continúa determinada por el contexto de
  interfaz, no por `kAbstract`.
- La rama de métodos de interfaz ya usaba el tipo contenedor y permanece como
  contrato `);`.
- Los eventos no comparten esta ruta: Stage 38 no los admite como miembros de
  interfaz, por lo que no se inventó una extensión del lenguaje ni un cuerpo.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp` añade un productor con método,
  propiedad e indexador. La fixture reescribe sus miembros para quitar
  `kAbstract` y añadir `virtual`; la verificación debe materializarla usando
  exclusivamente `LibraryMetadataTypeKind::Interface`.

No se modificaron SaruGL, bindings ni scripts externos.

## Verificación prevista

La regresión dirigida certifica una NSMETA válida cuyo contrato de interfaz no
depende de `kAbstract`. De acuerdo con el flujo solicitado, no se ejecutarán
bindings, SaruGL ni baseline desde este carril.

## Commit de referencia

### Español

**Título:** Reconstruye interfaces NSMETA por tipo contenedor

**Descripción:** Emite propiedades e indexadores de interfaces como contratos
sin copiar modificadores de implementación desde sus miembros. La
materialización deja de depender de `kAbstract` y conserva sin cambios las
declaraciones de clases y estructuras.

### English

**Title:** Rebuild NSMETA interfaces from declaring type

**Description:** Emit interface properties and indexers as contracts without
copying implementation modifiers from their members. Materialization no longer
depends on `kAbstract` while class and struct declarations remain unchanged.

## English summary

NSMETA public-source reconstruction now treats the declaring interface kind as
authoritative. Methods and accessors remain contracts even when member flags
look virtual rather than abstract.
