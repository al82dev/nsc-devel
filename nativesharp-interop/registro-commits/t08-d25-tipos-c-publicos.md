# T08-D25: una biblioteca NativeSharp publica los tipos C de su interfaz

Fecha: 2026-11-09. Indicación del usuario: una biblioteca NativeSharp tiene que poder publicar los tipos C importados
que forman parte de su interfaz, aunque el productor todavía no los use en ninguna firma pública. Las condiciones son:

- el consumidor los usa desde la biblioteca precompilada sin volver a incluir el encabezado;
- no se omiten tipos necesarios en silencio ni se publican declaraciones internas sin examinar las consecuencias;
- se usa el contrato vigente de NSMeta, sin versiones nuevas.

## Problema

NSMeta publicaba solo los tipos C a los que llegaba la superficie publicada:

- firmas, campos, tipos y exportaciones;
- dependencias de las plantillas;
- la clausura de esos tipos (records de los campos, tipo base de los typedef).

En `pruebas-tanda-08-nombres-metadata`, `CNamesProducer.nsmeta` no contenía `NAlias` ni `NMood`. El consumidor sin
encabezado fallaba con «unknown identifier 'NAlias'».

## Cómo se identifica la superficie

Primero se fijó la regla, y después se implementó.

| Pregunta | Regla |
|---|---|
| ¿Quién decide que un encabezado es interfaz? | El productor, por referencia: `<CppLibraryReference … PublicTypes="Header" />`. Sin el atributo, el catálogo es exactamente el de antes, así que las bibliotecas existentes y sus consumidores no cambian. |
| ¿Qué tipos entran? | Todo record C (`struct`/`union` con etiqueta o con nombre por typedef), enum con nombre y `typedef` declarado en el propio encabezado o en un encabezado bajo el mismo directorio (subdirectorios incluidos). El directorio es el del encabezado de la referencia. |
| ¿Y los encabezados transitivos de fuera del directorio (CRT, SDK, otras bibliotecas)? | No forman parte de la superficie. Solo aportan por la clausura de un tipo publicado: un campo `ExtUsed` de `NHolder` se publica, `ExtOnly` del mismo encabezado no. Así no se publican `size_t`, `uint32_t` ni el resto del SDK. |
| ¿Declaraciones internas? | Los identificadores reservados de C (`__x`, `_X`) son de la implementación y no se publican; tampoco los records anónimos sin nombre ni las declaraciones incompletas sin definición. Si un tipo publicado los necesita, siguen llegando por la clausura. |
| ¿Funciones, globales y macros? | Fuera del alcance: la indicación es sobre tipos. Una función o una global sigue publicándose solo si la usa la superficie NativeSharp. |
| ¿Dónde se puede usar? | Solo en proyectos `Library` o `SharedLibrary` (un ejecutable no publica NSMeta) y solo con `Language="C"`. Cualquier otro uso se rechaza al cargar el proyecto. |

## Duplicados y conflictos

| Caso | Comportamiento |
|---|---|
| El mismo tipo por dos referencias del productor (encabezados que incluyen un tercero común), o por una firma y por la superficie | Se publica una vez: records y enums por su identidad; typedefs por módulo y nombre. |
| Dos declaraciones distintas con el mismo nombre en la superficie del productor | El productor se rechaza: «CppLibraryReference PublicTypes: C type 'NAlias' has different declarations in '…/api.h' and '…/clash.h'». NSC no elige ninguna. Etiquetas y typedef son espacios de nombres separados, como en C, así que `typedef struct NRecord {…} NRecord;` no es conflicto. |
| El consumidor incluye un encabezado propio que declara el tipo de otra forma | Lo rechaza la regla existente: «conflicting C/C++ type alias 'NAlias' … reconciled=no». |
| El consumidor incluye el mismo encabezado y además usa la biblioteca | La reconciliación existente funciona y el programa se ejecuta. |
| Dos bibliotecas publican `NMood` con valores distintos | El consumidor que lo usa se rechaza («ambiguous namespace state reference 'NHappy'»). |
| Dos bibliotecas publican la misma declaración | El 2026-11-09 el consumidor que la usaba se rechazaba («conflicting C/C++ type alias 'NAlias'»). Desde T08-D26 es un solo tipo y el consumidor la usa. |

## Cambios

- **`CppLibraryReference` y su descriptor:** `publicTypes`.
- **`ProjectLoader.cpp`:**
  - acepta el atributo `PublicTypes`, que solo admite el valor `Header`;
  - lo rechaza en ejecutables y fuera de `Language="C"`.
- **`src/Interop/Cpp/CppPublicHeaderTypes.h` (nuevo):**
  - `selectPublicHeaderTypes` recorre los inventarios de records, enums y typedefs de Clang de cada referencia
    `PublicTypes`;
  - aplica la regla del directorio y la exclusión de reservados;
  - detecta los conflictos por nombre y por espacio de nombres C;
  - devuelve las semillas de records y enums y los candidatos de typedef.
