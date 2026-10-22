# NSC — Tracking sobrecalculado de rendimiento de compilación

**Fecha:** 2026-10-21
**Rama y revisión auditadas:** `development` en `5b6582648661fdf78c32d267dca900edca1fffa5`
**Presupuesto técnico:** `NSC-IMPLEMENTACION-RENDIMIENTO.md` y
`NSC-ANALISIS-RENDIMIENTO-POR-FUNCION.md`
**Matriz de cobertura:** `NSC-MATRIZ-COBERTURA-RENDIMIENTO.md`
**Progreso absoluto:** `0 / 670`
**Pendientes:** `670`
**Terminados:** `0`
**Pendientes sin gate adicional:** `460`
**Pendientes que requieren diseño:** `141`
**Pendientes que requieren autorización:** `24`
**Pendientes de validación del usuario:** `45`

Esta es la vista canónica de estado granular. Cada renglón representa una sola subtarea verificable; ningún
renglón representa una tanda, un subsistema completo ni una agrupación de consumidores. El presupuesto, los
gates y las pruebas permanecen en el documento técnico.

Los 302 renglones originales permanecen íntegros. Los 368 renglones añadidos incorporan al mismo denominador
la medición, la instrumentación, las cachés persistentes, los rediseños C01/C04, los cambios estructurales que
antes estaban fuera y el cierre final. `REQUIERE DISEÑO`, `REQUIERE AUTORIZACIÓN` y `VALIDACIÓN DEL USUARIO`
son causas de pendiente, no exclusiones.

## Distribución completa por tandas

| Tanda | Identificadores | Elementos | Estado |
|---|---|---:|---|
| 0 | `PERF-0175..PERF-0194`, `PERF-0442..PERF-0443` | 22 | PENDIENTE |
| 1 | `PERF-0001..PERF-0016-*` | 20 | PENDIENTE |
| 2 | `PERF-0017..PERF-0064-*` | 84 | PENDIENTE |
| 3 | `PERF-0065..PERF-0082` | 30 | PENDIENTE |
| 4 | `PERF-0083..PERF-0086-*` | 9 | PENDIENTE |
| 5 | `PERF-0087..PERF-0109-*` | 28 | PENDIENTE |
| 6 | `PERF-0110..PERF-0159-*` | 100 | PENDIENTE |
| 7 | `PERF-0160..PERF-0174` | 31 | PENDIENTE |
| 8 | `PERF-0195..PERF-0218` | 24 | PENDIENTE |
| 9 | `PERF-0219..PERF-0248` | 30 | PENDIENTE |
| 10 | `PERF-0249..PERF-0278` | 30 | PENDIENTE |
| 11 | `PERF-0279..PERF-0303` | 25 | PENDIENTE |
| 12 | `PERF-0304..PERF-0328` | 25 | PENDIENTE |
| 13 | `PERF-0329..PERF-0363`, `PERF-0444..PERF-0450` | 42 | PENDIENTE |
| 14 | `PERF-0364..PERF-0423`, `PERF-0451..PERF-0542` | 152 | PENDIENTE |
| 15 | `PERF-0424..PERF-0441` | 18 | PENDIENTE |
| **TOTAL ABSOLUTO** |  | **670** | **0 TERMINADOS** |

Las tandas son divisiones operativas y no alteran el denominador. El orden interno respeta productores antes
que consumidores. Ninguna tanda se considera cerrada mientras alguno de sus renglones siga pendiente.

## Cobertura de los inventarios fuente

| Inventario | Tandas que lo cubren |
|---|---|
| `R-01..R-05` | 1, 2 y 3 |
| `R-06` | 5, 10, 11 y 14 |
| `R-07` | 0, 1, 4, 13 y 14 |
| `R-08` | 1, 9, 13 y 14 |
| `R-09` | 8 |
| `R-10` | 6, 10 y 13 |
| `R-11` | 5 y 14 |
| `R-12` | 14 |
| `R-13` | 6 y 14 |
| `R-14` | 6 |
| `R-15` | 7, 13 y 14 |
| Hallazgos P1/P2 suplementarios de los apéndices A y B | 14 |
| `P1..P9` del análisis de cachés | 0 y 8 a 14 |
| C01 `TemplateParseCache` | 11 |
| C04 `MirCache` | 12 |
| Validación focal, integral y rendimiento certificado | 0 y 15 |

## Pendientes — 670

