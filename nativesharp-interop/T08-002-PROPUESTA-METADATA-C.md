# T08-002: funciones, globales y alias C en bibliotecas precompiladas (hallazgo H3)

Estado: **PROPUESTA NO IMPLEMENTADA**. No modifica NSMETA; cualquier cambio de
formato requiere autorización.

## Qué necesita el consumidor precompilado

HECHO COMPROBADO con el fixture `pruebas-tanda-08-identidad-c/plantillas`
(productor `CGenProducer` con `Gen.h`, O0, compilador de este carril):

| Uso de C en la biblioteca | Consumidor | Resultado |
|---|---|---|
| Método no genérico que llama a `gen_c_number()` o lee `gen_c_counter` | sin encabezado | compila y ejecuta |
| Método no genérico que devuelve `GenCount` (`typedef int`) | sin encabezado | compila y ejecuta; la firma publica el tipo canónico |
| Plantilla pública que llama a `gen_c_number()` o lee `gen_c_counter` | sin encabezado | `unknown identifier 'gen_c_number'` / `'gen_c_counter'` en `CGenProducer.nsmeta.public-templates.ns` |
| La misma plantilla | con el mismo `Gen.h` propio | mismo error |
| Plantilla pública que declara un local `GenCount` | sin encabezado | `unknown type name 'GenCount' in module 'CGenProducer'` |

Conclusiones:

1. HECHO COMPROBADO: un cuerpo ya compilado en la biblioteca no necesita metadata
   de funciones, globales ni alias C; el consumidor enlaza la biblioteca.
2. HECHO COMPROBADO: los records y enums de la superficie viajan en el catálogo
   C y el consumidor restaura su identidad por la ruta del driver (resultados de
   T08-002).
3. HECHO COMPROBADO: el cuerpo de una plantilla pública viaja como fuente y el
   consumidor lo analiza en el ámbito del módulo productor. Ahí no existen las
   declaraciones C del productor; las sesiones propias del consumidor
   pertenecen a su proyecto y no son visibles para ese módulo.
4. Por tanto, el consumidor necesita, por módulo productor, exactamente las
   funciones, globales y alias C que usan los cuerpos de plantillas publicados,
   con identidad suficiente para no confundirlos con otras declaraciones.
5. Relacionado (H4): un record publicado cuyo campo es un enum anónimo hace
   fallar al productor con `C enum field dependency has no importable identity`.
   Es un hueco del catálogo de tipos, no de esta propuesta.

## Propuesta mínima de metadata

Una sección opcional nueva dentro del catálogo C ya versionado
(`cppCRecordCatalog`, envoltura de `encodeCTypeCatalogWithProvenance`), con
versión de envoltura 4. Si ninguna plantilla pública usa declaraciones C, la
sección no existe y los bytes del catálogo no cambian (invariante de ausencia).

Entrada de función:

| Campo | Fuente actual |
|---|---|
| nombre y nombre calificado de búsqueda | `CppFunctionDeclaration` |
| USR de Clang (opcional) | `clangUsr` |
| símbolo externo | `externalSymbol` |
| tipo de retorno y parámetros como claves NSC | proyección existente (`CppSemanticFunction`) |
| convención de llamada, variádica, con prototipo, `dllimport`, no lanza | `CppFunctionDeclaration` |
| triple de destino | entrada de inventario |

Entrada de global:

| Campo | Fuente actual |
|---|---|
| nombre, USR, símbolo externo, clave de tipo | `CppSemanticGlobal` |
| const, volatile, atómico, TLS, `dllimport` | `CppSemanticGlobal` |
| tamaño y alineación, semántica volatile de MS | `CppSemanticGlobal` |

Entrada de alias: nombre, nombre calificado, USR y tipo subyacente (`CppType`,
que el codec ya serializa con su profundidad acotada).

Los records y enums que aparezcan en esas firmas se añaden a las semillas del
cierre existente (`provideCppCRecordMetadataCatalog`).

Selección en el productor: solo las declaraciones C demandadas por los cuerpos
de plantillas publicados, con la captura de demanda existente
(`CppFunctionDemand`) aplicada a la fuente de plantillas antes de escribir la
metadata. Una función `static inline` o cualquier declaración interna usada por
una plantilla se rechaza con un diagnóstico propio: necesitaría transportar su
definición, fuera de esta propuesta mínima.

Restauración en el consumidor: reconstruir `NamespaceFunctionSymbol`,
`NamespaceStateSymbol` y `CppImportedTypeAlias` con el ordinal del proyecto
productor e instalarlos con los proveedores que el driver ya configura para
catálogos precompilados (`PrecompiledModuleConfiguration.cpp:545-563`). Si el
consumidor tiene una sesión propia con una declaración del mismo USR, se comparan
símbolo, firma y enlace con las reglas actuales de agrupación de funciones y
globales; una discrepancia es un error, y nunca se une por nombre solo.

Dependencia binaria: la biblioteca C ya se registra como dependencia binaria de la
biblioteca NativeSharp (`LibraryMetadataCppBinaryDependency`). NO VERIFICADO que
se propague cuando el símbolo solo lo usa una plantilla instanciada en el
consumidor.

Versión: el catálogo se distingue por su byte de envoltura y por la verificación
estructural. Que además haya que elevar el minor de NSMETA, como se hizo en
1.43–1.45, es una decisión pendiente.

## Pruebas propuestas

1. Plantilla pública que llama a una función C, lee una global C y usa un
   typedef C; consumidor sin encabezado; O0–O3; ejecución con resultado
   comprobado.
2. El mismo consumidor con el mismo encabezado propio: compila y concilia por USR,
   símbolo y firma.
3. Consumidor con un encabezado propio cuya función del mismo USR tiene otra firma:
   error de conflicto.
4. Plantilla que usa una función `static inline`: diagnóstico del productor.
5. No uso: biblioteca sin plantillas con C; los bytes del catálogo son idénticos
   a los actuales.
6. Compatibilidad: los NSMETA existentes sin la sección se consumen igual
   (fixtures actuales sin modificar).
7. Codec: ida y vuelta, truncamiento, duplicados y profundidad de tipos.
8. Identidad restaurada: USR, símbolo y firma de cada entrada comparados con
   Clang, por la ruta del driver.

## Archivos que cambiarían

`src/Interop/Internal/CppCRecordMetadataCodec.h/.cpp` (sección y envoltura 4),
`src/Interop/Cpp/CppSemanticRecord.cpp` (selección en el productor),
reconstrucción en `CppSemanticInventory.cpp` y `CppSemanticFunction.cpp`,
`src/Driver/PrecompiledModuleConfiguration.cpp` (proveedores),
`src/Interop/LibraryMetadataVerification.cpp`, posiblemente
`include/nativesharp/Interop/LibraryMetadata.h`, pruebas y fixtures nuevos.
Todos tocan metadata o productores compartidos.
