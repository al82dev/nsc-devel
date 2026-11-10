# T08-D26: el mismo tipo C publicado por dos productores precompilados

Fecha: 2026-11-10. Indicación del usuario:

- reconciliar las declaraciones C equivalentes que publican productores distintos, para que un consumidor use el
  mismo tipo desde las dos bibliotecas;
- comprobar la identidad y la compatibilidad real, no unir dos tipos solo porque coinciden el nombre o el USR;
- mantener el rechazo de las declaraciones incompatibles;
- probar con bibliotecas estáticas y DLL, con y sin encabezado en el consumidor, y con conflictos.

## Problema

Cada biblioteca precompilada restaura sus tipos C con su propia identidad NativeSharp (`LibA::NRecord`,
`LibB::NRecord`). La reconciliación anterior unía un tipo C vivo con uno precompilado; no unía dos precompilados.

Un consumidor que referenciaba dos bibliotecas con el mismo `NAlias` recibía «conflicting C/C++ type alias», con el
encabezado o sin él. Sonda: `claude\sonda-d25\dos-firmas`.

## Regla

Una declaración C del productor B se une con la del productor A solo si se cumplen las dos condiciones.

| Condición | Qué se compara |
|---|---|
| Identidad C | Etiqueta, ámbito (espacios de nombres y records contenedores), clase (`struct`, `union`, `enum`), destino y USR de Clang. Sin USR no se une nada. |
| Contrato de la declaración | **Record completo:** tamaño, alineación, modo `volatile` y cada campo (nombre, tipo, desplazamiento, ancho de bits, USR), y los formatos de punto flotante. **Records incompletos:** dos con la misma identidad son el mismo tipo, como en C entre unidades de traducción (regla de T08-D16). **Enum:** tipo subyacente, tamaño, alineación y cada enumerador (nombre, valor y tipo), en orden. |
| Clausura | Cada record o enum que aparece en un campo (también tras punteros, arreglos y funciones) cumple las mismas dos condiciones en los dos productores. |

Lo que no se exige ni basta:

- **No se exige el mismo encabezado.** Dos encabezados distintos con la misma declaración dan tipos compatibles en
  C. La regla no depende de que el encabezado exista al compilar el consumidor.
- **No basta el nombre ni el USR.** El USR de `struct NRecord` es `c:@S@NRecord` en cualquier encabezado. Con un
  campo de otro tipo, el contrato difiere y los tipos no se unen.
- **Un record completo y uno incompleto no se unen**, aunque C los considere compatibles, para no cambiar el layout
  de un tipo que un productor publicó como opaco.

**Tipo canónico.** Es el del primer productor en el orden de proyectos del grafo. Los demás nombran ese tipo
(`reconciledIdentityKey`), igual que un tipo vivo conciliado. La regla es por tipo: si en B solo difiere `NRecord`,
`NInner` y `NMood` siguen siendo un tipo con los de A.

## Cambios

- **`CppCRecordMetadataCodec`:**
  - `reconcilePrecompiledCTypes` decodifica los catálogos C de todos los productores. Ordena los productores por
    proyecto y empareja cada record o enum con su equivalente canónico;
  - `compareCRecordClosureEvidence` recibe `contractsOnly`, que recorre la clausura comparando contratos en lugar de
    evidencia de origen (`compareCRecordDeclarationContracts`, `compareCEnumDeclarationContracts`);
  - `reconstructPrecompiledCImportedTypes` marca los tipos restaurados (`fromPrecompiledCatalog`) y asigna
    `reconciledIdentityKey` a los no canónicos.
- **`ProjectNameLookup`:**
  - acepta la conciliación entre productores aunque ninguno referencie al otro. Un proyecto vivo llega al canónico
    por cualquier productor visible que publique la misma declaración;
  - nuevo `reconciledCppTypeKeys`.
- **`QualifiedTypeNames`:**
  - dos alias C de proyectos distintos son compatibles si sus records o enums tienen la misma identidad canónica;
  - cubre el caso vivo con precompilado (sin cambio de comportamiento) y el de dos precompilados, también para
    typedef de punteros a función.
