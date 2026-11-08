# T08 H3: enumerador de enum C nombrado en plantilla

Una plantilla pública que usaba `GenModeB` no podía compilarse desde NSMETA si el enum C no aparecía en otra parte de la superficie del productor. El catálogo solo recogía enums alcanzables desde tipos publicados o campos de records.

El productor considera las demandas visibles de plantillas publicadas y agrega al cierre los enums C nombrados que contienen el enumerador solicitado. Conserva la captura y reconciliación existentes del catálogo de enums.

El validador dirigido de identidad terminó con código 0. `ConsumerEnumConstant` y `CConstNamedTemplate` compilaron y se ejecutaron. `CConstAnonTemplate` conserva el rechazo esperado; esta corrección no implementa enums anónimos ni cierra T08-002. Registro: `A:/nsc-interop-build/tanda08-identidad-enum-template-2.log`.
