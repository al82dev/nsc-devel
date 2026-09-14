# Identidad física de callables con genéricos multimódulo

## Resumen

Esta entrega corrige el emparejamiento de un callable precompilado cuando su firma contiene una
especialización genérica cuyo template y argumento nominal pertenecen a módulos diferentes.

El productor publicaba correctamente el método y su símbolo físico en NSMETA. Al reconstruir la
API, el consumidor también encontraba todos los módulos, pero creaba una clave distinta para la
misma especialización. Por ello **ProjectMemberModel::bindPrecompiledPhysicalNames** no podía
asociar la declaración sintética con la identidad física ya publicada.

El caso reducido conserva la forma completa del reporte:

~~~text
Bfs.Run(
    AdjacencyGraph,
    ReadOnlySpan<NodeId>,
    Span<int>,
    Span<int>,
    Span<NodeId>,
    NodeVisitor)
~~~

Aquí ReadOnlySpan y Span pertenecen a un módulo store, NodeId a un módulo de identificadores,
AdjacencyGraph a almacenamiento, y Bfs junto con NodeVisitor al módulo core.

## Causa raíz

El generador de fuente pública reconstruye correctamente los argumentos externos mediante la
forma explícita:

~~~text
GraphIds::Native.Graph.NodeId
~~~

**ProjectNameLookup** ya entiende que **Module::Type** es una solicitud de identidad exacta. Sin
embargo, **ProjectGenericModel::resolveDeclaredTypeIdentity**, que se ejecuta antes del name lookup
ordinario y construye las claves de monomorfización, no implementaba esa rama.

En consecuencia:

- el productor resolvía NodeId a **type:GraphIds::Native.Graph.NodeId**;
- el consumidor trataba la escritura calificada como un nombre no resuelto;
- la clave del consumidor incluía texto y origen sintético en vez de la identidad nominal;
- ReadOnlySpan<NodeId> y Span<NodeId> adquirían nombres físicos locales diferentes;
- la tabla de aliases no encontraba especializaciones con claves semánticas iguales;
- el callable Bfs.Run no coincidía con el miembro NSMETA que sí contenía su mangledName.

El callback no era la causa. Permaneció en el fixture para demostrar que su presencia dentro de la
misma firma sigue funcionando.

## Implementación

### Resolución genérica explícita por módulo

**ProjectGenericModel::resolveDeclaredTypeIdentity** reconoce ahora el separador **::** antes de
aplicar la escala ordinaria de nombres de NativeSharp.

La resolución:

1. separa el módulo solicitado y el nombre de tipo;
2. exige que ambos componentes sean no vacíos;
3. busca una declaración cuya identidad pertenezca exactamente al módulo y cuyo sourceName
   coincida con el tipo solicitado;
4. permite miembros no públicos únicamente dentro del mismo módulo;
5. rechaza una respuesta ambigua en lugar de elegir por orden;
6. devuelve la identidad canónica que alimenta la clave de especialización.

Esto mantiene alineados **ProjectGenericModel** y **ProjectNameLookup** sin cambiar la sintaxis
pública, el formato NSMETA ni el algoritmo de mangling.

### Asociación física

No se modificó el emparejamiento de callables. Una vez que productor y consumidor comparten la
misma clave semántica, la infraestructura existente puede construir el alias:

~~~text
tipo especializado local -> tipo especializado publicado
~~~

La firma sintética se canonicaliza con ese alias y recupera el mangledName exacto almacenado por
el productor. El consumidor no vuelve a emitir la función.

## Archivos modificados

- **src/Frontend/ProjectGenericModel.cpp**
  - amplía **resolveDeclaredTypeIdentity** con resolución exacta de **Module::Type**;
  - conserva las reglas de visibilidad para declaraciones internas.
- **tests/unit/Stage48_2CrossModuleCallableIdentityTests.cpp**
  - crea cinco módulos O0 independientes;
  - verifica emisión y lectura del NSMETA productor;
  - consume el callable desde artefactos LibraryReference.
- **CMakeLists.txt**
  - registra la prueba enfocada.

No quedaron cambios de instrumentación en **ProjectCompilation** ni **ProjectMemberModel**.

## Evidencia enfocada

El fixture compila, en orden:

1. **SpanStore**, con Span<T> y ReadOnlySpan<T>;
2. **GraphIds**, con NodeId;
3. **GraphStorage**, con AdjacencyGraph;
4. **GraphCore**, con NodeVisitor y Bfs.Run;
5. **GraphOrdering**, consumidor exclusivo de los cuatro artefactos precompilados.

Se comprueba que Bfs.Run tenga mangledName en el NSMETA productor y que el consumidor pueda enlazar
su identidad física pese a contener dos genéricos sobre NodeId, dos Span<int> iguales y el callback.

Resultado:

~~~text
PASS: 7 checks
~~~

No se ejecutó baseline ni se compiló el runtime externo.

## Commit de referencia

**Título ES:** Resuelve callables genéricos entre módulos

**Title EN:** Resolve generic callables across modules

**Descripción ES:** Enseña al modelo genérico a resolver Module::Type como identidad nominal
exacta antes de construir claves de especialización. Esto permite asociar callables NSMETA cuyas
firmas combinan templates, argumentos y callbacks de varios módulos. Añade un fixture O0 de cinco
módulos con ReadOnlySpan<NodeId>, Span<NodeId>, Span<int> y NodeVisitor.

**Description EN:** Teach the generic model to resolve Module::Type as an exact nominal identity
before building specialization keys. This lets NSMETA bind callables whose signatures combine
templates, arguments, and callbacks from multiple modules. Add a focused five-module O0 fixture
covering ReadOnlySpan<NodeId>, Span<NodeId>, Span<int>, and NodeVisitor.

# Physical identity for callables with cross-module generics

## English design summary

The NSMETA writer already published the callable and its mangled symbol. The mismatch was created
while the consumer rebuilt a provider-owned closed generic: the early generic model did not
understand the module-qualified type spelling emitted by the public-source bridge, even though
ordinary name lookup did.

Exact module-qualified lookup now returns the same canonical declared-type identity used by the
producer. Specialization keys match again, the existing local-to-published type alias becomes
available, and callable binding recovers the producer physical symbol without changing NSMETA.
