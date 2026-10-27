# Congelamiento 3.0, elemento 139 — constructores de especializaciones cerradas publicadas

Fecha: 2026-10-27. Revisión de partida: `15dc778d5` (development).

## Nota de trabajo

```text
HEAD: 15dc778d5
Elemento: 139 del plan (frontera entre 130a y la publicación NSMETA)
Contrato vigente: elemento 130a (doc 49); decisión de Alberto (2026-10-27), abajo
Documento histórico que podría contradecirlo: doc 49, que deja todo constructor de instancia de un genérico
  cerrado bajo demanda, también en una biblioteca; este elemento lo acota
Archivos exactos que se modificarán:
  src/Frontend/Bodies/MethodBodyBuildCoordinator.cpp
  tests/unit/Freeze30ContractTests.cpp
  docs/3.0/decisiones-congelamiento/59-CONSTRUCTORES-DE-ESPECIALIZACIONES-PUBLICADAS.md
```

## Problema

130a dejó bajo demanda todo constructor de instancia de un genérico cerrado, y `seedDemandedGenericMethods`
siembra en una biblioteca sólo los métodos públicos. Pero NSMETA publica todo tipo raíz públicamente alcanzable,
incluidas las especializaciones cerradas de sus plantillas públicas, y la verificación de lectura exige una
identidad física para cada constructor público o protegido de esos tipos. Una biblioteca que publica una
especialización sin construirla ya no se puede emitir. Con el nsc de `15dc778d5`:

```text
nsc: error: Stage 48.2 static-library metadata generation failed: metadata producer read-back verification
failed: public callables have no physical identity; refusing to emit unusable NSMETA
  ...LinkedList__g1__string__he32f6eb370c93c80.LinkedList__g1__string__he32f6eb370c93c80()
  ...List__g1__string__h2d671804fd219d31.List__g1__string__h2d671804fd219d31()
  ...List__g1__string__h2d671804fd219d31.List__g1__string__h2d671804fd219d31(builtin:int)
```

Es `NativeSharp.Core.Collections.Generic` (`OwnedStringCollections` publica `List<string>` y
`LinkedList<string>`). Sonda mínima: `A:\temp\nsc-dec\d44` (`Store<string>` en una firma pública).

Se descartó quitar la exigencia de identidad de NSMETA: la biblioteca emite, pero el consumidor no puede
construir el tipo publicado. Lo enlaza con una declaración sintética que sólo lleva los miembros públicos con
símbolo, y el modelo de enlace es de dueño único. Materializar el constructor en el consumidor exigiría clonar
la plantilla y enlazar con linkonce_odr/COMDAT.

## Decisión de Alberto (2026-10-27)

- Especialización cerrada interna o usada sólo dentro de un ejecutable: constructores bajo demanda (130a).
- Especialización cerrada que forma parte de la superficie pública de una biblioteca: sus constructores
  públicos y protegidos son obligación del productor y reciben identidad física.
- Los constructores privados sólo se compilan si los alcanza un constructor público sembrado.
- No se siembran plantillas abiertas ni especializaciones internas.
- No se omite la identidad física en NSMETA.
- Que un constructor público inválido haga fallar una especialización publicada es correcto. La garantía «un
  constructor inválido no seleccionado no se analiza» se limita a constructores no publicados.

## Contrato

`seedDemandedGenericMethods` siembra en una biblioteca también los constructores de instancia públicos y
protegidos que `isDeferredGenericMethod` deja bajo demanda, cuando su tipo es públicamente alcanzable
(`declaringTypeIsPubliclyReachable`).

- Es el mismo criterio con el que NSMETA publica el tipo (`isPubliclyReachableRootType`) y exige la identidad.
  Una especialización cerrada de una plantilla pública del proyecto raíz se publica aunque ninguna firma
  pública la nombre, así que también recibe sus constructores.
- Un constructor privado entra por el cierre de llamadas de los sembrados (`this(...)`, `base(...)`), como
  cualquier otro privado.
- Un ejecutable no pasa ordinal de superficie pública y conserva 130a sin cambio.
- `LibraryMetadataConstruction` no cambia: la identidad sigue siendo obligatoria.

## SCOPE LOCK

- **Cambia.**
  - Una biblioteca analiza y baja los constructores públicos y protegidos de las especializaciones cerradas que
    publica.
  - Uno inválido para su argumento de tipo se rechaza en el productor.
- **Idéntico.**
  - Ejecutables.
  - Plantillas abiertas, que el consumidor instancia y cuyo constructor demanda (doc 49).
  - Especializaciones de plantillas no públicas.
  - NSMETA y su verificación.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol.

Sondas `A:\temp\nsc-dec\d45` (`run.cmd`, `-O0`):

- **L1** emite. Publica `Store<string>`, con `Store(int) : this(size, 2)`, `private Store(int, int)` y
  `Store()`. Su `Inner<string>` no se publica y tiene un `Inner()` inválido para string que nadie usa.
- **L2** se rechaza en el productor: «no method in the method group exactly matches delegate signature».
  Publica `Store<string>` con `public Store() { k = Keys.Of; }`, inválido para string.
- **C1**, consumidor de L1, sale con 314971 (71 + 9·100 + 4·1000 + 31·10000). Corre `new Store<string>(5)`,
  `new Store<string>()`, `Api.Hidden()` y su propio `new Store<int>(1)`.

El 71 sale de la cadena pública → privada en el productor; el 31, de la plantilla instanciada en el consumidor.
En la base `15dc778d5` las mismas fuentes (prueba Freeze30, abajo) fallan con «public callables have no physical
identity», también L2.

`NativeSharp.Core.Collections.Generic` (`d45\cg.cmd`) ya no falla por identidad física. Ahora la verificación
de lectura se detiene en el defecto siguiente, que esa falla ocultaba:

```text
metadata member '...LinkedListEnumerator__g1__string__h5ed9428842eb9426.get_Current()' carries ref-return
outside an ordinary method or indexer getter
```

Es el `ref T Current` de P3 contra `verifyReferenceReturnMemberMetadata`. Se corrige en el elemento 140, junto
con la validación de `List<string>` y `LinkedList<string>` publicados y su consumidor.

Prueba nueva: `Freeze30ContractTests::testPublishedSpecializationConstructors`, con L1, L2 y C1. Se ejecutó
el binario completo en `A:\temp\nsc-dec\bt`:

- Este árbol: 1079 de 1085 comprobaciones. Pasan F139Library y las de 130a, F130aLibrary y
  F130aDictionaryClassKey/HashSetClassKey entre ellas.
- Base `15dc778d5` con la prueba nueva: fallan además las dos de F139Library («public callables have no physical
  identity»).

Las seis que fallan en los dos árboles son del entorno de `bt`:

- cuatro de modo archivo: no hay `lib/NativeSharp.Core` encima del compilador;
- P3SortedDictionaryGet `-O0`/`-O2`: el proyecto de la prueba no referencia las colecciones.

Resto: `seq.cmd f139` contra `f135a` es idéntico en todo. Incluye:

- `all.cmd`;
- `asynccases`, `gencases` y `loopcases`;
- los 66 programas de Freeze30 en `-O0` y `-O2`;
- los 27 proyectos de `tests/cases`;
- d28;
- las bibliotecas.

## No verificado

- Batería completa y unitarias: gate final.
- Un constructor protegido sembrado (la regla lo cubre igual que a uno público; no tiene sonda propia).
