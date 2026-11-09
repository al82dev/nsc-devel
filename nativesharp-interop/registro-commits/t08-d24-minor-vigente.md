# T08-D24: el productor NSMETA escribe el contrato vigente

Fecha: 2026-11-09. Decisión del usuario: T08-D24, revisar la lógica del minor mínimo frente al contrato único
vigente (`docs/NSMETA-CONTRATO-ACTUAL.md`: un solo contrato, el del compilador actual; las etiquetas históricas no
seleccionan productos ni deciden capacidades).

## Problema

`LibraryMetadataConstruction.cpp` no escribía siempre el contrato vigente. El minor de cada biblioteca dependía de
su contenido:

- partía de 1.40, «legible por lectores 1.40»;
- subía a 1.41 con dependencias binarias C++;
- tomaba 1.43–1.49 según la versión del catálogo C;
- subía a 1.49 si había calificadores de dato apuntado (T08-D02).

Es un versionamiento por etiqueta, contrario al contrato único. Además fallaban pruebas que esperan el contrato
vigente (`kMetadataFormatMinor`):

- `nsc.namespace-function-metadata`;
- `nsc.stage50_9.metadata-host-compatibility`;
- la comprobación «current NSMETA preserves the vectorcall delegate flag» de `nsc.item344.vectorcall`
  (observada en T08-D22).

## Cambio

`LibraryMetadataConstruction.cpp`:

- La biblioteca empieza y termina con `kMetadataFormatMinor` (1.49).
- Se retiran:
  - la subida a 1.41 por dependencias binarias;
  - la selección de minor por versión del catálogo C;
  - la subida condicionada por calificadores y su función `hasPointeeQualifiers`, que queda sin uso.
- El catálogo C sigue la codificación que elige `collectCppMetadataCatalog`. El lector ya acepta cada versión de
  catálogo con un minor mayor o igual que el suyo (`decodeVersionedCTypeCatalog`), así que un archivo 1.49 lee
  cualquiera de ellas.

No cambian el lector ni el códec. Las pruebas que escriben modelos con minors anteriores siguen pasando por el
códec.

**Prueba actualizada** (fijaba la lógica retirada y contradecía las de arriba): `tests/unit/CppCIdentityTests.cpp`,
`checkCatalogueVersions`, que usa `identidad-c\validar.cmd`. Esperaba que cada productor real usara «the lowest minor
that represents its envelope» (1.43–1.45). Ahora espera el contrato vigente, «uses the current NSMETA contract».
Las reescrituras del catálogo en 1.42–1.45 y su lectura no cambian.

## Validación

ctest, con el entorno de las pruebas:

- pasan `nsc.namespace-function-metadata`, `nsc.stage50_9.metadata-host-compatibility` y `nsc.item344.vectorcall`
  (7 de 7), que antes fallaban;
- pasan `nsc.cpp-interop.session`, `nsc.stage48_1.c-header`, `nsc.stage48_3.c-host`, `nsc.stage48_4.cpp-host`,
  `nsc.stage48-2.external-template-type-qualification`, `nsc.stage48-2.cross-module-callable-identity`,
  `nsc.library-metadata-version-tolerance` y `nsc.library-metadata-merger`;
- `nsc.stage48_2.library-metadata` pasa 442 de 444. Las dos comprobaciones que fallan leen
  `tests/fixtures/nsmeta/historical-1.46.nsmeta`, que no está en el repositorio. Es el fallo previo registrado en
  T08-D02 y T08-D19, y no depende de este cambio.

`A:\nsc-interop-build\claude\scripts\regresion-d24.cmd`, resultado 0. Cubre productores y consumidores
precompilados:

- `identidad-c\validar.cmd` (con la prueba actualizada), con el catálogo reescrito en 1.42–1.45 y la
  restauración de USR;
- calificadores públicos (T08-D02);
- dependencias privadas (T08-D19);
- opaco del consumidor (T08-D16);
- typedef de productores (T08-D10);
- record en dos encabezados (T08-D09);
- `validar-vectorcall`.