- **`CppSemanticRecord.cpp`, `provideCppCRecordMetadataCatalog`:** agrega esas semillas a las del catálogo actual,
  antes de calcular la clausura. Los typedef pasan por la misma captura que los de plantilla, de modo que la
  clausura de sus records y enums es la de siempre.
- **NSMeta:** el formato y la versión no cambian. Cambia solo el contenido del catálogo C de las bibliotecas que
  declaran el atributo.
- **Caché:** el atributo está en el archivo de proyecto, que forma parte de la identidad de la caché. Si se quita,
  la caché falla y no se restaura un catálogo con más tipos de los declarados.

## Pruebas

**Nueva:** `pruebas-tanda-08-nombres-metadata/validar-tipos-publicos.py`.

- **Precompilado sin encabezado, O0–O3:** `NAlias`, `NMood` (con `NSad` y `NHappy`), `NPoint` (encabezado del mismo
  directorio), `NHandle` y `NHolder.used` (campo de un encabezado externo). Los tamaños y valores coinciden con los
  de la biblioteca C.
- **Consumidor con el encabezado y la biblioteca:** correcto.
- **Producto DLL:** el consumidor sin encabezado usa los tipos publicados.
- **No publicados:**
  - `ExtOnly` (encabezado externo, no requerido);
  - `_InternalRecord` (reservado);
  - `NAlias` cuando la biblioteca no declara `PublicTypes`.
- **Duplicados y conflictos:** los seis casos de la tabla anterior.
- **Cargador:** `PublicTypes="All"`, en un ejecutable y con `Language="C++"`.
- **Caché:** `['not-found', 'hit', 'not-found']` (repetir acierta; quitar el atributo falla).

**Actualizada:** `pruebas-tanda-08-nombres-metadata/validar.cmd`, la prueba de aceptación del diseño de T08-020.
`Producer.nsproj` declara ahora `PublicTypes="Header"`. Antes fallaba en `NAlias`/`NMood`; ahora pasa con y sin
encabezado (`Shared` y `Metadata`) en O0–O3, y las cuatro sondas por posición compilan.

**Sin cambios y pasa:** `validar-referenciados.cmd`, con los tipos publicados por firmas y sus conflictos.

Las tres pruebas forman parte del gate complementario.

## Limitación que queda: dos productores con la misma declaración

**Resuelta el 2026-11-10 por T08-D26** ([detalle](t08-d26-identidad-tipos-c-productores.md)). Lo que sigue
describe el estado del 2026-11-09.

Cada biblioteca precompilada conserva su propia identidad para sus tipos C. Si dos bibliotecas publican el mismo
`NAlias` del mismo encabezado, el consumidor que lo nombra recibe «conflicting C/C++ type alias».

Esto ya ocurría antes de D25 con tipos publicados por firmas. Sonda: `claude\sonda-d25\dos-firmas`, dos copias de
`ProducerReferenced`, sin `PublicTypes`.

La reconciliación existente une un tipo C vivo con uno precompilado; no une dos precompilados. Resolverlo es decidir
si dos productores comparten la identidad NativeSharp de un tipo C idéntico. Eso cambia la identidad de tipos entre
módulos, así que se presenta como decisión aparte (T08-D26).

Hoy un consumidor no puede nombrar ese tipo si referencia las dos bibliotecas. Incluir el encabezado él mismo
tampoco lo resuelve: la misma sonda (`C2`) da el mismo conflicto.

## Validación

Compilador `A:\nsc-interop-build\tanda1-ON`; ctest mide ese mismo compilador (`NSC_TEST_COMPILER`).

1. **Pruebas dirigidas** (`claude\scripts\dirigidas-d25.cmd`): `validar-tipos-publicos.py`, `validar.cmd` y
   `validar-referenciados.cmd`. Resultado: 0.
2. **Gates de interoperabilidad**, que reconstruyen el compilador y los 444 ejecutables de prueba antes de
   ejecutarse:
   - `validar-tanda-08.cmd`: código 0;
   - `validar-tanda-08-complementaria.cmd`: código 0, con las tres pruebas de D25 incluidas.
3. **ctest:** 29 pruebas, 26 pasan. Las mismas 3 fallan que antes de D25, y ninguna es de interoperabilidad C:
   - `stage48_2`: 2 de 444, el fixture 1.46 que no existe;
   - `item341`: conflicto con el `throw` chequeado;
   - `item347`: desbordamiento de pila en la ruta SIMD, anterior a la tanda.

   Detalle en [pruebas generales](t08-pruebas-generales-item341-item347.md).
