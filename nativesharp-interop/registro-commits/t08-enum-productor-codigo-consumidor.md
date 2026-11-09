# T08-002: enumerador C del productor frente al código propio del consumidor (validación del bloque)

Fecha: 2026-11-08. Regresión encontrada al validar el bloque T08-002, sin casilla propia.

## Reproducción

`pruebas-tanda-08-identidad-c/plantillas/validar-enum-conflicto-o0.cmd` (O0):

- El consumidor `ConsumerAnonymousConstantOther` tiene su propio encabezado `GenOther.h` con
  `enum { GenLoose = 12 };`.
- Ese consumidor lee `GenLoose`, y también lo hace la plantilla del productor `CGenProducerLoose`, cuyo
  `Gen.h` declara la misma constante con otro origen.

La prueba exige «ambiguous namespace state reference» para la lectura del consumidor, pero el consumidor se
construía.

Causa: la regla de `c9511d0ab` (interop-t08-typedef-parametro-enum) conserva solo los enumeradores del
proyecto que hace la referencia. Quería que una plantilla transportada usara la constante de su productor.
Sin embargo, el código propio del consumidor también pertenece a un proyecto, y su encabezado declara una
de las candidatas, así que la regla también se aplicaba ahí. Su propio documento dice que «una referencia
sin productor identificado conserva la ambigüedad». La muestra de `c9511d0ab` no lo detectó porque su
consumidor no tenía encabezado.

## Cambio

- `NamespaceStateSymbol.h`: nueva marca interna `cppFromPrecompiledCatalog`. No es NSMeta ni contrato
  público.
- `CppSemanticRecord.cpp`: la marca se pone en los estados reconstruidos desde el catálogo de dependencias C
  de una biblioteca precompilada (globales y enumeradores).
- `IdentifierAnalyzer.cpp`: la selección por productor solo se aplica si las candidatas del proyecto que
  hace la referencia vienen de ese catálogo. Así es en una plantilla transportada. Si el código propio del
  consumidor tiene la constante en su encabezado, la referencia sigue siendo ambigua.

## Prueba

- `plantillas/validar-enum-conflicto-o0.cmd`: aprobada.
- `typedef-funcion-record/validar-enum.py` (la muestra de `c9511d0ab`): aprobada.
- La batería del bloque T08-002 (`pruebas-tanda-08-identidad-c/validar.cmd`) está en la
  [validación del bloque](t08-002-validacion-bloque.md).
