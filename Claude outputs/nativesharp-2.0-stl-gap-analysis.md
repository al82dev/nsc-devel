# NativeSharp 2.0 — Análisis funcional frente a la STL de C++

**Fecha:** 23 de septiembre de 2026
**Alcance:** `A:\nsc-devel\lib`, `C:\native-sharp\lib`, `A:\native-sharp-runtime-2.0` y comparación funcional con la STL de C++11–23
**Plataforma oficial:** Windows x64
**Compilador de referencia:** `nsc 2.0.0+build...5500000000000001` (NativeSharp 2.0.0, stage 70)

> **Revisión del 23 de septiembre.** Las afirmaciones sobre niveles de optimización, estado de
> artefactos y tipos ausentes se contrastaron contra el disco ese día. Lo que cambió respecto a la
> versión anterior está marcado con **[corregido]**. El método está al final, para que cualquiera
> pueda repetirlo.

## Veredicto

NativeSharp 2.0 ya no tiene una biblioteca estándar pequeña. El runtime contiene 16 módulos de
primer nivel y **904 ficheros `.ns`** bajo sus directorios `src`, sin contar pruebas, probes ni la
copia de la stdlib. Funcionalmente cubre una parte considerable de C++11–23 y, en algunas áreas,
ofrece más que la STL.

No existe compatibilidad de API ni de código fuente con C++, ni ése es el objetivo. La comparación
de este documento se refiere a capacidades funcionales.

## Política de plataforma

Windows x64 es la plataforma oficial y el objetivo deliberado de diseño de NativeSharp.

- Win32, COFF, BCrypt, DbgHelp, Winsock y LLVM para Windows son dependencias intencionales.
- Linux y macOS quedan fuera del alcance y no forman parte del roadmap.
- No se considera la ausencia de backends Linux/macOS un gap funcional del proyecto.
- No deben introducirse capas multiplataforma que compliquen, ralenticen o diluyan la
  implementación para Windows.
- Las abstracciones internas sólo deben existir cuando mejoren mantenibilidad, pruebas, rendimiento
  o claridad dentro del objetivo Windows.

## Niveles de optimización — **[corregido]**

**El compilador certifica dos niveles, no cuatro.** Su propio contrato, leído con
`nsc --print-toolchain-contract`:

```
NSTOOLCHAIN 2
compiler_release=2.0.0
stage=70
certified_optimizations=O0,O2
artifact_kinds=exe,static-lib,dll
```

La versión anterior de este documento pedía certificar el runtime "en O0, O1, O2 y O3". Ese
objetivo no es alcanzable tal como está escrito: O1 y O3 no aparecen en el contrato. Puede que las
banderas se acepten —no se ha verificado— pero no están respaldadas.

En consecuencia, la herramienta de bindings `tools\nsb.py` expone `--opt` limitado a `O0` y `O2`, y
rechaza cualquier otro valor.

## Cobertura existente

### Contenedores y memoria

- `List`, `LinkedList`, `Queue`, `Stack` y `PriorityQueue`.
- `Dictionary`, `HashSet`, `SortedDictionary`, `SortedSet` y `SortedList`.
- `FlatMap`, `FlatSet`, `FlatHashMap` y `FlatHashSet`.
- `BitSet`, operaciones sobre bitsets y serialización.
- `BinaryHeap`, `SpanHeap` y `RingBuffer<T>`.
- Listas y colas intrusivas.
- `Span<T>` y `ReadOnlySpan<T>`.
- `IAllocator`, `SystemAllocator`, arenas lineales, slabs, block pools, tracking y fallos
  inyectados.

### Texto

- Procesamiento UTF-8.
- Conversión y formateo de enteros y flotantes.
- Representación decimal shortest-roundtrip para IEEE-754.
- Compilador y matcher de expresiones regulares propios.
- Tokenizers, quoting, `SpanWriter`, `StringBuilderWriter` y `FormatWriter`.

El motor regex decodifica puntos de código UTF-8. Actualmente el case folding y las clases
abreviadas como `\w` son ASCII.

### Sistema

