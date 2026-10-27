# NativeSharp.Core.Threading: auditoría y verificación de Language 3

Fecha: 2026-10-05

## Fuente canónica y alcance

- Fuente canónica: `stdlib/NativeSharp.Core.Threading` en la rama local `development`.
- Producto: nueve archivos `.ns` y `NativeSharp.Core.Threading.nsproj`.
- Dependencia: `NativeSharpCore.lib` con metadata `NativeSharpCore.nsmeta`.
- Lenguaje: `Language 3`.
- Target verificado: Windows x64, O0.

La búsqueda del repositorio no encontró otra implementación de este proyecto. Los consumidores externos y
los probes no se consideraron producto del componente.

## Matriz de cobertura

- `Atomics.ns`: se auditaron `MemoryOrder`, `AtomicFence`, `AtomicInt32`, `AtomicInt64`,
  `AtomicPointer` e `Interlocked`, incluidas todas las familias de carga, almacenamiento y RMW.
- `Event.ns`: se auditaron sus dos constructores, `Set`, `Reset`, `Wait`, destructor y seis
  operaciones internas de runtime.
- `Job.ns`: se auditaron `JobFunction<T>`, `IJob` y `Job<T>`, incluidas dependencias, scheduling,
  cancelación, espera y resultado.
- `Monitor.ns`: se auditaron `Enter`, `Exit`, `Wait`, `Pulse` y `PulseAll`.
- `Mutex.ns`: se auditaron sus dos constructores, `Lock`, `Unlock`, destructor y cinco operaciones
  internas de runtime.
- `ScopedLock.ns`: se auditaron el constructor de adquisición y el destructor de liberación.
- `Semaphore.ns`: se auditaron sus dos constructores, `Wait`, `Release`, destructor y cinco
  operaciones internas de runtime.
- `Thread.ns`: se auditaron `ThreadStart`, constructor, `Join`, `Detach`, destructor, `Sleep` y
  operaciones internas.
- `ThreadPool.ns`: se auditaron sus dos constructores, colas generales y dirigidas, métricas,
  espera, shutdown y worker loop.

Toda la superficie se conservó y compiló. La prueba O0 ejercitó las familias indicadas en la sección de
evidencia; `Thread.Detach` se cubrió por compilación, no mediante ejecución para evitar dejar un hilo suelto.

Los cuerpos vacíos o retornos neutros de las operaciones internas son contratos intrínsecos reconocidos por
NSC y sustituidos por lowering nativo. No se reemplazaron por implementaciones ordinarias.

## Ownership y seguridad

- `Job<T>` mantiene `where T : copy` porque `Result()` puede leerse repetidamente sin consumir el valor.
- `Job<T>.dependencies_` ahora es `owned IJob[]`: el job posee y destruye la asignación del arreglo.
- Los elementos guardados en `dependencies_` siguen siendo referencias no propietarias a otros jobs.
- `ThreadPool` conserva ownership de workers, colas, índices, contadores y monitor.
- `ScopedLock` conserva una referencia no propietaria al mutex; su destructor sólo ejecuta `Unlock()`.
- Los handles Win32 continúan representados por escalares `nint` dentro de clases propietarias.
- Las firmas con `void*` de atomics no desreferencian memoria en el código fuente; el compilador las baja como
  operaciones intrínsecas. Por ello no corresponde agregar bloques `unsafe` artificiales a este componente.
- No existen structs move-only, sentencias `throw` ni efectos `throws` que deban publicarse.

No se cambió ninguna firma pública, algoritmo, orden de sincronización ni semántica de retorno.

## Evidencia de compilación O0

La biblioteca se compiló con:

- compilador: `C:\native-sharp\bin\nsc.exe`;
- versión: NativeSharp 3.0.0, build `2a44eedad51a7a9b`;
- referencia binaria: `A:\nativesharp-libraries\build\O0\NativeSharpCore.lib`;
- salida privada: `A:\nativesharp-libraries\_validation\core-threading`;
- resultado: `project OOP static library emission: ok`;
- fuentes: 9 de 9;
- unidades de código: 11;
- miembros del archivo COFF: 15;
- artefactos producidos: `.lib`, `.h` y `.nsmeta`.

## Prueba puntual del componente

Un ejecutable Language 3 consumió exclusivamente `NativeSharpCoreThreading.lib` y `NativeSharpCore.lib` del
cierre privado. Fue enlazado con `A:\nslink-devel\build-release\bin\nslink.exe` y ejercitó:

- atomics de 32 y 64 bits, atomics de puntero, `Interlocked` y fences;
- evento manual, semáforo, mutex y `ScopedLock`;
- monitor y timeout no bloqueante;
- creación y `Join` de un hilo;
- `ThreadPool`, envío de callback, `WaitIdle` y `Shutdown`;
- especialización y ejecución de `Job<int>` con resultado repetible.

La emisión PE O0 enlazó dos bibliotecas y el ejecutable terminó con el código esperado 42.

## Exclusiones

- No se ejecutó baseline ni minibaseline.
- No se compilaron O1, O2 ni O3.
- No se ejecutaron pruebas de estrés ni casos deliberadamente bloqueantes.
- No se publicó, instaló ni copió ningún artefacto a `C:`.
- No se modificó el compilador ni otro componente.

## Estado

`Verificado` para compilación de biblioteca O0, consumo binario, enlace NSLink y la prueba puntual descrita.

## Nota de distribución / Distribution note (2026-10-25)

ES: En `nsc-devel/lib` y en la distribución (`C:\native-sharp\lib`) esta biblioteca se entrega como fuente,
junto a `NativeSharp.Core`, sin un `NativeSharpCore.lib` en su carpeta. La `LibraryReference` relativa a
`NativeSharpCore.lib` que trajo la sincronización del 2026-10-05 no existía ahí, así que ni la biblioteca ni
sus consumidores compilaban ("referenced library does not exist"). El proyecto vuelve a referenciar Core como
proyecto: `../NativeSharp.Core/NativeSharp.Core.nsproj`. La referencia binaria sigue siendo la forma de
`nativesharp-libraries`, donde `NativeSharpCore.lib` sí se construye al lado.

EN: In `nsc-devel/lib` and in the distribution (`C:\native-sharp\lib`) this library ships as source, next to
`NativeSharp.Core`, with no `NativeSharpCore.lib` in its folder. The `LibraryReference` to a relative
`NativeSharpCore.lib` brought by the 2026-10-05 sync did not exist there, so neither the library nor its
consumers compiled ("referenced library does not exist"). The project references Core as a project again:
`../NativeSharp.Core/NativeSharp.Core.nsproj`. The binary reference remains the form used in
`nativesharp-libraries`, where `NativeSharpCore.lib` is built alongside.