- **`CppSemanticRecord`:**
  - los campos y enumeradores de un tipo conciliado son los del canónico, así que no se instalan dos veces;
  - un campo cuyo record está conciliado nombra el canónico (`fieldRecordType`);
  - un tipo vivo que coincide con varias declaraciones equivalentes de productores se concilia con su canónico.
- **`CppSemanticInventory`:** una global C de tipo record conciliado nombra el canónico.
- **`ProjectMemberModel`:** el tipo de un campo C conciliado es visible para el proyecto dueño de la declaración
  conciliada, también para records (antes solo para enums).
- **`ProjectCompilation`:** las firmas publicadas por un productor usan sus propias claves (`type:LibB::NMood`).
  Antes de compararlas con las del consumidor se sustituye cada clave conciliada completa por la canónica.
- **NSMeta:** no cambia. El formato, la versión y el contenido del catálogo son los de antes; la conciliación ocurre
  en el consumidor.

## Pruebas

**Nueva:** `pruebas-tanda-08-identidad-c-productores/validar-identidad-productores.py`. `LibA` y `LibB` publican
`NAlias`, `NRecord`, `NInner`, `NMood` y el opaco `NOpaque`.

El consumidor comprueba:

- que el puntero devuelto por una biblioteca es el de la otra;
- que se pasa a la otra por puntero y por valor (`NRecord`);
- `&b->inner` como `NInner*`;
- los enumeradores sin ambigüedad, el tamaño y el opaco de una biblioteca leído por la otra.

| Caso | Resultado |
|---|---|
| Estáticas, consumidor sin encabezado, O0–O3 | correcto |
| Estáticas, consumidor con encabezado y llamadas C directas | correcto |
| Dos DLL, sin y con encabezado | correcto |
| Una DLL y una estática, sin y con encabezado | correcto |
| La misma declaración en dos encabezados distintos | un tipo |
| Mismo nombre y USR, un campo `double` frente a `long long` | «conflicting C/C++ type alias 'NAlias'» |
| Lo anterior, sin nombrar el tipo | las bibliotecas coexisten |
| Lo anterior, `NInner` y `NMood` iguales | siguen siendo un tipo |
| Puntero al `NRecord` de una biblioteca pasado a la otra | «no viable overload of 'Value' accepts the argument types» |
| Solo difiere el record anidado `NInner` | «conflicting C/C++ type alias 'NInner'»; coexisten sin nombrarlo |
| `NMood` con otro valor | «ambiguous namespace state reference 'NHappy'»; coexisten |
| `NOpaque` opaco frente a completo | «conflicting C/C++ type alias 'NOpaque'»; coexisten |
| Encabezado propio del consumidor con otro `NRecord` | «conflicting C/C++ type alias 'NAlias'» |

**Expectativas actualizadas.** Las dos esperaban el rechazo que D26 resuelve. En los dos casos se ajustó el
montaje para que las bibliotecas C se puedan enlazar juntas.

- **`validar-tipos-publicos.py` (D25), caso «dos-iguales».** Dos bibliotecas con la misma declaración: ahora
  compila y se ejecuta. La segunda biblioteca C usa otras funciones (`same_*`), para no duplicar símbolos.
- **`validar-opaco-consumidor.py` (D16), caso «dos-productores».** Dos productores con la misma etiqueta opaca y un
  consumidor con su propia declaración adelantada. Antes se rechazaba como ambiguo; ahora es un tipo, y el opaco
  de un productor se pasa al otro y a las funciones C del consumidor. Lo valida C: dos tipos incompletos con la
  misma etiqueta son compatibles. Los otros tres rechazos de la prueba no cambian.

**Sin cambios y pasan:** `validar-typedef-productores.py` (D10, typedef incompatibles de dos productores, que siguen
rechazados) y las pruebas de conciliación, colisiones, incompletos, enum y plantillas
(`claude\scripts\dirigidas-d26.cmd`).

La nueva prueba entra en el gate complementario.

## Validación

Ver la sección «Validación» al final, con los resultados de los gates y de ctest.