- Archivos, directorios, glob, árboles, rutas, enlaces y metadatos.
- Procesos, argumentos, environment, pipes, espera y códigos de salida.
- Bibliotecas nativas.
- Memoria mapeada y memoria compartida.
- Reloj monotónico, UTC, durations, deadlines, timers y sleep.
- Mutexes, semáforos, eventos y objetos de sincronización nombrados.
- Información de CPU, sistema operativo, volúmenes y espacio en disco.

### Concurrencia

- Threads y scoped threads.
- Job system con workers, afinidad, métricas y ejecución determinista.
- `Parallel.For`, `Parallel.ForDeterministic` y `Parallel.ForSerial`.
- Colas SPSC, MPSC y blocking queues.
- `Latch`, `Barrier` y `JobCounter`.
- `Future<T>` con resultado `owned`, error, timeout y consumo único.
- `CancellationSource` y `CancellationToken`.

`NativeSharp.Jobs` publica 33 tipos y dos plantillas públicas, `BlockingQueue<T>` y `Future<T>`.

### Diagnósticos y dominios adicionales

- Stack traces, mapa de módulos, DbgHelp y contexto de crash.
- Hashing: SHA, HMAC, CRC, SipHash, xxHash y Bloom filters.
- Networking: DNS, TCP, UDP, HTTP, TLS, URI y sesiones.
- Logging estructurado y asíncrono.
- Grafos.
- Compresión, archives, checksums y transferencias.

### Matemáticas

- Vectores, matrices, geometría, transformaciones y proyecciones.
- Quaternions, curvas, ruido, random, estadística y solvers.
- NURBS.
- `NdArray` con shapes, strides, slicing, views, transposición, reducciones y ufuncs.
- Álgebra lineal en desarrollo: LU, Cholesky, normas, productos y matrices triangulares.

`NativeSharp.Math.Core` son **313 ficheros** y 208 tipos públicos, todos en el namespace
`NativeSharp.Math`. De esos ficheros, 99 son matrices y 44 vectores: once familias de tipo
repetidas por tamaño.

## Gaps funcionales reales

### Verificación de ausencia — **[corregido]**

Los tipos que siguen se buscaron por declaración (`class`, `struct`, `interface`, `enum`) en los
904 ficheros del runtime y en los 86 de la stdlib instalada. Resultado:

| tipo | runtime | stdlib instalada |
|---|---|---|
| `Optional` | no | **sí** — `NativeSharp.Core\Optional.ns` |
| `Span` | no | **sí** — `NativeSharp.Core\Span.ns` |
| `Atomic` | no | **sí** — `NativeSharp.Core.Threading\Atomics.ns` |
| `Complex` | **parcial** — sólo `Math\src\Scalars\ComplexMath.ns` | no |
| `Result`, `Variant`, `Tuple` | no | no |
| `FunctionRef`, `InlineFunction`, `OwnedFunction` | no | no |
| `ScopeExit`, `ScopeGuard` | no | no |
| `SharedPtr`, `WeakPtr`, `RefCounted` | no | no |
| `Deque`, `ForwardList`, `MultiMap`, `MultiSet` | no | no |
| `SmallVector`, `StringName`, `ObjectPool` | no | no |
| `SharedMutex`, `AtomicRef`, `Promise` | no | no |
| `Fft`, `Svd`, `Eigen` | no | no |
| `TimeZone`, `Normalization`, `Grapheme` | no | no |
| `Iterator` (contrato genérico) | no | no |

Es decir: la lista de huecos de las secciones siguientes no es una impresión, está comprobada.
`Complex` merece el matiz de "parcial": existe aritmética compleja en Math, pero no un tipo público
y completo.

### 1. Iteradores, ranges y algoritmos genéricos

Es el gap horizontal más importante.

- Contrato uniforme de iteradores.
- Ranges y views lazy.
- Pipelines `transform`, `filter`, `take`, `drop` y equivalentes.
- Proyecciones y output iterators.
- Catálogo completo de sorting, partition, merge, set operations, transform, reduce, scan y search.
- Versiones paralelas generalizadas.

### 2. Tipos utilitarios genéricos