| Número | Qué resuelve | Estado |
|---|---|---|
| PERF-0001 | Reconstrucción repetida del nombre completo de los tipos | PENDIENTE |
| PERF-0002 | Reconstrucción repetida de la clave de los tipos | PENDIENTE |
| PERF-0003-A | Copia innecesaria al consultar el nombre completo de un tipo | PENDIENTE |
| PERF-0003-B | Copia innecesaria al consultar la clave de un tipo | PENDIENTE |
| PERF-0004-A | Cobertura de equivalencia de identidades cacheadas | PENDIENTE |
| PERF-0004-B | Registro del target focal de identidades cacheadas | PENDIENTE |
| PERF-0005 | Duplicación del procesamiento interno de bloques SHA-256 | PENDIENTE |
| PERF-0006 | Copia y relleno completos de entradas SHA-256 en memoria | PENDIENTE |
| PERF-0007 | Lectura byte a byte durante SHA-256 de archivos | PENDIENTE |
| PERF-0008 | Equivalencia del SHA-256 optimizado en memoria | PENDIENTE |
| PERF-0009-A | Equivalencia del SHA-256 optimizado sobre archivos | PENDIENTE |
| PERF-0009-B | Registro del target focal de SHA-256 | PENDIENTE |
| PERF-0010 | Lectura fragmentada de ejecutables PE | PENDIENTE |
| PERF-0011 | Comprobación duplicada al resolver tipos LLVM | PENDIENTE |
| PERF-0012 | Cálculo innecesario de procedencia de dependencias | PENDIENTE |
| PERF-0013 | Canonicalización repetida de raíces de paquetes | PENDIENTE |
| PERF-0014 | Copia innecesaria del plan de restauración | PENDIENTE |
| PERF-0015 | Relectura de metadatos NSMETA repetidos | PENDIENTE |
| PERF-0016-A | Cobertura de deduplicación de metadatos precompilados | PENDIENTE |
| PERF-0016-B | Registro del target focal de deduplicación NSMETA | PENDIENTE |
| PERF-0017 | Implementaciones dispersas del hash transparente | PENDIENTE |
| PERF-0018-A | Hash heterogéneo duplicado en el índice de nombres de miembros | PENDIENTE |
| PERF-0018-B | Hash heterogéneo duplicado en el índice de ordinales de tipos | PENDIENTE |
| PERF-0019-A | Ausencia de consulta indexada por clave de identidad | PENDIENTE |
| PERF-0019-B | Ausencia de consulta indexada por clave con prefijo `type:` | PENDIENTE |
| PERF-0020-A | Ausencia del mapa de ordinales por identidad | PENDIENTE |
| PERF-0020-B | Ausencia de una bandera propia de disponibilidad del índice | PENDIENTE |
| PERF-0021 | Construcción repetida de búsquedas de ordinales de tipo | PENDIENTE |
| PERF-0022 | Compatibilidad de modelos construidos sin índice | PENDIENTE |
| PERF-0023 | Integración de la implementación del índice de identidades | PENDIENTE |
| PERF-0024-A | Equivalencia entre búsqueda indexada y recorrido lineal | PENDIENTE |
| PERF-0024-B | Fallback de búsqueda en modelos armados sin `build()` | PENDIENTE |
| PERF-0025 | Búsqueda lineal de tipos en herencia | PENDIENTE |
| PERF-0026 | Búsqueda lineal de tipos en interfaces | PENDIENTE |
| PERF-0027 | Búsqueda lineal de tipos en restricciones genéricas | PENDIENTE |
| PERF-0028 | Búsqueda lineal de layouts de valores | PENDIENTE |
| PERF-0029 | Búsqueda lineal de lifetime trivial de structs | PENDIENTE |
| PERF-0030 | Búsqueda lineal del ancho de bit-fields | PENDIENTE |
| PERF-0031 | Búsqueda lineal de bases de bit-fields de clase | PENDIENTE |
| PERF-0032-A | Búsqueda lineal en `isStructValueType` de HIR | PENDIENTE |
| PERF-0032-B | Búsqueda lineal en `isClassReferenceType` de HIR | PENDIENTE |
| PERF-0032-C | Búsqueda lineal en `isReferenceType` de HIR | PENDIENTE |
| PERF-0033 | Búsqueda lineal de enums durante HIR | PENDIENTE |
| PERF-0034 | Búsqueda lineal de tipos durante llamadas HIR | PENDIENTE |
| PERF-0035 | Búsqueda lineal de tipos en call frames de Core | PENDIENTE |
| PERF-0036 | Búsqueda lineal de tipos durante MIR | PENDIENTE |
| PERF-0037-A | Búsqueda lineal en `typeKeyHasDefinedDefaultStorage` | PENDIENTE |
| PERF-0037-B | Búsqueda lineal en `typeKeyIsStruct` | PENDIENTE |
| PERF-0038-A | Búsqueda lineal en `isEnumTypeKey` | PENDIENTE |
| PERF-0038-B | Búsqueda lineal en `elementTypeHasTrivialLifetime` | PENDIENTE |
| PERF-0038-C | Búsqueda lineal en `isExternCDelegateTypeKey` | PENDIENTE |
| PERF-0039-A | Búsqueda lineal en `aggregateIsUnion` | PENDIENTE |
| PERF-0039-B | Búsqueda lineal en `aggregateRequiresActiveMember` | PENDIENTE |
| PERF-0039-C | Búsqueda lineal en `aggregateAllowsSynthesizedMember` | PENDIENTE |
| PERF-0039-D | Búsqueda lineal en `aggregateIsBorrowedView` | PENDIENTE |
| PERF-0039-E | Búsqueda lineal en `aggregateElementTypeKeys` | PENDIENTE |
| PERF-0039-F | Búsqueda lineal en `aggregateElementNames` | PENDIENTE |
| PERF-0040 | Búsqueda lineal de tipos al enlazar lambdas | PENDIENTE |
| PERF-0041-A | Búsqueda lineal en `classifyCVariadicPromotion` | PENDIENTE |
| PERF-0041-B | Búsqueda lineal en `isAnyDelegateTypeKey` | PENDIENTE |
| PERF-0042 | Búsqueda lineal de tipos nombrados en miembros | PENDIENTE |
| PERF-0043 | Búsqueda lineal del tipo subyacente de enums en LLVM | PENDIENTE |
| PERF-0044-A | Búsqueda lineal en `isDelegateReferenceType` de LLVM | PENDIENTE |
| PERF-0044-B | Búsqueda lineal en `isClassReferenceType` de LLVM | PENDIENTE |
| PERF-0045 | Búsqueda lineal de tipos para información de depuración | PENDIENTE |
| PERF-0046-A | Búsqueda lineal en `isExternCDelegateType` | PENDIENTE |
| PERF-0046-B | Búsqueda lineal en `isVectorCallDelegateType` | PENDIENTE |
| PERF-0047-A | Búsqueda lineal en `isFloat32x4TypeKey` | PENDIENTE |
| PERF-0047-B | Búsqueda lineal en `compactFloatVectorLaneCount` | PENDIENTE |
| PERF-0047-C | Búsqueda lineal en `integerSimd128LaneBits` | PENDIENTE |
| PERF-0047-D | Búsqueda lineal en `floatingSimd256LaneBits` | PENDIENTE |
| PERF-0047-E | Búsqueda lineal en `integerSimd256LaneBits` | PENDIENTE |
| PERF-0048-A | Búsqueda lineal en `namedTypeForCoreKey` | PENDIENTE |
| PERF-0048-B | Búsqueda lineal en `collectReachableHostTypes` | PENDIENTE |
| PERF-0049-A | Ausencia del almacenamiento del índice de layouts por miembro | PENDIENTE |
| PERF-0049-B | Ausencia de una bandera propia de disponibilidad del índice | PENDIENTE |
| PERF-0050 | Precedencia determinista del índice de layouts | PENDIENTE |
| PERF-0051 | Recorridos lineales al consultar layouts de campos | PENDIENTE |
| PERF-0052-A | Cobertura diferencial del índice de layouts de campos | PENDIENTE |
| PERF-0052-B | Registro del target focal de layouts de campos | PENDIENTE |
| PERF-0053-A | Ausencia del mapa de layouts de structs por clave | PENDIENTE |
| PERF-0053-B | Ausencia del acceso indexado a layouts de structs | PENDIENTE |
| PERF-0053-C | Ausencia del mapa de layouts de clases por clave | PENDIENTE |
| PERF-0053-D | Ausencia del acceso indexado a layouts de clases | PENDIENTE |
| PERF-0054-A | Construcción tardía o repetida del índice de structs | PENDIENTE |
| PERF-0054-B | Construcción tardía o repetida del índice de clases | PENDIENTE |
| PERF-0055-A | Recorrido LLVM en `findStructLayout` | PENDIENTE |
| PERF-0055-B | Recorrido LLVM en `findClassLayout` | PENDIENTE |
| PERF-0055-C | Fallback lineal de layouts de structs para modelos sin índice | PENDIENTE |
| PERF-0055-D | Fallback lineal de layouts de clases para modelos sin índice | PENDIENTE |
| PERF-0056-A | Cobertura diferencial de índices de layouts MIR | PENDIENTE |
| PERF-0056-B | Registro del target focal de índices de layouts MIR | PENDIENTE |
| PERF-0057 | Resolución ABI lineal cuando existe índice de layouts | PENDIENTE |
| PERF-0058-A | Falta de propagación del índice a `configureFunctionAbi` | PENDIENTE |
| PERF-0058-B | Falta de propagación del índice a `lowerOneFunction` | PENDIENTE |
| PERF-0059 | Recorridos lineales al sumar frames awaited | PENDIENTE |
| PERF-0060-A | Ausencia de referencia compartida a layouts de structs | PENDIENTE |
| PERF-0060-B | Ausencia de selección entre layouts propias y compartidas | PENDIENTE |
| PERF-0061 | Falta de compartición de layouts entre lowerers | PENDIENTE |
| PERF-0062-A | Lectura directa del vector copiado en `FutureFieldAddress` | PENDIENTE |
| PERF-0062-B | Lectura directa del vector copiado en `lowerAwaitFuture` | PENDIENTE |
| PERF-0063 | Ausencia de caché de tipos LLVM | PENDIENTE |
| PERF-0064-A | Consulta repetida antes de reutilizar un tipo LLVM válido | PENDIENTE |
| PERF-0064-B | Falta de almacenamiento de resultados LLVM no nulos | PENDIENTE |
| PERF-0065 | Construcción completa de workers para funciones precompiladas | PENDIENTE |
| PERF-0066 | Construcción completa de workers para funciones C importadas | PENDIENTE |
| PERF-0067-A | Falta de una ruta ligera para funciones precompiladas sin cuerpo | PENDIENTE |
| PERF-0067-B | Falta de una ruta ligera para funciones C importadas | PENDIENTE |
| PERF-0068-A | Creación de workers para miembros que deben omitirse | PENDIENTE |
| PERF-0068-B | Creación de workers para funciones precompiladas | PENDIENTE |
| PERF-0068-C | Creación de workers para funciones C importadas | PENDIENTE |
| PERF-0069-A | Escritura directa de resultado HIR precompilado | PENDIENTE |
| PERF-0069-B | Escritura directa de resultado HIR importado desde C | PENDIENTE |
| PERF-0069-C | Escritura directa de resultado HIR regular | PENDIENTE |
| PERF-0070-A | Redimensionamiento del vector HIR por miembro en cada worker | PENDIENTE |
| PERF-0070-B | Redimensionamiento del vector HIR de namespace en cada worker | PENDIENTE |
| PERF-0071 | Sobrecosto del camino secuencial filtrado de HIR | PENDIENTE |
| PERF-0072 | Búsqueda repetida de locales por callable | PENDIENTE |
| PERF-0073 | Búsqueda repetida de locales por lambda | PENDIENTE |
| PERF-0074 | Búsqueda repetida de cleanups de parámetros | PENDIENTE |
| PERF-0075-A | Reconstrucción del índice de locales por callable | PENDIENTE |
| PERF-0075-B | Reconstrucción del índice de locales por lambda | PENDIENTE |
| PERF-0075-C | Reconstrucción del índice de cleanups por callable | PENDIENTE |
| PERF-0075-D | Ausencia del acceso a locales indexados por callable | PENDIENTE |
| PERF-0075-E | Ausencia del acceso a locales indexados por lambda | PENDIENTE |
| PERF-0075-F | Ausencia del acceso a cleanups indexados por callable | PENDIENTE |
| PERF-0076 | Recorridos de locales al construir funciones regulares | PENDIENTE |
| PERF-0077 | Recorridos de locales al construir funciones de namespace | PENDIENTE |
| PERF-0078 | Recorridos de locales al construir funciones lambda | PENDIENTE |
| PERF-0079 | Recorridos de cleanups en funciones regulares | PENDIENTE |
| PERF-0080 | Recorridos de cleanups en funciones de namespace | PENDIENTE |
| PERF-0081-A | Equivalencia del orden HIR entre ejecución secuencial y paralela | PENDIENTE |
| PERF-0081-B | Registro del target focal de orden HIR | PENDIENTE |
| PERF-0082 | Búsqueda repetida de funciones lambda al marcar closures | PENDIENTE |
| PERF-0083-A | Canonicalización repetida del directorio del proyecto | PENDIENTE |
| PERF-0083-B | Consulta repetida de existencia del directorio del proyecto | PENDIENTE |
| PERF-0084-A | Reconstrucción de `comparisonKey` al ordenar fuentes | PENDIENTE |
| PERF-0084-B | Reconstrucción de `generic_string` al ordenar fuentes | PENDIENTE |
| PERF-0084-C | Comparación directa repetida durante el ordenamiento de fuentes | PENDIENTE |
| PERF-0085-A | Canonicalización repetida en el primer recorrido de propiedad transitiva | PENDIENTE |
| PERF-0085-B | Canonicalización repetida en el segundo recorrido de propiedad transitiva | PENDIENTE |
| PERF-0086-A | Canonicalización repetida del proyecto al elegir artefactos exactos | PENDIENTE |
| PERF-0086-B | Canonicalización repetida dentro del ordenamiento de artefactos | PENDIENTE |
| PERF-0087 | Ausencia de índice de tipos declarados por nombre fuente | PENDIENTE |
| PERF-0088 | Recolección no indexada de tipos declarados | PENDIENTE |
| PERF-0089 | Búsqueda lineal de identidades calificadas por módulo | PENDIENTE |
| PERF-0090 | Búsqueda lineal de identidades de tipos declarados | PENDIENTE |
| PERF-0091-A | Cobertura diferencial del índice de tipos declarados | PENDIENTE |
| PERF-0091-B | Registro del target focal de tipos declarados | PENDIENTE |
| PERF-0092 | Copia anticipada del scope genérico copy-on-write | PENDIENTE |
| PERF-0093 | Materialización innecesaria de scopes genéricos de solo lectura | PENDIENTE |
| PERF-0094 | Copia innecesaria de hijos en el modelo genérico | PENDIENTE |
| PERF-0095 | Copia innecesaria de hijos al expandir aliases | PENDIENTE |
| PERF-0096 | Copia innecesaria de hijos al procesar nodos genéricos | PENDIENTE |
| PERF-0097 | Primera copia innecesaria en dependencias de templates | PENDIENTE |
| PERF-0098 | Segunda copia innecesaria en dependencias de templates | PENDIENTE |
| PERF-0099 | Copia innecesaria de hijos al recolectar templates | PENDIENTE |
| PERF-0100 | Copia innecesaria del namespace de origen | PENDIENTE |
| PERF-0101-A | Construcción de string para buscar templates por nombre terminal | PENDIENTE |
| PERF-0101-B | Reconstrucción del nombre calificado de cada template | PENDIENTE |
| PERF-0101-C | Comparación contra nombres calificados reconstruidos | PENDIENTE |
| PERF-0102 | Resolución duplicada de sintaxis de tipos demandados | PENDIENTE |
| PERF-0103 | Canonicalización repetida de rutas de visibilidad | PENDIENTE |
| PERF-0104-A | Tres tablas separadas para clasificar keywords | PENDIENTE |
| PERF-0104-B | Hasta tres consultas hash por identificador | PENDIENTE |
| PERF-0105 | Realocaciones evitables durante lexing | PENDIENTE |
| PERF-0106 | Asignaciones temporales al consultar nombres de tipos | PENDIENTE |
| PERF-0107 | Crecimiento repetido del buffer de consulta del parser | PENDIENTE |
| PERF-0108 | Reconstrucción del buffer de búsqueda de tipos | PENDIENTE |
| PERF-0109-A | Realocaciones al reunir nombres conocidos por el parser | PENDIENTE |
| PERF-0109-B | Reconstrucción repetida de nombres conocidos por compilación | PENDIENTE |
| PERF-0110-A | Búsqueda lineal en metadata decodificada y ordenada | PENDIENTE |
| PERF-0110-B | Búsqueda lineal en metadata construida en memoria | PENDIENTE |
| PERF-0111 | Recorridos de metadata al publicar miembros | PENDIENTE |
| PERF-0112 | Recorridos de metadata al publicar contratos | PENDIENTE |
| PERF-0113-A | Equivalencia de búsquedas en metadata ordenada | PENDIENTE |
| PERF-0113-B | Equivalencia del fallback sobre metadata en memoria | PENDIENTE |
| PERF-0113-C | Registro del target focal de superficie pública | PENDIENTE |
| PERF-0114 | Recalculo de prioridad al ordenar métodos publicados | PENDIENTE |
| PERF-0115-A | Llamadas repetidas al ranking dentro del comparador | PENDIENTE |
| PERF-0115-B | Ausencia de índice de dispatch por clave de tipo | PENDIENTE |
| PERF-0115-C | Ausencia de índice de firmas de interfaces | PENDIENTE |
| PERF-0115-D | Reconstrucción del orden después de clasificar los métodos | PENDIENTE |
| PERF-0116-A | Ausencia de memo de resultados de tipos de contrato | PENDIENTE |
| PERF-0116-B | Ausencia de memo de alcanzabilidad pública | PENDIENTE |
| PERF-0117-A | Rematerialización al validar declaraciones precompiladas | PENDIENTE |
| PERF-0117-B | Rematerialización al validar métodos nativos | PENDIENTE |
| PERF-0117-C | Rematerialización al validar constructores | PENDIENTE |
| PERF-0117-D | Envoltorios públicos sin reutilización de caché | PENDIENTE |
| PERF-0117-E | Caché aislada durante generación de superficie pública | PENDIENTE |
| PERF-0117-F | Caché aislada durante publicación de símbolos nativos | PENDIENTE |
| PERF-0118 | Cachés aisladas durante materialización recursiva | PENDIENTE |
| PERF-0119-A | Cobertura de equivalencia de la caché de contratos | PENDIENTE |
| PERF-0119-B | Registro del target focal de materialización de contratos | PENDIENTE |
| PERF-0120-A | Nueva visita de un template genérico ya verificado | PENDIENTE |
| PERF-0120-B | Falta de registro de templates verificados con éxito | PENDIENTE |
| PERF-0121 | Memos aislados entre raíces de verificación | PENDIENTE |
| PERF-0122-A | Búsqueda lineal de tablas de dispatch por tipo | PENDIENTE |
| PERF-0122-B | Búsqueda lineal de slots por firma | PENDIENTE |
| PERF-0123 | Búsqueda lineal de miembros virtuales | PENDIENTE |
| PERF-0124-A | Recorrido repetido en `findMetadataDispatch` | PENDIENTE |
| PERF-0124-B | Recorrido repetido en `findMetadataDispatchSlot` | PENDIENTE |
| PERF-0124-C | Falta de propagación de índices a `verifyVirtualMemberDispatch` | PENDIENTE |
| PERF-0125 | Construcción anticipada del primer diagnóstico de metadata | PENDIENTE |
| PERF-0126 | Construcción anticipada del segundo diagnóstico de metadata | PENDIENTE |
| PERF-0127 | Construcción anticipada del tercer diagnóstico de metadata | PENDIENTE |
| PERF-0128 | Construcción anticipada del cuarto diagnóstico de metadata | PENDIENTE |
| PERF-0129-A | Ausencia del almacenamiento de miembros declarados por tipo | PENDIENTE |
| PERF-0129-B | Ausencia de una bandera propia de disponibilidad del índice de miembros | PENDIENTE |
| PERF-0129-C | Ausencia del acceso indexado a miembros declarados por tipo | PENDIENTE |
| PERF-0129-D | Falta de construcción del índice al cerrar el modelo de miembros | PENDIENTE |
| PERF-0129-E | Falta de fallback para modelos de miembros armados manualmente | PENDIENTE |
| PERF-0130 | Filtrado lineal de miembros en predicados de tipos | PENDIENTE |
| PERF-0131-A | Filtrado lineal de miembros en `bindAtomicOperation` | PENDIENTE |
| PERF-0131-B | Filtrado lineal de miembros en operaciones Mutex/Monitor | PENDIENTE |
| PERF-0131-C | Filtrado lineal de miembros en `callTypeIsSend` | PENDIENTE |
| PERF-0131-D | Filtrado lineal de miembros en `callTypeIsSync` | PENDIENTE |
| PERF-0132 | Filtrado lineal de miembros durante construcción | PENDIENTE |
| PERF-0133 | Bucle interno lineal del ciclo de vida de callables | PENDIENTE |
| PERF-0134 | Filtrado lineal de miembros durante coordinación HIR | PENDIENTE |
| PERF-0135 | Filtrado lineal de miembros en call frames de Core | PENDIENTE |
| PERF-0136 | Filtrado lineal de miembros para debug info LLVM | PENDIENTE |
| PERF-0137 | Filtrado lineal de miembros en restricciones genéricas | PENDIENTE |
| PERF-0138-A | Cobertura diferencial del índice de miembros por tipo | PENDIENTE |
| PERF-0138-B | Registro del target focal de miembros por tipo | PENDIENTE |
| PERF-0139-A | Ausencia del almacenamiento de planes de interfaz por clase | PENDIENTE |
| PERF-0139-B | Falta de construcción del índice de planes de interfaz | PENDIENTE |
| PERF-0139-C | Ausencia del acceso indexado a planes de interfaz | PENDIENTE |
| PERF-0139-D | Falta de fallback lineal para modelos sin índice de planes | PENDIENTE |
| PERF-0140 | Recorrido lineal de planes de interfaz en asignaciones | PENDIENTE |
| PERF-0141-A | Ausencia de almacenamiento del ordinal de `Exception` | PENDIENTE |
| PERF-0141-B | Ausencia de almacenamiento del ordinal de `IDisposable` | PENDIENTE |
| PERF-0141-C | Ausencia de almacenamiento del ordinal de `ThreadStart` | PENDIENTE |
| PERF-0141-D | Ausencia de almacenamiento del ordinal de `StringView` | PENDIENTE |
| PERF-0141-E | Ausencia de almacenamiento del ordinal de la fachada nativa | PENDIENTE |
| PERF-0142-A | Resolución repetida del ordinal de `Exception` | PENDIENTE |
| PERF-0142-B | Resolución repetida del ordinal de `IDisposable` | PENDIENTE |
| PERF-0142-C | Resolución repetida del ordinal de `ThreadStart` | PENDIENTE |
| PERF-0142-D | Resolución repetida del ordinal de `StringView` | PENDIENTE |
| PERF-0142-E | Resolución repetida del ordinal de la fachada nativa | PENDIENTE |
| PERF-0143 | Falta de propagación de ordinales de Core a los shards | PENDIENTE |
| PERF-0144 | Búsqueda repetida de la raíz de excepciones | PENDIENTE |
| PERF-0145 | Búsqueda repetida de `IDisposable` | PENDIENTE |
| PERF-0146 | Búsqueda repetida de `ThreadStart` | PENDIENTE |
| PERF-0147 | Búsqueda repetida del tipo usado por patrones de string | PENDIENTE |
| PERF-0148 | Búsqueda repetida de la fachada nativa | PENDIENTE |
| PERF-0149-A | Ausencia del almacenamiento de tipos por nombre simple | PENDIENTE |
| PERF-0149-B | Falta de construcción del índice por nombre simple | PENDIENTE |
| PERF-0149-C | Ausencia del acceso ordenado a candidatos por nombre simple | PENDIENTE |
| PERF-0150 | Búsqueda lineal de identificadores de tipo en asignaciones | PENDIENTE |
| PERF-0151 | Cobertura de nombres simples repetidos | PENDIENTE |
| PERF-0152-A | Ausencia del almacenamiento de extensiones por nombre | PENDIENTE |
| PERF-0152-B | Falta de construcción del índice de extensiones | PENDIENTE |
| PERF-0152-C | Ausencia del acceso indexado a extensiones por nombre | PENDIENTE |
| PERF-0152-D | Falta de fallback lineal para modelos sin índice de extensiones | PENDIENTE |
| PERF-0153 | Recorrido lineal de extensiones candidatas | PENDIENTE |
| PERF-0154-A | Ausencia del almacenamiento de templates por clave canónica | PENDIENTE |
| PERF-0154-B | Falta de construcción del índice de templates | PENDIENTE |
| PERF-0154-C | Ausencia del acceso indexado a templates por clave | PENDIENTE |
| PERF-0154-D | Falta de fallback lineal para modelos sin índice de templates | PENDIENTE |
| PERF-0155 | Recorrido lineal de templates al resolver carriers | PENDIENTE |
| PERF-0156-A | Alias inútiles recorridos por cada identidad física | PENDIENTE |
| PERF-0156-B | Recanonicalización de la clave del tipo declarante por miembro | PENDIENTE |
| PERF-0156-C | Canonicalización innecesaria cuando no existen aliases efectivos | PENDIENTE |
| PERF-0157-A | Ausencia del almacenamiento del índice de funciones precompiladas | PENDIENTE |
| PERF-0157-B | Falta de construcción del índice de funciones precompiladas | PENDIENTE |
| PERF-0157-C | Ausencia de búsqueda por los cuatro campos de identidad | PENDIENTE |
| PERF-0157-D | Falta de preservación del primer ordinal entre identidades iguales | PENDIENTE |
| PERF-0158 | Recorrido lineal al enlazar resúmenes de escape | PENDIENTE |
| PERF-0159-A | Cobertura diferencial de identidades precompiladas | PENDIENTE |
| PERF-0159-B | Registro del target focal de identidades precompiladas | PENDIENTE |
| PERF-0160-A | Recalculo del checksum de un archivo en cada unidad LLVM | PENDIENTE |
| PERF-0160-B | Falta de sincronización por entrada de checksum compartido | PENDIENTE |
| PERF-0161-A | Falta de propiedad de la caché en `ProjectOopObjectGenerator` | PENDIENTE |
| PERF-0161-B | Falta de propagación de la caché a cada emisor | PENDIENTE |
| PERF-0161-C | Falta de consumo de la caché en `createDebugFile` | PENDIENTE |
| PERF-0162-A | Ausencia del índice de bindings por par de tipos | PENDIENTE |
| PERF-0162-B | Falta de construcción del índice de bindings de interfaz | PENDIENTE |
| PERF-0162-C | Falta de fallback lineal para bindings de interfaz | PENDIENTE |
| PERF-0163-A | Ausencia del índice de slots por miembro | PENDIENTE |
| PERF-0163-B | Falta de construcción del índice de slots de interfaz | PENDIENTE |
| PERF-0163-C | Falta de fallback lineal para slots de interfaz | PENDIENTE |
| PERF-0164-A | Plegado repetido de nombres de paquetes | PENDIENTE |
| PERF-0164-B | Plegado repetido de nombres de módulos | PENDIENTE |
| PERF-0164-C | Plegado repetido de nombres de módulos amigos | PENDIENTE |
| PERF-0165-A | Plegado repetido al comparar visibilidad de paquete | PENDIENTE |
| PERF-0165-B | Plegado repetido al comparar visibilidad de módulo | PENDIENTE |
| PERF-0165-C | Plegado repetido al comprobar módulos amigos | PENDIENTE |
| PERF-0165-D | Compatibilidad de las APIs públicas de visibilidad usadas por el IDE | PENDIENTE |
| PERF-0166 | Asignaciones de string al consultar especializaciones | PENDIENTE |
| PERF-0167 | Búsqueda lineal de templates cerrados por nombre | PENDIENTE |
| PERF-0168 | Búsqueda repetida de inicializadores de campos estáticos | PENDIENTE |
| PERF-0169 | Recorrido lineal al materializar constantes estáticas | PENDIENTE |
| PERF-0170 | Búsqueda lineal de módulos runtime por DLL | PENDIENTE |
| PERF-0171-A | Deduplicación lineal de símbolos por módulo | PENDIENTE |
| PERF-0171-B | Falta de reutilización del índice en el primer llamador de artefactos | PENDIENTE |
| PERF-0171-C | Falta de reutilización del índice en el segundo llamador de artefactos | PENDIENTE |
| PERF-0171-D | Falta de reutilización del índice en el tercer llamador de artefactos | PENDIENTE |
| PERF-0171-E | Falta de reutilización del índice en validación de referencias | PENDIENTE |
| PERF-0172 | Índice LLVM existente sin acceso de solo lectura | PENDIENTE |
| PERF-0173 | Búsqueda lineal de la función destructora de closures | PENDIENTE |
| PERF-0174 | Búsqueda lineal de funciones callback MIR | PENDIENTE |
| PERF-0175 | Revisión exacta de Git que servirá como origen del oráculo de rendimiento | PENDIENTE |
| PERF-0176 | Registro reproducible del ejecutable, versión y `CodegenIdentity` del oráculo | PENDIENTE |
| PERF-0177 | Registro reproducible del toolchain y configuración usados por el host O2 | PENDIENTE |
| PERF-0178 | Registro de procesador, memoria, almacenamiento y estado térmico de la máquina de medición | PENDIENTE |
| PERF-0179 | Congelación del comando exacto del escenario hola mundo con Core | PENDIENTE |
| PERF-0180 | Congelación del comando exacto del proyecto mediano | PENDIENTE |
| PERF-0181 | Congelación del comando exacto del build repetido sin cambios | PENDIENTE |
| PERF-0182 | Separación reproducible entre caché fría y caché caliente | PENDIENTE |
| PERF-0183 | Captura basal de cinco muestras del escenario hola mundo frío | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0184 | Captura basal de cinco muestras del escenario hola mundo caliente | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0185 | Captura basal de cinco muestras del proyecto mediano frío | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0186 | Captura basal de cinco muestras del proyecto mediano caliente | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0187 | Manifiesto basal de dumps HIR por escenario | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0188 | Manifiesto basal de dumps MIR por escenario | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0189 | Manifiesto basal de LLVM IR por escenario | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0190 | Manifiesto basal de hashes de objetos por unidad | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0191 | Manifiesto basal de ejecutables, bibliotecas y PDB | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0192 | Manifiesto basal de NSMETA y superficies públicas generadas | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0193 | Captura basal de diagnósticos, códigos de salida y salida estándar | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0194 | Cierre verificable del oráculo R0 con hashes, comandos y procedencia | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0195 | Ausencia de un campo `time` en las opciones de `nsc build` | PENDIENTE |
| PERF-0196 | Ausencia de un campo `timeTrace` en las opciones de `nsc build` | PENDIENTE |
| PERF-0197 | Falta de reconocimiento de `--time` por el parser de `nsc build` | PENDIENTE |
| PERF-0198 | Falta de reconocimiento de `--time-trace=<ruta>` por `nsc build` | PENDIENTE |
| PERF-0199 | Falta de validación de argumento vacío para `--time-trace` | PENDIENTE |
| PERF-0200 | Falta de reenvío de `--time` al driver anidado | PENDIENTE |
| PERF-0201 | Falta de reenvío de `--time-trace` al driver anidado | PENDIENTE |
| PERF-0202 | Ausencia de ayuda CLI para las dos opciones de medición | PENDIENTE |
| PERF-0203 | Cobertura focal del parseo y reenvío de `--time` | PENDIENTE |
| PERF-0204 | Cobertura focal del parseo y reenvío de `--time-trace` | PENDIENTE |
| PERF-0205 | Ausencia de interruptores centralizados para instrumentación LLVM | PENDIENTE |
| PERF-0206 | Apertura y truncado incondicional del archivo de `ColdLlvmTrace` | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0207 | `flush` incondicional por unidad en `ColdLlvmTrace` | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0208 | Estado de stream construido aun con la traza LLVM desactivada | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0209 | Cronómetro adquirido aunque `tracePhase` no deba registrar | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0210 | Mutex global adquirido aunque `tracePhase` no deba registrar | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0211 | Apertura y cierre de archivo por fase y módulo en `tracePhase` | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0212 | Generación incondicional de la función de traza runtime con debug completo | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0213 | Emisión de llamadas de entrada de función para la traza runtime | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0214 | Emisión de llamadas de asignación y liberación para la traza runtime | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0215 | Emisión de llamadas de stores de campo para la traza runtime | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0216 | Equivalencia de IR y artefactos cuando las trazas permanecen habilitadas | PENDIENTE |
| PERF-0217 | Medición comparativa individual de cada traza habilitada y deshabilitada | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0218 | Registro explícito de la decisión contractual sobre la traza runtime | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0219 | Inventario completo de entradas que determinan `runtime.obj` | PENDIENTE |
| PERF-0220 | Identidad estable de versión del generador de runtime | PENDIENTE |
| PERF-0221 | Inclusión del perfil de pánico en la clave de `runtime.obj` | PENDIENTE |
| PERF-0222 | Inclusión de debug, optimización y características en la clave de `runtime.obj` | PENDIENTE |
| PERF-0223 | Namespace persistente propio para entradas de `runtime.obj` | PENDIENTE |
| PERF-0224 | Metadata persistente de tamaño y digest para `runtime.obj` | PENDIENTE |
| PERF-0225 | Consulta de caché antes de construir el módulo runtime | PENDIENTE |
| PERF-0226 | Restauración atómica de un `runtime.obj` válido | PENDIENTE |
| PERF-0227 | Verificación COFF del `runtime.obj` restaurado | PENDIENTE |
| PERF-0228 | Construcción del runtime únicamente después de un miss válido | PENDIENTE |
| PERF-0229 | Optimización LLVM del runtime únicamente después de un miss | PENDIENTE |
| PERF-0230 | Almacenamiento atómico del `runtime.obj` recién producido | PENDIENTE |
| PERF-0231 | Cuarentena y reconstrucción de entradas corruptas de runtime | PENDIENTE |
| PERF-0232 | Exclusión mutua de escritores concurrentes de `runtime.obj` | PENDIENTE |
| PERF-0233 | Cobertura focal de hit, miss, corrupción e invalidación de runtime | PENDIENTE |
| PERF-0234 | Inventario canónico por DLL de símbolos requeridos para import libraries | PENDIENTE |
| PERF-0235 | Orden canónico de símbolos usado en la clave de cada import library | PENDIENTE |
| PERF-0236 | Inclusión de arquitectura e identidad de NSLink en la clave de import libraries | PENDIENTE |
| PERF-0237 | Namespace persistente propio para import libraries | PENDIENTE |
| PERF-0238 | Metadata persistente de tamaño y digest para cada import library | PENDIENTE |
| PERF-0239 | Consulta de caché antes de lanzar NSLink por DLL | PENDIENTE |
| PERF-0240 | Restauración atómica de una import library válida | PENDIENTE |
| PERF-0241 | Verificación COFF de la import library restaurada | PENDIENTE |
| PERF-0242 | Lanzamiento de NSLink únicamente después de un miss válido | PENDIENTE |
| PERF-0243 | Almacenamiento atómico de la import library recién producida | PENDIENTE |
| PERF-0244 | Cuarentena y reconstrucción de import libraries corruptas | PENDIENTE |
| PERF-0245 | Exclusión mutua de escritores concurrentes por identidad de DLL | PENDIENTE |
| PERF-0246 | Preservación textual de diagnósticos cuando falla NSLink | PENDIENTE |
| PERF-0247 | Cobertura focal fría, caliente, corrupta e invalidada de import libraries | PENDIENTE |
| PERF-0248 | Demostración de cero procesos NSLink en un build caliente sin cambios | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0249 | Inventario de todas las entradas que determinan `public-api` precompilada | PENDIENTE — REQUIERE DISEÑO |
| PERF-0250 | Inventario de todas las entradas que determinan `public-templates` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0251 | Identidad estable de versión del emisor de superficie pública | PENDIENTE — REQUIERE DISEÑO |
| PERF-0252 | Inclusión del digest del payload NSMETA en la clave de superficie | PENDIENTE — REQUIERE DISEÑO |
| PERF-0253 | Inclusión de `CodegenIdentity` en la clave de superficie | PENDIENTE — REQUIERE DISEÑO |
| PERF-0254 | Inclusión de opciones semánticas relevantes en la clave de superficie | PENDIENTE — REQUIERE DISEÑO |
| PERF-0255 | Tipo propietario de la caché de superficies en archivos propios | PENDIENTE |
| PERF-0256 | Namespace persistente independiente para superficies precompiladas | PENDIENTE |
| PERF-0257 | Cabecera versionada del payload de superficie pública | PENDIENTE |
| PERF-0258 | Escritura atómica del texto `public-api` | PENDIENTE |
| PERF-0259 | Escritura atómica del texto `public-templates` | PENDIENTE |
| PERF-0260 | Metadata de tamaño y digest para ambos textos | PENDIENTE |
| PERF-0261 | Bloqueo de escritores concurrentes por módulo e identidad | PENDIENTE |
| PERF-0262 | Cuarentena de superficies truncadas o corruptas | PENDIENTE |
| PERF-0263 | Consulta de caché antes de ejecutar `PublicSurfaceBuilder` | PENDIENTE |
| PERF-0264 | Omisión completa de `PublicSurfaceBuilder` durante un hit válido | PENDIENTE |
| PERF-0265 | Restauración del nombre virtual original de `public-api` | PENDIENTE |
| PERF-0266 | Restauración del nombre virtual original de `public-templates` | PENDIENTE |
| PERF-0267 | Preservación del orden y ordinales de las unidades virtuales restauradas | PENDIENTE |
| PERF-0268 | Fallback frío ante miss, versión distinta o entrada corrupta | PENDIENTE |
| PERF-0269 | Invalidación por cambio del NSMETA de entrada | PENDIENTE |
| PERF-0270 | Invalidación por cambio del compilador o del emisor | PENDIENTE |
| PERF-0271 | Integración de superficies con la política común de poda | PENDIENTE |
| PERF-0272 | Contadores separados de hit, miss, corrupción e invalidación | PENDIENTE |
| PERF-0273 | Cobertura de determinismo al emitir dos veces la misma metadata | PENDIENTE |
| PERF-0274 | Cobertura focal de caché fría y caliente por módulo | PENDIENTE |
| PERF-0275 | Cobertura focal de corrupción e invalidación de superficies | PENDIENTE |
| PERF-0276 | Igualdad byte a byte de `public-api` y `public-templates` restauradas | PENDIENTE |
| PERF-0277 | Igualdad de dumps y artefactos entre superficie fría y caliente | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0278 | Medición del frontend evitado por módulo precompilado | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0279 | Inventario de productores y consumidores actuales de `TemplateParseCache` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0280 | Llave calculable antes de lexear o parsear una plantilla | PENDIENTE — REQUIERE DISEÑO |
| PERF-0281 | Inclusión del digest del texto fuente en la llave de plantillas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0282 | Inclusión de versión de lenguaje, parser y compilador en la llave | PENDIENTE — REQUIERE DISEÑO |
| PERF-0283 | Inclusión de opciones e imports que alteran el parseo en la llave | PENDIENTE — REQUIERE DISEÑO |
| PERF-0284 | Separación de la raíz persistente respecto de la caché MIR | PENDIENTE |
| PERF-0285 | Extensión y cabecera exclusivas para payloads de plantillas | PENDIENTE |
| PERF-0286 | Serialización completa del resultado de parseo reutilizable | PENDIENTE — REQUIERE DISEÑO |
| PERF-0287 | Preservación de rangos, archivos y posiciones de cada nodo restaurado | PENDIENTE — REQUIERE DISEÑO |
| PERF-0288 | Restauración del AST dentro del ownership de la arena activa | PENDIENTE — REQUIERE DISEÑO |
| PERF-0289 | Reasignación determinista de ordinales de archivo y unidad restaurados | PENDIENTE — REQUIERE DISEÑO |
| PERF-0290 | Consulta de caché antes del lexer de plantillas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0291 | Omisión del parser durante un hit válido de plantillas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0292 | Política que impide almacenar resultados con errores | PENDIENTE |
| PERF-0293 | Fallback al lexer y parser ante miss o incompatibilidad | PENDIENTE |
| PERF-0294 | Cuarentena de payloads de plantillas corruptos | PENDIENTE |
| PERF-0295 | Escritura atómica sin `WRITE_THROUGH` por cada archivo | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0296 | Exclusión mutua de escritores concurrentes de la misma plantilla | PENDIENTE |
| PERF-0297 | Invalidación por cambio de fuente, opciones o identidad del parser | PENDIENTE |
| PERF-0298 | Integración de la caché de plantillas con la poda común | PENDIENTE |
| PERF-0299 | Contadores separados de parseos ejecutados y evitados | PENDIENTE |
| PERF-0300 | Cobertura focal de round trip del AST de plantillas | PENDIENTE |
| PERF-0301 | Cobertura de diagnósticos idénticos en miss y entrada inválida | PENDIENTE |
| PERF-0302 | Igualdad de dumps y artefactos con caché de plantillas fría y caliente | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0303 | Medición del lexing y parseo evitados por la caché de plantillas | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0304 | Inventario de productores y consumidores actuales de `MirCache` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0305 | Llave MIR calculable antes de construir las funciones MIR | PENDIENTE — REQUIERE DISEÑO |
| PERF-0306 | Huella determinista del HIR de entrada para la llave MIR | PENDIENTE — REQUIERE DISEÑO |
| PERF-0307 | Inclusión de layouts, bindings y estáticos relevantes en la llave MIR | PENDIENTE — REQUIERE DISEÑO |
| PERF-0308 | Inclusión de opciones semánticas, debug y target en la llave MIR | PENDIENTE — REQUIERE DISEÑO |
| PERF-0309 | Inclusión de versión de formato y compilador en la llave MIR | PENDIENTE — REQUIERE DISEÑO |
| PERF-0310 | Separación de la raíz persistente respecto de plantillas | PENDIENTE |
| PERF-0311 | Extensión y cabecera exclusivas para payloads MIR | PENDIENTE |
| PERF-0312 | Consulta de caché antes de `buildMirFunctions` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0313 | Uso efectivo del resultado restaurado en vez de descartarlo | PENDIENTE — REQUIERE DISEÑO |
| PERF-0314 | Restauración ordenada de `functions_` y sus ordinales | PENDIENTE — REQUIERE DISEÑO |
| PERF-0315 | Reconstrucción de mapas auxiliares desde funciones MIR restauradas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0316 | Integración de resultados restaurados con layouts y dispatch actuales | PENDIENTE — REQUIERE DISEÑO |
| PERF-0317 | Composición determinista de hits y misses dentro de un proyecto | PENDIENTE — REQUIERE DISEÑO |
| PERF-0318 | Preservación del orden global cuando sólo algunas funciones se restauran | PENDIENTE — REQUIERE DISEÑO |
| PERF-0319 | Política que impide almacenar MIR después de un error | PENDIENTE |
| PERF-0320 | Fallback a lowering MIR ante miss o incompatibilidad | PENDIENTE |
| PERF-0321 | Cuarentena de payloads MIR corruptos | PENDIENTE |
| PERF-0322 | Escritura atómica y bloqueo por identidad MIR | PENDIENTE |
| PERF-0323 | Invalidación por cambios de HIR, modelos, opciones o compilador | PENDIENTE |
| PERF-0324 | Integración de la caché MIR con la poda común | PENDIENTE |
| PERF-0325 | Contadores separados de funciones MIR construidas y restauradas | PENDIENTE |
| PERF-0326 | Cobertura focal de round trip de todos los kinds MIR soportados | PENDIENTE |
| PERF-0327 | Igualdad de dumps, objetos y ejecutables con MIR frío y caliente | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0328 | Medición del lowering MIR evitado por la caché rediseñada | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0329 | Registro de tamaño y mtime del artefacto al copiar durante restore | PENDIENTE — REQUIERE DISEÑO |
| PERF-0330 | Prefiltro conservador por tamaño antes de rehash del destino | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0331 | Prefiltro conservador por mtime antes de rehash del destino | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0332 | Preservación de la detección de corrupción cuando el prefiltro no coincide | PENDIENTE |
| PERF-0333 | Lectura del lock existente antes de intentar reescribirlo | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0334 | Comparación byte a byte del lock existente con el nuevo | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0335 | Preservación del mtime del lock cuando su contenido no cambia | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0336 | Cobertura focal de restore intacto, modificado y corrupto | PENDIENTE |
| PERF-0337 | Consulta de `loadedByModule` antes de releer un NSMETA ya cargado | PENDIENTE |
| PERF-0338 | Preservación del diagnóstico de dos rutas distintas para un módulo | PENDIENTE |
| PERF-0339 | Identidad canónica de NSMETA por ruta, tamaño, mtime y digest | PENDIENTE — REQUIERE DISEÑO |
| PERF-0340 | Almacén por proceso de metadata NSMETA verificada | PENDIENTE — REQUIERE DISEÑO |
| PERF-0341 | Propagación de metadata verificada desde configuración hasta generación | PENDIENTE — REQUIERE DISEÑO |
| PERF-0342 | Eliminación de la segunda decodificación en `validateLibraryReference` | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0343 | Eliminación de la segunda autenticación de metadata inmutable | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0344 | Reutilización de `publishedNativeSymbols` junto con la metadata | PENDIENTE — REQUIERE DISEÑO |
| PERF-0345 | Detección de reemplazo de NSMETA entre configuración y enlace | PENDIENTE — REQUIERE DISEÑO |
| PERF-0346 | Ownership acotado de la caché NSMETA sin estado estático residual | PENDIENTE |
| PERF-0347 | Cobertura focal de NSMETA repetido, reemplazado y conflictivo | PENDIENTE |
| PERF-0348 | Identidad canónica de una DLL PE por ruta, tamaño, mtime y digest | PENDIENTE — REQUIERE DISEÑO |
| PERF-0349 | Memo por proceso del resultado de verificación PE | PENDIENTE — REQUIERE DISEÑO |
| PERF-0350 | Reutilización del resultado PE en validación inicial de runtime | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0351 | Reutilización del resultado PE en verificación posterior al enlace | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0352 | Detección de reemplazo de DLL entre ambas verificaciones | PENDIENTE — REQUIERE DISEÑO |
| PERF-0353 | Preservación textual de todos los diagnósticos PE | PENDIENTE |
| PERF-0354 | Cobertura focal de DLL válida, inválida, truncada y reemplazada | PENDIENTE |
| PERF-0355 | Formato versionado del sello `last-prune` de la caché | PENDIENTE — REQUIERE DISEÑO |
| PERF-0356 | Lectura tolerante a ausencia o corrupción del sello de poda | PENDIENTE |
| PERF-0357 | Política explícita del intervalo mínimo entre podas | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0358 | Exclusión mutua de procesos que intentan podar la misma raíz | PENDIENTE |
| PERF-0359 | Actualización atómica del sello después de una poda completa | PENDIENTE |
| PERF-0360 | Poda inmediata conservada cuando se excede el límite duro | PENDIENTE |
| PERF-0361 | Recuperación automática si una poda queda interrumpida | PENDIENTE |
| PERF-0362 | Cobertura focal de sello reciente, vencido, corrupto y concurrente | PENDIENTE |
| PERF-0363 | Medición conjunta de hashing, verificación y poda evitados | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0364 | Inventario de todos los mutadores en sitio de `TypeSyntax` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0365 | Demostración de qué nodos hoja permanecen inmutables tras la instantánea | PENDIENTE — REQUIERE DISEÑO |
| PERF-0366 | Contrato de ownership y lifetime para compartir nodos hoja | PENDIENTE — REQUIERE DISEÑO |
| PERF-0367 | Omisión de `cloneAliasType` únicamente para hojas demostradas inmutables | PENDIENTE — REQUIERE DISEÑO |
| PERF-0368 | Cobertura de alias, const genéricos, orígenes y mutaciones tardías | PENDIENTE |
| PERF-0369 | Inventario de lectura y escritura de las ocho pasadas genéricas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0370 | Grafo de dependencias de datos entre las ocho pasadas genéricas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0371 | Demostración del orden mínimo equivalente entre pasadas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0372 | Banderas por unidad para resultados genéricos ya calculados | PENDIENTE — REQUIERE DISEÑO |
| PERF-0373 | Reutilización de resultados genéricos precompilados junto con NSMETA | PENDIENTE — REQUIERE DISEÑO |
| PERF-0374 | Fusión de pasadas que compartan recorrido sin dependencia cruzada | PENDIENTE — REQUIERE DISEÑO |
| PERF-0375 | Preservación del orden exacto de diagnósticos de genéricos | PENDIENTE |
| PERF-0376 | Cobertura diferencial de las ocho pasadas antes y después del rediseño | PENDIENTE |
| PERF-0377 | Inventario de dependencias en los ordinales de las unidades públicas 0 y 1 | PENDIENTE — REQUIERE DISEÑO |
| PERF-0378 | Regla determinista para particionar superficie pública por namespace | PENDIENTE — REQUIERE DISEÑO |
| PERF-0379 | Nombres virtuales estables para unidades públicas particionadas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0380 | Orden estable de unidades y declaraciones después de particionar | PENDIENTE — REQUIERE DISEÑO |
| PERF-0381 | Preservación de posiciones y diagnósticos entre unidades particionadas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0382 | Cobertura byte a byte del texto agregado antes y después de particionar | PENDIENTE |
| PERF-0383 | Medición del paralelismo real de lex y parse por unidad pública | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0384 | Medición certificada del tamaño y población real de cada clase AST | PENDIENTE — REQUIERE DISEÑO |
| PERF-0385 | Demostración del orden actual de destrucción de nodos AST | PENDIENTE — REQUIERE DISEÑO |
| PERF-0386 | Diseño de bloques de arena con tamaño y alineación correctos | PENDIENTE — REQUIERE DISEÑO |
| PERF-0387 | Construcción por placement new conservando direcciones estables | PENDIENTE — REQUIERE DISEÑO |
| PERF-0388 | Registro de destructores y destrucción inversa equivalente | PENDIENTE — REQUIERE DISEÑO |
| PERF-0389 | Recuperación correcta cuando un constructor AST lanza o falla | PENDIENTE — REQUIERE DISEÑO |
| PERF-0390 | Inventario de campos raros candidatos a `AstNodeExtras` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0391 | Almacenamiento perezoso de extras sin cambiar accesores públicos | PENDIENTE — REQUIERE DISEÑO |
| PERF-0392 | Preservación de semántica de copia y movimiento de nodos AST | PENDIENTE — REQUIERE DISEÑO |
| PERF-0393 | Compatibilidad con pruebas que inspeccionan miembros privados del AST | PENDIENTE |
| PERF-0394 | Cobertura de lifetime, direcciones y destrucción de la arena por bloques | PENDIENTE |
| PERF-0395 | Medición de bytes y asignaciones por nodo antes y después | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0396 | Ownership explícito del `ProjectGraph` compartido con el driver anidado | PENDIENTE — REQUIERE DISEÑO |
| PERF-0397 | Interfaz interna para entregar el grafo ya cargado al driver anidado | PENDIENTE — REQUIERE DISEÑO |
| PERF-0398 | Eliminación de la recarga de la raíz dentro del driver anidado | PENDIENTE — REQUIERE DISEÑO |
| PERF-0399 | Preservación del orden, diagnósticos y reemplazos del grafo recargado | PENDIENTE |
| PERF-0400 | Cobertura diferencial de carga única frente a carga repetida del grafo | PENDIENTE |
| PERF-0401 | Medición aislada del costo de destruir `ProjectCompilation` | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0402 | Ownership explícito de `ProjectCompilation` en la ruta CLI de vida corta | PENDIENTE — REQUIERE DISEÑO |
| PERF-0403 | Conservación de destrucción normal en bibliotecas, IDE y pruebas | PENDIENTE — REQUIERE DISEÑO |
| PERF-0404 | Decisión contractual sobre liberar o abandonar memoria al salir del proceso | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0405 | Inventario de mapas de shards de cuerpos construidos sin ser consultados | PENDIENTE — REQUIERE DISEÑO |
| PERF-0406 | Inicialización perezosa independiente de cada mapa de shard | PENDIENTE — REQUIERE DISEÑO |
| PERF-0407 | Invalidación y reconstrucción correcta de mapas perezosos reutilizados | PENDIENTE — REQUIERE DISEÑO |
| PERF-0408 | Compatibilidad de mapas perezosos con pruebas que acceden miembros privados | PENDIENTE |
| PERF-0409 | Inventario de todo el estado mutable de un lowerer MIR por función | PENDIENTE — REQUIERE DISEÑO |
| PERF-0410 | Rutina completa de reset que no conserve estado entre funciones | PENDIENTE — REQUIERE DISEÑO |
| PERF-0411 | Ownership de un lowerer MIR reutilizable por hilo de trabajo | PENDIENTE — REQUIERE DISEÑO |
| PERF-0412 | Cobertura de equivalencia y contaminación entre funciones consecutivas | PENDIENTE |
| PERF-0413 | Inventario del estado capturado por la `std::function` de `buildVisibility` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0414 | Sustitución iterativa de la recursión de visibilidad con el mismo orden | PENDIENTE — REQUIERE DISEÑO |
| PERF-0415 | Cobertura de ciclos, profundidad y diagnósticos de visibilidad iterativa | PENDIENTE |
| PERF-0416 | Hash transparente de `knownTypeNames_` sin alterar su conjunto ni orden observable | PENDIENTE — REQUIERE DISEÑO |
| PERF-0417 | Cobertura comparativa del hash transparente frente al scratch reutilizado | PENDIENTE |
| PERF-0418 | Estado explícito para sustituir la recursión de `callTypeIsSend` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0419 | Estado explícito para sustituir la recursión de `callTypeIsSync` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0420 | Cobertura de ciclos y resultados idénticos para `Send` y `Sync` iterativos | PENDIENTE |
| PERF-0421 | Inventario de strings y vectores copiados de HIR a MIR por nodo | PENDIENTE — REQUIERE DISEÑO |
| PERF-0422 | Identidad internada de `typeKey` compartible entre HIR y MIR | PENDIENTE — REQUIERE DISEÑO |
| PERF-0423 | Cobertura de ownership y equivalencia para datos HIR/MIR compartidos | PENDIENTE |
| PERF-0424 | Ejecución focal de cada target nuevo de las tandas 1 a 7 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0425 | Ejecución focal de cada target nuevo de las tandas 8 a 14 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0426 | Confirmación de 3,149 de 3,149 unitarias con el producto final | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0427 | Confirmación de 6,026 de 6,026 pruebas externas con el producto final | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0428 | Comparación final byte a byte de dumps HIR contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0429 | Comparación final byte a byte de dumps MIR contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0430 | Comparación final byte a byte de LLVM IR contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0431 | Comparación final de NSMETA contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0432 | Comparación final de objetos por unidad contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0433 | Comparación final de ejecutables y bibliotecas contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0434 | Comparación final de PDB y rutas de depuración contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0435 | Comparación final de diagnósticos y códigos de salida contra R0 | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0436 | Cinco mediciones finales de hola mundo con caché fría | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0437 | Cinco mediciones finales de hola mundo con caché caliente | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0438 | Cinco mediciones finales del proyecto mediano con caché fría | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0439 | Cinco mediciones finales del proyecto mediano con caché caliente | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0440 | Informe de medianas, dispersión, hits, misses y procesos evitados por tanda | PENDIENTE |
| PERF-0441 | Gate aritmético final de 670 terminados y cero pendientes | PENDIENTE |
| PERF-0442 | Directorio O2 independiente en `A:` para el host de medición | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0443 | Confirmación binaria de que el host medido usa O2 y no conserva `/Od` | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0444 | Inventario de dependencias y orden entre verificaciones NSMETA | PENDIENTE — REQUIERE DISEÑO |
| PERF-0445 | Lectura y autenticación paralela de NSMETA mediante el pool existente | PENDIENTE — REQUIERE DISEÑO |
| PERF-0446 | Resultados NSMETA aislados por ordinal durante el trabajo paralelo | PENDIENTE — REQUIERE DISEÑO |
| PERF-0447 | Publicación secuencial de resultados NSMETA en el orden vigente | PENDIENTE — REQUIERE DISEÑO |
| PERF-0448 | Preservación del primer diagnóstico NSMETA según el orden actual | PENDIENTE |
| PERF-0449 | Cancelación determinista de workers NSMETA después del primer fallo publicable | PENDIENTE — REQUIERE DISEÑO |
| PERF-0450 | Cobertura diferencial de verificación NSMETA secuencial y paralela | PENDIENTE |
| PERF-0451 | Inventario de rutas absolutas incorporadas a debug info y a claves de objetos | PENDIENTE — REQUIERE DISEÑO |
| PERF-0452 | Reenvío de `-fdebug-prefix-map` desde `nsc build` al driver anidado | PENDIENTE |
| PERF-0453 | Regla estable para relativizar raíces temporales sin tocar fuentes reales | PENDIENTE — REQUIERE DISEÑO |
| PERF-0454 | Inclusión determinista del prefix map en la identidad de compilación | PENDIENTE — REQUIERE DISEÑO |
| PERF-0455 | Opción del compilador para una raíz de caché de artefactos estable y externa al proyecto | PENDIENTE — REQUIERE DISEÑO |
| PERF-0456 | Preservación explícita del contrato de rutas del PDB | PENDIENTE — REQUIERE AUTORIZACIÓN |
| PERF-0457 | Cobertura de PDB y diagnósticos con y sin prefix map | PENDIENTE |
| PERF-0458 | Demostración de hit de objetos entre dos raíces temporales equivalentes | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0459 | Verificación de disponibilidad del escritor COFF de import libraries en LLVM vigente | PENDIENTE — REQUIERE DISEÑO |
| PERF-0460 | Modelo de entrada canónico para una import library escrita en proceso | PENDIENTE — REQUIERE DISEÑO |
| PERF-0461 | Escritura en proceso del formato short import COFF | PENDIENTE — REQUIERE DISEÑO |
| PERF-0462 | Orden determinista de miembros y tabla de símbolos de la import library | PENDIENTE — REQUIERE DISEÑO |
| PERF-0463 | Diagnósticos equivalentes del escritor en proceso | PENDIENTE |
| PERF-0464 | Comparación de símbolos y bytes frente a la salida canónica de NSLink | PENDIENTE |
| PERF-0465 | Fallback a NSLink cuando el escritor en proceso no soporta una entrada | PENDIENTE — REQUIERE DISEÑO |
| PERF-0466 | Reserva superior de `loaded` antes de expandir `binaryDependencies` | PENDIENTE |
| PERF-0467 | Sustitución de referencias inestables por ordinales al expandir dependencias binarias | PENDIENTE — REQUIERE DISEÑO |
| PERF-0468 | Eliminación de la copia completa de texto IR al calcular la clave incremental del runtime | PENDIENTE — REQUIERE DISEÑO |
| PERF-0469 | Cobertura de identidad incremental del runtime sin materializar su texto completo | PENDIENTE |
| PERF-0470 | Precomputación de identidades canónicas de módulos en `ProjectGraph` | PENDIENTE |
| PERF-0471 | Índice de descriptores precompilados únicos sin `find_if` por elemento | PENDIENTE |
| PERF-0472 | Cobertura de caché global estable sin modificar NSC Builder ni el IDE | PENDIENTE |
| PERF-0473 | Inventario de la condición que desactiva la caché pre-LLVM con debug completo | PENDIENTE — REQUIERE DISEÑO |
| PERF-0474 | Inventario de datos de debug que deben formar parte de la llave pre-LLVM | PENDIENTE — REQUIERE DISEÑO |
| PERF-0475 | Identidad estable de rutas y checksums para objetos con debug completo | PENDIENTE — REQUIERE DISEÑO |
| PERF-0476 | Restauración de objetos pre-LLVM con debug completo sin perder información | PENDIENTE — REQUIERE DISEÑO |
| PERF-0477 | Invalidación de objetos debug por fuente, prefix map, target y opciones | PENDIENTE — REQUIERE DISEÑO |
| PERF-0478 | Cobertura focal fría y caliente de la caché pre-LLVM con `debug=full` | PENDIENTE |
| PERF-0479 | Medición de hits pre-LLVM con `debug=full` | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0480 | Inventario de entradas que determinan la llave de enlace incremental | PENDIENTE — REQUIERE DISEÑO |
| PERF-0481 | Inventario de artefactos de enlace y PDB reutilizables entre builds | PENDIENTE — REQUIERE DISEÑO |
| PERF-0482 | Eliminación de materializaciones de texto usadas sólo para la llave incremental | PENDIENTE — REQUIERE DISEÑO |
| PERF-0483 | Validación del ejecutable restaurado antes de omitir el enlace final | PENDIENTE — REQUIERE DISEÑO |
| PERF-0484 | Validación y restauración coherente del PDB asociado | PENDIENTE — REQUIERE DISEÑO |
| PERF-0485 | Invalidación por cambio de objeto, import library, opción o linker | PENDIENTE — REQUIERE DISEÑO |
| PERF-0486 | Cobertura focal de enlace incremental frío, caliente y corrupto | PENDIENTE |
| PERF-0487 | Medición de procesos de enlace final y generación PDB evitados | PENDIENTE — VALIDACIÓN DEL USUARIO |
| PERF-0488 | Inventario de claves y lifetime del `ProjectLoaderCache` actual | PENDIENTE — REQUIERE DISEÑO |
| PERF-0489 | Demostración de qué cargas se repiten dentro de un mismo proceso | PENDIENTE — REQUIERE DISEÑO |
| PERF-0490 | Extensión acotada del lifetime del loader sin conservar archivos obsoletos | PENDIENTE — REQUIERE DISEÑO |
| PERF-0491 | Invalidación del loader por ruta, tamaño, mtime y contenido | PENDIENTE — REQUIERE DISEÑO |
| PERF-0492 | Cobertura de hit, modificación y desaparición para `ProjectLoaderCache` | PENDIENTE |
| PERF-0493 | Inventario de consultas y tasa de hit del `ComptimeCache` en una compilación | PENDIENTE — REQUIERE DISEÑO |
| PERF-0494 | Demostración de reutilización posible entre unidades o compilaciones | PENDIENTE — REQUIERE DISEÑO |
| PERF-0495 | Identidad e invalidación completas para resultados comptime persistentes | PENDIENTE — REQUIERE DISEÑO |
| PERF-0496 | Cobertura diferencial de resultados comptime reutilizados | PENDIENTE |
| PERF-0497 | Cierre del inventario de cachés con productor, consumidor, llave e invalidador para cada una | PENDIENTE |
| PERF-0498 | Consumo del índice de templates por nombre en `resolveNameOfTemplateGenericMethod` | PENDIENTE |
| PERF-0499 | Cobertura de orden, duplicados y fallback de `resolveNameOfTemplateGenericMethod` | PENDIENTE |
| PERF-0500 | Medición de llamadas y locales inspeccionados por las consultas de ownership | PENDIENTE — REQUIERE DISEÑO |
| PERF-0501 | Índice por lugar y alcance para referencias mutables activas del callable | PENDIENTE — REQUIERE DISEÑO |
| PERF-0502 | Sustitución del barrido completo de `locals_` en `conflictingMutableReference` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0503 | Eliminación del string temporal `"L" + to_string(localOrdinal)` en ownership | PENDIENTE |
| PERF-0504 | Índice de préstamos locales activos por ordinal del propietario | PENDIENTE — REQUIERE DISEÑO |
| PERF-0505 | Índice de reasignaciones de parámetros prestados por ordinal del propietario | PENDIENTE — REQUIERE DISEÑO |
| PERF-0506 | Índice de vistas vivas por raíz local sin recorrer todos los locales | PENDIENTE — REQUIERE DISEÑO |
| PERF-0507 | Cobertura diferencial de ownership con scopes, reborrows, invalidación y vistas | PENDIENTE |
| PERF-0508 | Medición de expresiones y búsquedas ejecutadas por `resolveCapabilityCalls` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0509 | Eliminación de la copia por iteración de los descriptores de capability | PENDIENTE |
| PERF-0510 | Resolución en una sola pasada de llamadas capability si la medición la justifica | PENDIENTE — REQUIERE DISEÑO |
| PERF-0511 | Cobertura diferencial de sustituciones, dependencias y diagnósticos comptime | PENDIENTE |
| PERF-0512 | Índice por clave compuesta para propietarios de implementaciones de traits | PENDIENTE — REQUIERE DISEÑO |
| PERF-0513 | Preservación del primer template coincidente en el índice de traits | PENDIENTE — REQUIERE DISEÑO |
| PERF-0514 | Sustitución del recorrido lineal en `findTraitImplementationOwner` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0515 | Cobertura diferencial de propietarios de traits con claves repetidas | PENDIENTE |
| PERF-0516 | Índice MIR compartido `mangledName -> primer ordinal` durante el lowerer | PENDIENTE — REQUIERE DISEÑO |
| PERF-0517 | Sustitución del bucle interno de `inheritMoveNextDropFrames` | PENDIENTE |
| PERF-0518 | Sustitución del bucle interno de `inheritConstructorFrames` | PENDIENTE |
| PERF-0519 | Preservación determinista del primer nombre MIR coincidente y fallback | PENDIENTE |
| PERF-0520 | Cobertura diferencial de frames async, generators y funciones ausentes | PENDIENTE |
| PERF-0521 | Índice por proyecto, fuente y offset para targets de atributos comptime | PENDIENTE — REQUIERE DISEÑO |
| PERF-0522 | Sustitución de los recorridos de tipos y miembros en `findAttributeTarget` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0523 | Índice por identidad para `ProjectMetaprogrammingModel::findType` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0524 | Vista pública precalculada de miembros para `visibleMembers` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0525 | Cobertura de orden y visibilidad de consultas de metaprogramación | PENDIENTE |
| PERF-0526 | Verificación estática y medida de que `finalizeExternalMetadataTypes` es una sola pasada | PENDIENTE — REQUIERE DISEÑO |
| PERF-0527 | Criterio explícito para no reescribir `finalizeExternalMetadataTypes` si no existe producto cartesiano | PENDIENTE — REQUIERE DISEÑO |
| PERF-0528 | Índice de constructores generator por `memberOrdinal` en MIR | PENDIENTE — REQUIERE DISEÑO |
| PERF-0529 | Sustitución del recorrido de funciones en `resolveMemberPhysicalIdentity` | PENDIENTE |
| PERF-0530 | Cobertura de identidad física normal, async y generator | PENDIENTE |
| PERF-0531 | Decoración o comparación sin strings temporales en `canonicalize` de NSMETA | PENDIENTE — REQUIERE DISEÑO |
| PERF-0532 | Preservación exacta de precedencia, conflicto y texto diagnóstico de `canonicalize` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0533 | Medición de copias y ordenamientos en `canonicalizeNestedRecords` | PENDIENTE — REQUIERE DISEÑO |
| PERF-0534 | Cobertura byte a byte del merge y canonicalización de NSMETA | PENDIENTE |
| PERF-0535 | Verificación de que el accesor de evento pertenece al tipo declarante | PENDIENTE — REQUIERE DISEÑO |
| PERF-0536 | Sustitución segura del recorrido de `recordEventAssignmentBinding` por lookup indexado | PENDIENTE — REQUIERE DISEÑO |
| PERF-0537 | Cobertura de orden y diagnóstico para bindings de asignación de eventos | PENDIENTE |
| PERF-0538 | Clasificación trazable de las 150 entradas del ranking del apéndice B | PENDIENTE — REQUIERE DISEÑO |
| PERF-0539 | Gate inverso: cada ID `PERF-*` debe declarar un hallazgo fuente | PENDIENTE |
| PERF-0540 | Gate directo: cada hallazgo accionable debe declarar uno o más IDs `PERF-*` | PENDIENTE |
| PERF-0541 | Reconciliación del cierre histórico de C01/C04 con su ineficacia actual | PENDIENTE |
| PERF-0542 | Clasificación explícita de H-1..H-9 entre rendimiento, refactor y fuera de alcance | PENDIENTE |

## Terminados — 0

Ninguno.
