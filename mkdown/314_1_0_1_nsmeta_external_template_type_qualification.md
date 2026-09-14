# Tipos externos calificados en plantillas NSMETA

## Resumen

Esta entrega corrige la reconstrucción de plantillas públicas precompiladas cuando una firma
genérica usa un tipo nominal declarado por otro módulo y el archivo productor lo había hecho
visible mediante un using.

El defecto observado producía una plantilla reutilizable como:

~~~text
public static void FreeArray<T>(IAllocator allocator, T* items, long capacity)
~~~

El fragmento se almacenaba literalmente en NSMETA, pero las directivas using que daban significado
a IAllocator no forman parte de ese fragmento. Al importar la biblioteca, el consumidor analizaba
la plantilla dentro del módulo productor y buscaba IAllocator allí. El tipo real, aun cuando su
identidad completa estaba disponible durante la compilación original, quedaba perdido.

La fuente reutilizable ahora conserva la identidad explícita:

~~~text
public static void FreeArray<T>(
    ForeignTypes::Foreign.Memory.IAllocator allocator,
    T* items,
    long capacity)
~~~

## Causa raíz

**buildMetadataModel** obtenía **LibraryMetadataGenericTemplate.sourceText** copiando directamente
el rango fuente de la declaración genérica. Este mecanismo es válido para sintaxis autocontenida,
pero no para nombres cuyo significado depende del contexto léxico exterior.

Intentar resolver esos nombres durante la escritura tardía de NSMETA tampoco es correcto: las
declaraciones genéricas abiertas ya fueron retiradas del conjunto que procesa
**ProjectNameLookup**, por lo que sus nodos originales no conservan allí un binding confiable.

La identidad debe capturarse cuando **ProjectGenericModel** recorre la plantilla original y aún
conoce:

- el módulo productor;
- el namespace actual;
- los namespaces importados;
- los alias vigentes;
- la declaración nominal elegida por resolución.

## Implementación

### ProjectGenericModel

Se agregó **GenericTemplateExternalTypeReference**. Cada registro conserva:

- el rango fuente exacto del uso;
- la clave nominal canónica con módulo.

**collectTypeDependency** ahora visita recursivamente los argumentos de tipo y, para cada tipo
nominal no genérico, ejecuta **resolveDeclaredTypeIdentity** con el contexto original de la
plantilla. Si la declaración pertenece a otro módulo, guarda la referencia semántica. Las
referencias se ordenan y eliminan duplicados al cerrar el modelo.

Esto cubre tipos externos escritos en parámetros, retornos, variables, restricciones y tipos
anidados recorridos por el AST de la plantilla. Los parámetros de tipo y builtins no se convierten
en dependencias nominales.

### LibraryMetadata

**materializeGenericTemplateSource** reemplaza solamente el nombre base dentro de cada rango
registrado. Conserva calificadores de ownership, argumentos genéricos, punteros y arreglos, pero
transforma el nombre nominal a:

~~~text
Module::Namespace.Type
~~~

Los reemplazos se aplican del final al principio para no invalidar offsets y se rechazan rangos
malformados o superpuestos. La misma pasada incorpora los tipos nominales concretos a
**externalTypeKeys**, de modo que la clausura NSMETA describe también las dependencias usadas
exclusivamente por plantillas reutilizables.

No se transportan directivas using ni se intenta repetir la resolución en el consumidor. El
contrato precompilado queda autocontenido y determinista.

## Archivos modificados

- **include/nativesharp/Frontend/ProjectGenericModel.h**: nuevo registro semántico y colección por
  plantilla.
- **src/Frontend/ProjectGenericModel.cpp**: descubrimiento, resolución, orden y deduplicación de
  tipos externos.
- **src/Interop/LibraryMetadata.cpp**: materialización calificada de sourceText y ampliación de la
  clausura externalTypeKeys.
- **tests/unit/Stage48_2ExternalTemplateTypeQualificationTests.cpp**: fixture O0 con proveedor,
  productor y consumidor precompilados.
- **CMakeLists.txt**: registro del ejecutable de prueba puntual.

## Evidencia enfocada

La prueba construye tres módulos independientes:

1. **ForeignTypes** publica **Foreign.Memory.IAllocator**.
2. **CollectionTemplates** importa ese namespace y publica **FreeArray<T>** y
   **TryAllocateArray<T>** usando el nombre corto.
3. **TemplateConsumer** referencia únicamente los artefactos precompilados de ambos módulos e
   instancia **FreeArray<int>**.

También lee el NSMETA del productor y comprueba tanto el sourceText calificado de las dos firmas
como la presencia de la identidad externa canónica.

Resultado:

~~~text
PASS: 7 checks
~~~

No se ejecutó baseline ni se compiló el runtime externo para esta entrega.

## Commit de referencia

**Título ES:** Conserva tipos externos en plantillas NSMETA

**Title EN:** Preserve external types in NSMETA templates

**Descripción ES:** Registra durante el análisis genérico las identidades nominales externas
usadas por plantillas reutilizables y reescribe su fuente NSMETA con
Module::Namespace.Type. Extiende la clausura externalTypeKeys y añade una prueba O0
productor/consumidor.

**Description EN:** Record external nominal identities during generic analysis and rewrite
reusable NSMETA template source with Module::Namespace.Type. Extend the externalTypeKeys closure
and add a focused O0 producer/consumer test.

# Module-qualified external types in NSMETA templates

## English design summary

Reusable template source previously copied the declaration text without its surrounding using