- `Result<T,E>` o equivalente consolidado.
- `Variant`.
- Tuples.
- Callables con ownership: `FunctionRef`, `InlineFunction` y `OwnedFunction`.
- Traits y constraints de uso general.
- `source_location` proporcionado por el compilador.
- Scope guards: `scope_exit`, `scope_fail` y `scope_success`.

`Optional<T>` ya existe en Core.

### 3. Contenedores especializados

- `deque`.
- `forward_list`.
- `multimap` y `multiset`.
- Interval maps y sets.
- Small-vector o almacenamiento inline.
- Bounded strings y bounded lists.
- String interning/StringName.
- Object pools tipados.
- Generational handles.
- Operaciones avanzadas de árboles asociativos: búsqueda heterogénea, extract, merge y node
  handles, cuando resulten útiles para NativeSharp.

### 4. Unicode y formatting

- Normalización Unicode.
- Case folding Unicode completo.
- Categorías Unicode.
- Segmentación por grafemas.
- Comparación y collation Unicode.
- Clases Unicode para regex.
- Format strings verificables durante compilación.
- Sistema completo de formatting y print.
- `SpanReader` simétrico a `SpanWriter`.
- Política unificada para strings owning, views y builders.

### 5. Ownership compartido

NativeSharp cubre propiedad única mediante `owned`, `borrowed`, `move` y `raii`; no necesita copiar
`unique_ptr`.

Queda pendiente decidir y documentar el tratamiento de:

- Shared ownership.
- Weak references.
- Conteo de referencias intrusivo o no intrusivo.
- Aliasing ownership.
- Ciclos y costos atómicos.

Esto no implica que deba copiarse `shared_ptr`. Puede rechazarse deliberadamente o implementarse
una solución coherente con las reglas del lenguaje.

### 6. Concurrencia avanzada

- `shared_mutex`.
- `atomic_ref`.
- Espera y notificación sobre atomics.
- Promises separadas de futures.
- Continuations.
- `when_all` y `when_any`.
- Shared futures.
- Integración estructurada entre cancelación, futures y jobs.
- Executors o schedulers públicos, sólo si encajan con el modelo del lenguaje.
- Coroutines y generators, si se decide incorporarlos al lenguaje.

### 7. Tiempo avanzado

- Zonas horarias de Windows.
- Local time y conversiones por zona.
- DST y reglas históricas.
- Leap seconds si el proyecto los necesita.
- Calendarios y operaciones calendáricas más amplias.

### 8. Matemáticas y cómputo científico

- Terminar y certificar `NativeSharp.Math.LinearAlgebra`.
- QR, SVD y eigenvalues/eigenvectors.
- Matrices sparse.
- FFT y otras transformadas.
- ODE solvers.
- Precisión arbitraria.
- Unidades dimensionales.
- Cálculo intervalar ampliado.
- Tipo complejo público y completo — hoy sólo hay aritmética en `ComplexMath.ns`.
- Backends vectorizados y operaciones tipo BLAS cuando aporten rendimiento medible.

## Estado de madurez — **[corregido]**

Debe distinguirse entre código existente, artefactos compilados y certificación vigente. La versión
anterior decía que no había evidencia de un baseline reciente. **Ya la hay, en O0.**

### Lo que sí está certificado hoy

El 22 de septiembre se reemitieron las doce librerías base con el compilador vigente y se
reconstruyó `NativeSharp.Jobs` después de que se arreglara el bug del calificador de módulo. El
conjunto completo enlaza como artefactos precompilados:

```
nsc --check-project-oop-hir probes\all_libraries\AllLibraries.nsproj
project OOP HIR check: ok (26826 callables)
```

