# T08-D19: dependencias no públicas de plantillas publicadas, en NSMeta

Fecha: 2026-11-09. Casillas: T08-D19 y T08-018-03c1
([decisión anterior](t08-colisiones-c.md#decisión-pendiente-t08-018-03c1)).

## Problema

Una plantilla pública de una clase precompilada que usa un miembro **no público** de su clase no funcionaba en el
consumidor, porque el miembro no se publicaba (regla DLL-P4):

- sin una declaración C homónima, el consumidor rechazaba la instanciación: «unknown identifier 'other'»;
- con una global o una función C homónima visible, la plantilla se unía **en silencio** a la declaración C. El
  campo privado `counter = 7` leía la global C (40) y el método privado `twice` llamaba a la función C.

## Causa raíz

- `collectMetadataMemberOrdinals` (`LibraryMetadataConstruction.cpp`) solo publicaba miembros públicos y
  protegidos.
- La materialización del consumidor (`hasConsumerCallableAccessibility`, `MetadataSourceMapping.cpp`) y la lista
  de exportaciones de una DLL (`DriverArtifactGeneration.cpp`) seguían la misma regla.

En el consumidor, el cuerpo transportado de la plantilla no encontraba el miembro en su clase y la búsqueda seguía
hacia el ámbito global.

## Decisión aplicada

Se aplica la opción 1: publicar en NSMeta los miembros privados que usan las plantillas publicadas.

- **Qué se publica.** Un miembro privado o interno (campo o método, estático o de instancia) cuando una plantilla
  que la biblioteca publica cumple dos condiciones:
  - está declarada dentro de su tipo, o de un tipo anidado en él;
  - lo nombra.

  Se calcula comparando los identificadores del texto de la plantilla con el nombre del miembro. Es una
  sobreaproximación segura: un homónimo solo agrega un miembro más.
- **Accesibilidad.** Se conserva: el consumidor materializa el miembro como `private` o `internal`. Solo el código
  dentro del tipo (el cuerpo de la plantilla) puede usarlo, y el código del consumidor recibe «member '…' is not
  accessible».
- **Lo que no se publica.** Un miembro privado que no nombra ninguna plantilla publicada sigue sin publicarse.
- **DLL NativeSharp.** Exporta los símbolos de esos miembros: métodos y accesores de campos estáticos.

NSMeta no cambia de formato: estos miembros usan el registro de miembro existente con su accesibilidad real.

## Cambio

- `LibraryMetadataConstruction.cpp`: nueva `templateDependencyMemberOrdinals`, declarada en el nuevo
  `Interop/Internal/TemplateDependencyMembers.h`. `collectMetadataMemberOrdinals` publica esos miembros.
- `MetadataSourceMapping.cpp`: el consumidor materializa los miembros privados e internos que trae NSMeta, con su
  accesibilidad.
- `DriverArtifactGeneration.cpp`: la DLL exporta los métodos y los accesores de campos estáticos de esos miembros.

## Pruebas

**Nueva:** `pruebas-tanda-08-colisiones-c/validar-dependencias-privadas.py`. Aprobada en O0–O3, con biblioteca
estática y con DLL NativeSharp. El consumidor ve el encabezado C con `counter` y `twice`.

- **Casos positivos.** La plantilla usa, y comprueba:
  - `counter` (campo estático privado, homónimo de la global C): lee 7 y escribe 8, y la global C sigue en 40;
  - `twice` (método estático privado, homónimo de la función C): `twice(2)` da 14;
  - `other`, campo privado sin colisión;
  - `inner`, campo estático interno;
  - un campo y un método privados de instancia.

  El consumidor, además, usa la global y la función C, que siguen siendo las de C.
- **Rechazos:**
  - el consumidor no puede usar `counter`, `twice` ni `inner`: «member '…' is not accessible»;
  - un campo privado que ninguna plantilla nombra no se publica: «has no compatible member named 'hidden'».

La sonda `A:\nsc-interop-build\claude\sondas\openarr\sonda27b.py` da ahora 7 en los cuatro casos: público, privado,
privado sin colisión y método privado.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d19.cmd`. La primera ejecución tuvo tres fallos, que se corrigieron:

- la suite nueva enlazaba dos copias de la biblioteca C en O1–O3. Se corrigió la prueba: el consumidor de un
  productor estático toma las definiciones C del productor;
- dos nombres de validador del guion estaban mal escritos (`sombras-plantilla-c`);
- `nombres-metadata/validar.cmd` es la prueba de aceptación preparada en `t08-nombres-metadata-diseno.md`, que
  falla desde entonces por `NAlias` y `NMood` no publicados. La sustituyó `validar-referenciados.cmd`, que pasa.

Contenido de la regresión:

- la suite nueva y `colisiones-c/validar-colisiones.py`;
- productos DLL NativeSharp: `rutas-unicode/validar-dll-unicode.py` y `validar-transitivas-homonimas.py`;
- `typedef-c/validar-typedef-productores.py`;
- los validadores `dependencias-template`, `sombras-plantilla-c` (`o0-sombras`, `o0-inicializadores` y ámbito),
  `inline-template`, `global-static-template`, `nombres-metadata/validar-referenciados.cmd` y
  `meta-c/validar-productor.cmd`;
- la batería `identidad-c\validar.cmd`;
- las pruebas unitarias de metadata (`library-metadata-version-tolerance`, `library-metadata-merger`,
  `public-surface-emitter`) y `nsc_cpp_session_tests`, recompiladas;
- `stage48_2.library-metadata`: 442 de 444 comprobaciones pasan. Las dos que fallan abren el archivo histórico
  `tests/fixtures/nsmeta/historical-1.46.nsmeta`, que no existe en el repositorio. Es ajeno a este cambio y
  corresponde al contrato único de NSMeta (T08-D24);
- el conjunto de D02.