**18 de 18**, en `A:\native-sharp-runtime-2.0\build\O0\`: Foundation, Hashing, Memory, Text,
Collections, System, Graph, Logging, Net, Diagnostics, Jobs, Transfer, Math.Core, Math.Intervals,
Math.Root, Math.Solvers, Math.Statistics, más las tres artefactos de stdlib contra los que fueron
registradas.

Además, las dieciséis metadatas escritas por el compilador anterior se siguen leyendo con el
vigente, así que no hubo ruptura de formato.

### Lo que no está certificado

- **O2 no se ha construido.** El baseline es O0. Y hay un obstáculo concreto: las rutas
  `LibraryReference` escritas dentro de los `.nsproj` apuntan a `build\O0`, así que un pase O2
  emitiría en `build\O2` pero enlazaría dependencias O0. Hay que retargetarlas antes.
- **Tres de los dieciséis módulos están fuera del conjunto vigente:**

| módulo | estado |
|---|---|
| `NativeSharp.Math.LinearAlgebra` | 7 ficheros fuente, **sin `bin` ni artefactos** |
| `NativeSharp.Math.Arrays` | 30 ficheros, artefacto del 14 de septiembre en su propia `bin\O0` |
| `NativeSharp.Math.Nurbs` | 15 ficheros, artefacto del 14 de septiembre en su propia `bin\O0` |

Las tres siguen declarando `ProjectReference`, es decir recompilan las fuentes de sus dependencias
dentro de sí mismas en vez de enlazarlas. "Tener `.lib`" y "estar en el conjunto vigente" no son lo
mismo.

- Los documentos de ejecución que describen compiladores 0.9 / stage 67 son historia, no
  certificación.

## Prioridades recomendadas — **[corregido]**

1. **Certificar el runtime en O2.** O0 ya está. Requiere antes retargetar las rutas
   `LibraryReference` de los `.nsproj`, que hoy apuntan fijas a `build\O0`. O1 y O3 salen de la
   lista: no los certifica el compilador.
2. Incorporar las tres piezas de Math que están fuera del conjunto: convertir `Math.Arrays`,
   `Math.Nurbs` y `Math.LinearAlgebra` a `LibraryReference` y emitirlas al `build\O0` común.
3. Implementar iteradores, ranges, views y algoritmos genéricos.
4. Consolidar `Result`, `Variant`, tuples y callables con ownership.
5. Completar Unicode, formatting y lectura/escritura por spans.
6. Terminar álgebra lineal y el tipo complejo público.
7. Completar los contenedores especializados que tengan casos de uso reales.
8. Ampliar composición de jobs, futures y cancelación.
9. Medir y mejorar rendimiento, estabilidad, diagnósticos y documentación.

## Conclusión

NativeSharp Runtime 2.0 ya se parece más a una biblioteca de sistema moderna —una combinación de
biblioteca estándar, capacidades equivalentes a partes de Boost y servicios propios de plataforma—
que a una colección auxiliar.

La deuda principal no es la cantidad de módulos ni la portabilidad fuera de Windows. Es la capa
genérica que debe permitir combinar uniformemente contenedores, algoritmos, ownership y
concurrencia, más la certificación en O2 y la incorporación de los tres módulos de Math que hoy
quedan fuera del conjunto.

## Cómo se verificó

Para poder repetirlo:

- **Niveles certificados:** `C:\native-sharp\bin\nsc.exe --print-toolchain-contract`.
- **Baseline del runtime:** `nsc --check-project-oop-hir
  A:\native-sharp-runtime-2.0\probes\all_libraries\AllLibraries.nsproj`, que nombra cada artefacto
  como `LibraryReference`. Un consumidor que nombra un artefacto tiene que nombrar todo aquello
  contra lo que fue registrado, o nsc rechaza el proyecto con *precompiled LibraryReference type
  closure is incomplete* — por eso el probe es una prueba real de cierre y no un formalismo.
- **Conteo de fuentes y módulos:** recorrido de `A:\native-sharp-runtime-2.0` excluyendo `bin`,
  `obj`, `build`, `deploy`, `tests`, `probes`, `stdlib`, `.nsb-work` y `.nsb-out`.
- **Ausencia de tipos:** búsqueda de las declaraciones `class`, `struct`, `interface` y `enum` de
  cada nombre en esos 904 ficheros y en los 86 de `C:\native-sharp\lib`.
- **Estado de los módulos de Math:** listado de sus carpetas `bin\O0` y marcas de tiempo.
