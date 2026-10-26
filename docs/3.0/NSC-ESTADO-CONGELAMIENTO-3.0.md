# NativeSharp 3.0 — Estado para el congelamiento

- **Fecha**: 2026-10-25
- **Rama**: `development-release-candidate` (sin push)
- **Compilador validado**: `build-clang\bin\nsc.exe` construido desde `73b03a394` (pendientes P-1 a P-7:
  `d8687f28a` y `73b03a394`)
- **Plan**: `NSC-PLAN-CONGELAMIENTO-3.0.md`, 108 de 108 elementos terminados

## Resumen

Se cerraron los doce puntos del plan. Cada punto tiene sonda reproducida antes del cambio, corrección,
pruebas nuevas por ejecución en `tests/unit/Freeze30ContractTests.cpp` y un commit propio. Durante el
trabajo aparecieron defectos que el plan no listaba ("hallazgos"); se corrigieron en el mismo punto y
quedan registrados abajo.

Validación final por línea de comandos, en `-O0 -g` y en `-O2`:

- **98 casos** de `Freeze30ContractTests` (86 de los doce puntos y 12 de los pendientes) compilados y
  ejecutados con `nsc` (sin correr la suite unitaria): **98 correctos** en ambos niveles (anexo A).
  `P5DllMissing` solo se compila (el programa no puede cargar a propósito).
- **168 sondas originales** de los doce puntos: el mismo resultado en ambos niveles (anexo B).
- `checkall`: 349 proyectos, 75 fallas, **idénticas** a la base previa al congelamiento (ninguna nueva).
- `casecheck`: 52 casos de `tests/cases`, 0 fallas.
- La compiler-suite (`nsc_component_tests`) compila; sus 113 fixtures async/yield/await se ejecutaron por
  línea de comandos (`fixrundirs`). Fuera de los fixtures que no son autocontenidos (no compilan solos,
  ruido conocido de la herramienta), el único resultado distinto fue `TL-LANG-520`, actualizado en C-17
  porque ahora sí recibe el error que antes se perdía.

La matriz detectó además que tres pruebas de C-17 habían perdido su línea `class Program` al copiarse
de las sondas; se corrigió en `bf4834cf3`.

## Puntos

| Punto | Qué queda garantizado | Commit(s) | Pruebas |
|---|---|---|---|
| C-08 | `IAllocator` con tamaño y alineación explícitos: `new (alloc) T` pide `Allocate(sizeof(T), alignof(T))` y `Deallocate` recibe tamaño y alineación exactos. | `6758083f5` | `testAllocatorAlignment`; 4 fixtures de allocator de la suite |
| C-06 | La superficie de lenguaje 3.0 publicada es la real; `case [..]` se rechaza a propósito (list patterns en `match`/`is`). | `73150c308` | `NativeSharp3T49LanguageSurfaceTests` |
| C-07 | `NativeSharp.Core.IDisposable` publicado; `using` llama `Dispose` una vez en cada salida. | `7b4f91771` | `testCoreDisposable` |
| C-01 | Todo `new` llega a un dueño (`raii`/`owned`, retorno o parámetro owned) o es error; `throw` toma la propiedad; el `catch` es dueño de la excepción. | `ea0da601c` | `testLostOwnershipRejected`, `testOwningFormsAccepted`, `testCatchOwnsException` |
| C-02 | `await` síncrono de un productor que puede suspender es error; el poll/MoveNext es corrutina con frame residente y despacho por estado. | `ea0da601c` | `testSynchronousAwait*`, `testAsyncMainEntryPoint`, `testCoroutineResume`, `testCoroutineFrameLocals`, `testGeneratorResume`, `testGeneratorEarlyStop` |
| C-05 | Un archivo `.ns` se compila como su proyecto implícito; el driver ya no llega al pipeline 1.x; rutas sin `return` y globales 1.x se rechazan. | `2e154f127` | `testFileModeMatchesProject`, `testCaptureSurvivesSuspension`, `testMissingReturn`, `testGlobalDeclarationsRejected` |
| C-11 | Structs con destructor o campos owned: destrucción por scope, drop-flags en moves, move-only, campos y temporales destruidos. | `4d6237d84` | `testValueLocalLifetimes`, `testValueMoves`, `testStructEnumerator`, `testValueFieldsAndTemporaries` |
| C-12 | Un arreglo propietario destruye sus elementos struct antes de liberar el buffer; `delete items[i]` y `move(arr[i])` dejan el slot en cero. | `0b6da967f`, `01f408b04` | `testValueArrayElements`, `testValueListElements` |
| C-13 | Todo error declarado (`throws E`) se captura o se declara en el llamador: llamada, delegate, accesor, operador, `new`, `this`/`base` y `await`. | `8d46bf5cc` | `testDeclaredErrorsRejected`, `testDeclaredErrorsHandled` |
| C-14 | Un future que no completa libera su frame y sus parámetros owned (`F$drop`); los futures son move-only y `await f` consume el local. | `717c9e5f9` | `testFutureDrops`, `testFutureMoveOnly` |
| C-16 | Un generador destruye exactamente los locales inicializados; con `unwind-native` un pánico corre cada cleanup activo una vez. | `0f182413c` | `testGeneratorCleanup` |
| C-17 | El error de un generador async llega completo al future de `MoveNextAsync`, con su cleanup activo; el poll del generador async es corrutina. | `bc8fca456`, `bf4834cf3` | `testAsyncGeneratorErrors`, `testAsyncGeneratorResume`, `TL-LANG-520` |

## Hallazgos corregidos fuera de la lista original

- **C-01**: `throw` de un local raii usaba la excepción ya destruida; una excepción atrapada nunca se
  destruía; operadores y conversiones con retorno owned no transferían la propiedad.
- **C-02**: el poll/MoveNext repetía el cuerpo desde arriba; todos los `await` compartían estado y slot;
  el awaiter reemplazaba el frame del constructor; `ref` a locales colgaba tras suspender; un generador
  abandonado no corría sus `finally`.
- **C-05**: un método que podía terminar sin valor compilaba con `unreachable`; lambdas que capturan
  locales de corrutinas no compilaban; `return` en lambda de generador se rechazaba.
- **C-11**: structs con destructor eran copiables (doble destrucción); campos struct de objetos y
  temporales owned no se destruían.
- **C-12**: `delete items[i]` de un struct era no-op (List/Stack/Queue/LinkedList no destruían valores);
  `move(arr[i])` dejaba el slot intacto.
- **C-13**: un `throw` o un `await` con error dentro de un `try` de un cuerpo async saltaba el `catch`.
- **C-14**: el cleanup de un parámetro owned en un poll destruía el future; locales future fuera del frame;
  frames de constructor nunca liberados y frames de cero bytes.
- **C-17**: el poll del generador async repetía el cuerpo en cada `MoveNextAsync` (efectos repetidos,
  locales recreados perdidos, doble destrucción tras un error); un `await` sin `try` en un cuerpo async no
  destruía sus locales al propagar, y uno síncrono perdía el error; destruir un iterador async detenido no
  corría su cleanup; `Cancel` antes de empezar no lo terminaba; el constructor reservaba dos frames. Una
  lambda ya puede capturar un local de un generador async.

## Pendientes corregidos después del cierre

Al cerrar el plan quedaron diez limitaciones anotadas (commits `d8687f28a`, DLL, y `73b03a394`, el
resto). Se revisaron todas con sondas; las que eran
defectos se corrigieron y las demás se reclasificaron con evidencia (plan: elementos 102 a 108).

| Pendiente | Resultado | Evidencia |
|---|---|---|
| P-1 `throw` / `throw;` sin chequeo | Corregido: siguen la regla de una llamada (catch del try envolvente o `throws` del callable). Un throw en async sin `throws` tronaba el compilador; ahora es diagnóstico. Se agregó `throws` en book_examples, examples, tests y fixtures C++; los destructores atrapan lo que lanzan. | `testCheckedThrow`: 3 rechazos y `P1Handled` 789 |
| P-2 errores tipados de generador que no son excepción | No producible: `throw` y `throws` exigen `NativeSharp.Core.Exception`. | sonda P2 |
| P-3 arreglos `owned` locales y campos | Corregido: un local `owned T[]` es dueño como `raii T[]`; un campo `owned T[]` lo libera la entrada de destrucción del contenedor con sus elementos; `delete[]` del campo libera el bloque y lo vacía (también en clases genéricas). | `testOwnedArrays`: 1243, 65, 56, 42, 21 y un rechazo |
| P-4 temporal receptor `Make().Campo` | Corregido: con resultado de datos simples el temporal se destruye tras leer el campo. | `P4Receiver` 678 |
| P-5 imports de DLL sin resolver | Corregido: un símbolo que una DLL del sistema no exporta se rechaza al compilar; una DLL que no se encuentra se omite. | `P5DllCalled` rechazo, `P5DllMissing` compila |
| P-6 `await` en `catch`/`finally` de async que propaga | La limitación era incorrecta: el cleanup ya corre. | sondas 405 y 415 |
| P-7 `Cancel()` de iterador async detenido en `await` | Inalcanzable: el consumidor está suspendido dentro del mismo `await` y un future no queda a medias. | análisis y sonda de `MoveNextAsync` con préstamo vivo (rechazada) |

Durante P-3 apareció un defecto propio del cambio: `delete[]` de un campo owned no vaciaba el campo
cuando el análisis del cuerpo corría por callable (el merger no copiaba esos sitios) ni en la ruta
iterativa del HIR, y `List<T>` liberaba su buffer dos veces. Se corrigió antes del commit; las sondas
`C12List*` vuelven a dar 12, 12345 y 22013 en `-O0` y `-O2`.

## Limitaciones conocidas al congelar

1. Desde el código fuente un future no puede quedar suspendido (no hay poll parcial: solo `await` lo
   consulta, hasta completar); por eso `F$drop` de un future ya iniciado no tiene frame propio que liberar.
2. Un slot de arreglo o de struct movido o borrado queda con su valor por defecto, y ese valor se destruye
   después: el destructor debe tolerar el valor por defecto.
3. `delete[]` explícito de un arreglo libera el bloque y no recorre sus elementos (el modelo manual de
   `~List()`: cada elemento se borra a mano). Sin `delete[]`, el dueño (local `raii`/`owned` o el
   contenedor de un campo `owned`) libera el arreglo con sus elementos.
4. Una excepción que sale de `Main` termina el proceso con el código nativo `0xE04E5301`; en `-O2` el
   reporte de errores de Windows lo demoró de forma intermitente en las sondas, por eso no hay prueba
   unitaria de ese caso.

## Cómo se corrió la matriz

- `A:\temp\matrix2.py`: cada caso de `Freeze30ContractTests` (fuente, preludio y expectativa extraídos del
  archivo de pruebas) como proyecto con referencia a `NativeSharp.Core` (y a `Collections.Generic` o
  `PanicProfile` cuando la prueba lo pide), compilado con `nsc -O0 -g` y `nsc -O2`, ejecutado y comparado
  con el código de salida, la salida estándar o el fragmento de diagnóstico esperado.
- `A:\temp\matrix.py`: las sondas de `A:\temp\cong\src` de cada punto en los mismos dos niveles,
  comparando ambos resultados.

### Anexo A — casos de Freeze30ContractTests por línea de comandos

| Punto | Caso | Esperado | -O0 -g | -O2 | Veredicto |
|---|---|---|---|---|---|
| C-08 | `C08AllocatorAlignment` | 0 | 0 | 0 | ok |
| C-07 | `C07CoreDisposable` | 2 | 2 | 2 | ok |
| C-01 | `C01OwningForms` | 19 | 19 | 19 | ok |
| C-01 | `C01CatchOwnership` | 0 | 0 | 0 | ok |
| C-02 | `C02SynchronousProducers` | 56 | 56 | 56 | ok |
| C-02 | `C02AsyncMain` | 510 | 510 | 510 | ok |
| C-02 | `C02ResumeTwoAwaits` | 112 | 112 | 112 | ok |
| C-02 | `C02ResumeParameterLoop` | 33 | 33 | 33 | ok |
| C-02 | `C02ResumeNested` | 18 | 18 | 18 | ok |
| C-02 | `C02ResumeLoopAwait` | 6 | 6 | 6 | ok |
| C-02 | `C02FrameRaii` | 142 | 142 | 142 | ok |
| C-02 | `C02FrameBorrow` | 10 | 10 | 10 | ok |
| C-02 | `C02GeneratorPrefix` | 12111 | 12111 | 12111 | ok |
| C-02 | `C02GeneratorLoop` | 1231 | 1231 | 1231 | ok |
| C-02 | `C02GeneratorBreak` | 61 | 61 | 61 | ok |
| C-02 | `C02GeneratorNotStarted` | 5 | 5 | 5 | ok |
| C-02 | `C02GeneratorNatural` | 61 | 61 | 61 | ok |
| C-02 | `C02GeneratorRaii` | 412 | 412 | 412 | ok |
| C-02 | `C02GeneratorYieldBreak` | 11 | 11 | 11 | ok |
| C-05 | `C05CaptureAcrossAwait` | 28 | 28 | 28 | ok |
| C-05 | `C05CaptureAcrossYield` | 128 | 128 | 128 | ok |
| C-05 | `C05MissingReturn` | rechazo: "not all reachable control paths return a value f…" | rechazado | rechazado | ok |
| C-05 | `C05EveryPathReturns` | 42 | 42 | 42 | ok |
| C-05 | `C05GlobalExtern` | rechazo: "an 'extern C' import outside a class is not supp…" | rechazado | rechazado | ok |
| C-05 | `C05GlobalVariable` | rechazo: "a variable outside every namespace is not suppor…" | rechazado | rechazado | ok |
| C-11 | `C11ScopeOrder` | 217 | 217 | 217 | ok |
| C-11 | `C11EarlyReturn` | 6763 | 6763 | 6763 | ok |
| C-11 | `C11Exception` | 5 | 5 | 5 | ok |
| C-11 | `C11ConditionalMove` | 56 | 56 | 56 | ok |
| C-11 | `C11ReturnAndAssign` | 1212 | 1212 | 1212 | ok |
| C-11 | `C11CopyRejected` | rechazo: "cannot be copied into local 'y'; use move(source…" | rechazado | rechazado | ok |
| C-11 | `C11StructEnumerator` | 15323 | 15323 | 15323 | ok |
| C-11 | `C11ObjectValueField` | 8 | 8 | 8 | ok |
| C-11 | `C11Temporaries` | 123456 | 123456 | 123456 | ok |
| C-12 | `C12RaiiStruct` | 21 | 21 | 21 | ok |
| C-12 | `C12Delete` | 21 | 21 | 21 | ok |
| C-12 | `C12OwnedField` | 21 | 21 | 21 | ok |
| C-12 | `C12AssignAndMove` | 10302 | 10302 | 10302 | ok |
| C-13 | `C13Call` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13New` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13This` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13Base` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13Wrong` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13Delegate` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13LambdaReject` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13Filtered` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13CatchBody` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13AwaitUncaught` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13PropGet` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13IndexSet` | rechazo: "which is neither caught here nor declared…" | rechazado | rechazado | ok |
| C-13 | `C13AsyncLocalThrow` | 10 | 10 | 10 | ok |
| C-13 | `C13AsyncAwaitCaught` | 13 | 13 | 13 | ok |
| C-13 | `C13AsyncSuspendedError` | 1107 | 1107 | 1107 | ok |
| C-13 | `C13Accepted` | 11111 | 11111 | 11111 | ok |
| C-13 | `C13Lambda` | 1011 | 1011 | 1011 | ok |
| C-14 | `C14Discard` | 4 | 4 | 4 | ok |
| C-14 | `C14Scope` | 3 | 3 | 3 | ok |
| C-14 | `C14Overwrite` | 212 | 212 | 212 | ok |
| C-14 | `C14Await` | 505 | 505 | 505 | ok |
| C-14 | `C14MovedFuture` | 808 | 808 | 808 | ok |
| C-14 | `C14UnusedFutureParameter` | 308 | 308 | 308 | ok |
| C-14 | `C14Hold` | 1009 | 1009 | 1009 | ok |
| C-14 | `C14Spawn` | 7 | 7 | 7 | ok |
| C-14 | `C14Copy` | rechazo: "cannot be copied into local 'g'…" | rechazado | rechazado | ok |
| C-14 | `C14DoubleAwait` | rechazo: "use of moved-from move-only local 'f'…" | rechazado | rechazado | ok |
| C-16 | `C16Cond` | 13\|d3d1\|d2d1\|d2d3d1 | 13\|d3d1\|d2d1\|d2d3d1 | 13\|d3d1\|d2d1\|d2d3d1 | ok |
| C-16 | `C16Shapes` | 0\|d1d2\|sd5\|d5\|kg7\|g7 | 0\|d1d2\|sd5\|d5\|kg7\|g7 | 0\|d1d2\|sd5\|d5\|kg7\|g7 | ok |
| C-16 | `C16Panic` | 142\|d2d1 | 142\|d2d1 | 142\|d2d1 | ok |
| C-16 | `C16PanicCond` | 142\|g3d1 | 142\|g3d1 | 142\|g3d1 | ok |
| C-16 | `C16PanicAsync` | 142\|d2d1 | 142\|d2d1 | 142\|d2d1 | ok |
| C-16 | `C16AsyncGenPanic` | 142\|d2d1 | 142\|d2d1 | 142\|d2d1 | ok |
| C-17 | `C17Catch` | 43721 | 43721 | 43721 | ok |
| C-17 | `C17First` | 53 | 53 | 53 | ok |
| C-17 | `C17After` | 2001 | 2001 | 2001 | ok |
| C-17 | `C17Finally` | 6281 | 6281 | 6281 | ok |
| C-17 | `C17Chain` | 91 | 91 | 91 | ok |
| C-17 | `C17AsyncMain` | 43721 | 43721 | 43721 | ok |
| C-17 | `C17SyncDrop` | 415 | 415 | 415 | ok |
| C-17 | `C17AsyncDrop` | 415 | 415 | 415 | ok |
| C-17 | `C17AsyncAwaitDrop` | 405 | 405 | 405 | ok |
| C-17 | `C17Rerun` | 1100\|sc1\|m\|ed1\|\| | 1100\|sc1\|m\|ed1\|\| | 1100\|sc1\|m\|ed1\|\| | ok |
| C-17 | `C17Capture` | 128 | 128 | 128 | ok |
| C-17 | `C17DropParked` | 111 | 111 | 111 | ok |
| C-17 | `C17CancelFresh` | 0 | 0 | 0 | ok |
| C-17 | `C17Cancel1` | 0 | 0 | 0 | ok |
| C-17 | `C17Cancel2` | 0 | 0 | 0 | ok |
| P-1 | `P1Throw` | rechazo: throw of | rechazado | rechazado | ok |
| P-1 | `P1ThrowAsync` | rechazo: throw of | rechazado | rechazado | ok |
| P-1 | `P1Rethrow` | rechazo: throw of | rechazado | rechazado | ok |
| P-1 | `P1Handled` | 789 | 789 | 789 | ok |
| P-3 | `P3OwnedArray` | 1243 | 1243 | 1243 | ok |
| P-3 | `P3OwnedArrayField` | 65 | 65 | 65 | ok |
| P-3 | `P3OwnedArrayFieldDelete` | 56 | 56 | 56 | ok |
| P-3 | `P3OwnedArrayDelete` | rechazo: explicit delete of a RAII local | rechazado | rechazado | ok |
| P-3 | `P3OwnedArrayFieldPlain` | 42 | 42 | 42 | ok |
| P-3 | `P3OwnedArrayFieldGeneric` | 21 | 21 | 21 | ok |
| P-4 | `P4Receiver` | 678 | 678 | 678 | ok |
| P-5 | `P5DllCalled` | rechazo: is not exported by 'kernel32.dll' | rechazado | rechazado | ok |

### Anexo B — sondas originales de cada punto

Una sonda escrita con una forma que el congelamiento prohibió (por ejemplo `T x = new T();` o la firma
vieja de `IAllocator`) ahora se rechaza con el diagnóstico nuevo; eso es el resultado correcto.

Tras los pendientes (`73b03a394`) se repitieron las 168 sondas en ambos niveles. Solo cambiaron nueve,
todas por diseño: las siete sondas C-01 que lanzan sin `throws` (`C01CatchOwnership`, `C01Rethrow`,
`C01Throw`, `C01ThrowBorrow`, `C01ThrowMove`, `C01ThrowNew`, `C01Wrap`) ahora se rechazan con "throw of
... is neither caught here nor declared" (P-1; sus pruebas unitarias equivalentes declaran `throws`), y
`C12OwnedStruct` y `C12Field` pasan de 0 a 21 porque el arreglo owned local y el campo owned ya se
liberan con sus elementos (P-3). La tabla conserva los resultados del cierre del plan.

| Punto | Sonda | -O0 -g | -O2 |
|---|---|---|---|
| C-08 | `c08align` | rechazo: concrete class 'c08align::OffsetAllocator' does not implement interfac… | rechazo: concrete class 'c08align::OffsetAllocator' does not implement interfac… |
| C-08 | `c08new` | exit 0 | exit 0 |
| C-08 | `c08arena` | exit 0 | exit 0 |
| C-08 | `T13F01AllocatorProtocol` | rechazo: the object created by 'new' reaches no owner through the non-owning lo… | rechazo: the object created by 'new' reaches no owner through the non-owning lo… |
| C-08 | `T13F02AllocatorSelection` | rechazo: the object created by 'new' reaches no owner through the non-owning lo… | rechazo: the object created by 'new' reaches no owner through the non-owning lo… |
| C-08 | `T13F04ArenaAllocator` | exit 0 | exit 0 |
| C-08 | `T13F07AllocatorProvenance` | rechazo: the object created by 'new' reaches no owner through the non-owning lo… | rechazo: the object created by 'new' reaches no owner through the non-owning lo… |
| C-06 | `T05AsyncAwaitRun` | exit 6 | exit 6 |
| C-06 | `T17F03AsyncCancel` | exit 7 | exit 7 |
| C-06 | `T06GeneratorEarlyStop` | exit 1 | exit 1 |
| C-06 | `C06Case` | rechazo: list patterns are supported in 'match' and 'is', not in 'case' labels;… | rechazo: list patterns are supported in 'match' and 'is', not in 'case' labels;… |
| C-06 | `C06Dim` | rechazo: the object created by 'new' reaches no owner through the non-owning lo… | rechazo: the object created by 'new' reaches no owner through the non-owning lo… |
| C-06 | `C06List` | exit 123 | exit 123 |
| C-06 | `C06Move` | exit 3 | exit 3 |
| C-07 | `C07Using` | exit 22 | exit 22 |
| C-07 | `C07UsingRaii` | rechazo: expected ')' after using resource declaration, found 'raii'… | rechazo: expected ')' after using resource declaration, found 'raii'… |
| C-01 | `C01Arg` | exit 1 | exit 1 |
| C-01 | `C01Ctor` | exit 0 | exit 0 |
| C-01 | `C01Leaks` | rechazo: the object created by 'new' reaches no owner through the non-owning lo… | rechazo: the object created by 'new' reaches no owner through the non-owning lo… |
| C-01 | `C01More` | exit 24 | exit 24 |
| C-01 | `C01Ok` | exit 0 | exit 0 |
| C-01 | `C01Op` | exit 80 | exit 80 |
| C-01 | `C01OwningForms` | exit 19 | exit 19 |
| C-01 | `C01CatchOwnership` | exit 0 | exit 0 |
| C-01 | `C01Rethrow` | exit 124 | exit 124 |
| C-01 | `C01Throw` | rechazo: throw hands the exception object to the catch that handles it, and tha… | rechazo: throw hands the exception object to the catch that handles it, and tha… |
| C-01 | `C01ThrowBorrow` | rechazo: throw hands the exception object to the catch that handles it, and tha… | rechazo: throw hands the exception object to the catch that handles it, and tha… |
| C-01 | `C01ThrowMove` | exit 11 | exit 11 |
| C-01 | `C01ThrowNew` | exit 11 | exit 11 |
| C-01 | `C01Wrap` | exit 245 | exit 245 |
| C-02 | `C02AsyncMain` | exit 6 | exit 6 |
| C-02 | `C02GenAsyncMain` | exit 7 | exit 7 |
| C-02 | `C02RefAsyncMain` | exit 42 | exit 42 |
| C-02 | `C02Suspend` | rechazo: synchronous await of a producer that can suspend: the producer must be… | rechazo: synchronous await of a producer that can suspend: the producer must be… |
| C-02 | `C02Transitive` | rechazo: synchronous await of a producer that can suspend: the producer must be… | rechazo: synchronous await of a producer that can suspend: the producer must be… |
| C-02 | `C02M1` | exit 55 | exit 55 |
| C-02 | `C02M2` | exit 55 | exit 55 |
| C-02 | `C02M3` | exit 2 | exit 2 |
| C-02 | `C02M4` | exit 46 | exit 46 |
| C-02 | `C02M5` | exit 43 | exit 43 |
| C-02 | `C02M6` | exit 1110 | exit 1110 |
| C-02 | `C02M7` | exit 42 | exit 42 |
| C-02 | `C02P1` | exit 42 | exit 42 |
| C-02 | `C02V1` | exit 42 | exit 42 |
| C-02 | `C02V2` | exit 42 | exit 42 |
| C-02 | `C02V3` | exit 42 | exit 42 |
| C-02 | `C02VarOk` | exit 5 | exit 5 |
| C-02 | `C02VarSync` | rechazo: synchronous await of a producer that can suspend: the producer must be… | rechazo: synchronous await of a producer that can suspend: the producer must be… |
| C-02 | `FC02AsyncMain` | exit 510 | exit 510 |
| C-02 | `FC02SynchronousProducers` | exit 56 | exit 56 |
| C-02 | `G1` | exit 1231 | exit 1231 |
| C-02 | `G2` | exit 12111 | exit 12111 |
| C-02 | `K1Loop` | exit 6 | exit 6 |
| C-02 | `K2Drop` | exit 142 | exit 142 |
| C-02 | `K3Borrow` | exit 10 | exit 10 |
| C-02 | `K4GenBreak` | exit 61 | exit 61 |
| C-02 | `K5TwoAwaits` | exit 112 | exit 112 |
| C-02 | `K6ParamLoop` | exit 33 | exit 33 |
| C-02 | `K7Nested` | exit 18 | exit 18 |
| C-02 | `K8SyncAwait` | exit 5 | exit 5 |
| C-02 | `K9GenNoStart` | exit 5 | exit 5 |
| C-02 | `K10GenNatural` | exit 61 | exit 61 |
| C-02 | `K11GenRaii` | exit 412 | exit 412 |
| C-02 | `K12GenYieldBreak` | exit 11 | exit 11 |
| C-02 | `S1NatFalse` | exit 0 | exit 0 |
| C-02 | `S2NatTrue` | exit 0 | exit 0 |
| C-02 | `S3CancelFirst` | exit 0 | exit 0 |
| C-02 | `S4CancelSecond` | exit 0 | exit 0 |
| C-02 | `S5AsyncMainGen` | exit 0 | exit 0 |
| C-05 | `C05CaptureAcrossAwait` | exit 28 | exit 28 |
| C-05 | `C05CaptureAcrossYield` | exit 128 | exit 128 |
| C-05 | `C05EveryPathReturns` | exit 42 | exit 42 |
| C-05 | `C05MissingReturn` | rechazo: not all reachable control paths return a value from function 'Pick'… | rechazo: not all reachable control paths return a value from function 'Pick'… |
| C-05 | `C05GlobalExtern` | rechazo: an 'extern C' import outside a class is not supported in NativeSharp 3… | rechazo: an 'extern C' import outside a class is not supported in NativeSharp 3… |
| C-05 | `C05GlobalVariable` | rechazo: a variable outside every namespace is not supported in NativeSharp 3.0… | rechazo: a variable outside every namespace is not supported in NativeSharp 3.0… |
| C-11 | `C11Owned` | exit 2 | exit 2 |
| C-11 | `C11Dtor` | exit 217 | exit 217 |
| C-11 | `C11Early` | exit 6763 | exit 6763 |
| C-11 | `C11EarlyReturn` | exit 6763 | exit 6763 |
| C-11 | `C11Throw` | exit 5 | exit 5 |
| C-11 | `C11Exception` | exit 5 | exit 5 |
| C-11 | `C11Copy` | rechazo: [NS3020]: move-only value of type 'type:C11Copy::Guard' cannot… | rechazo: [NS3020]: move-only value of type 'type:C11Copy::Guard' cannot b… |
| C-11 | `C11CopyRejected` | rechazo: [NS3020]: move-only value of type 'type:C11CopyRejected… | rechazo: [NS3020]: move-only value of type 'type:C11CopyRejected:… |
| C-11 | `C11Move` | exit 4 | exit 4 |
| C-11 | `C11MoveParam` | exit 93 | exit 93 |
| C-11 | `C11ConditionalMove` | exit 56 | exit 56 |
| C-11 | `C11Field` | exit 21 | exit 21 |
| C-11 | `C11FieldAssign` | exit 12034 | exit 12034 |
| C-11 | `C11ObjectValueField` | exit 8 | exit 8 |
| C-11 | `C11ClassField` | exit 8 | exit 8 |
| C-11 | `C11ArrayField` | exit 21 | exit 21 |
| C-11 | `C11Assign` | exit 12 | exit 12 |
| C-11 | `C11Return` | exit 33 | exit 33 |
| C-11 | `C11ReturnAndAssign` | exit 1212 | exit 1212 |
| C-11 | `C11ScopeOrder` | exit 217 | exit 217 |
| C-11 | `C11Temp` | exit 56757 | exit 56757 |
| C-11 | `C11TempCall` | exit 55 | exit 55 |
| C-11 | `C11Temporaries` | exit 0x1e240 | exit 0x1e240 |
| C-11 | `C11ClassTemp` | exit 55 | exit 55 |
| C-11 | `C11ClassTempCall` | rechazo: ownership-transfer argument requires an owned parameter… | rechazo: ownership-transfer argument requires an owned parameter… |
| C-11 | `C11ClassDiscard` | exit 6 | exit 6 |
| C-11 | `C11StringTemp` | exit 5 | exit 5 |
| C-11 | `C11Foreach` | exit 61 | exit 61 |
| C-11 | `C11ForeachBreak` | exit 322 | exit 322 |
| C-11 | `C11StructEnumerator` | exit 15323 | exit 15323 |
| C-11 | `C11Generic` | exit 8 | exit 8 |
| C-11 | `C11Shapes` | exit 0x1e240 | exit 0x1e240 |
| C-12 | `C12RaiiStruct` | exit 21 | exit 21 |
| C-12 | `C12RaiiClass` | exit 12 | exit 12 |
| C-12 | `C12RaiiDelete` | rechazo: explicit delete of a RAII local is forbidden; lexical cleanup owns des… | rechazo: explicit delete of a RAII local is forbidden; lexical cleanup owns des… |
| C-12 | `C12Delete` | exit 21 | exit 21 |
| C-12 | `C12OwnedStruct` | exit 0 | exit 0 |
| C-12 | `C12OwnedField` | exit 21 | exit 21 |
| C-12 | `C12Field` | exit 0 | exit 0 |
| C-12 | `C12Assign` | exit 12 | exit 12 |
| C-12 | `C12MoveOut` | exit 301 | exit 301 |
| C-12 | `C12List` | exit 12 | exit 12 |
| C-12 | `C12ListGrow` | exit 12345 | exit 12345 |
| C-12 | `C12ListRemove` | exit 22013 | exit 22013 |
| C-12 | `C12LinkedList` | rechazo: 'RemoveFirst' throws 'type:NativeSharpCore::NativeSharp.Core.InvalidOp… | rechazo: 'RemoveFirst' throws 'type:NativeSharpCore::NativeSharp.Core.InvalidOp… |
| C-13 | `TC13Call` | rechazo: 'Fail' throws 'type:TC13Call::MyError', which is neither caught here n… | rechazo: 'Fail' throws 'type:TC13Call::MyError', which is neither caught here n… |
| C-13 | `TC13New` | rechazo: constructor throws 'type:TC13New::MyError', which is neither caught he… | rechazo: constructor throws 'type:TC13New::MyError', which is neither caught he… |
| C-13 | `TC13This` | rechazo: constructor throws 'type:TC13This::MyError', which is neither caught h… | rechazo: constructor throws 'type:TC13This::MyError', which is neither caught h… |
| C-13 | `TC13Base` | rechazo: constructor throws 'type:TC13Base::MyError', which is neither caught h… | rechazo: constructor throws 'type:TC13Base::MyError', which is neither caught h… |
| C-13 | `TC13Delegate` | rechazo: 'm' throws 'type:TC13Delegate::MyError', which is neither caught here … | rechazo: 'm' throws 'type:TC13Delegate::MyError', which is neither caught here … |
| C-13 | `TC13PropGet` | rechazo: 'get_Value' throws 'type:TC13PropGet::MyError', which is neither caugh… | rechazo: 'get_Value' throws 'type:TC13PropGet::MyError', which is neither caugh… |
| C-13 | `TC13IndexSet` | rechazo: 'set_Item' throws 'type:TC13IndexSet::MyError', which is neither caugh… | rechazo: 'set_Item' throws 'type:TC13IndexSet::MyError', which is neither caugh… |
| C-13 | `TC13Filtered` | rechazo: 'Fail' throws 'type:TC13Filtered::MyError', which is neither caught he… | rechazo: 'Fail' throws 'type:TC13Filtered::MyError', which is neither caught he… |
| C-13 | `TC13CatchBody` | rechazo: 'Fail' throws 'type:TC13CatchBody::MyError', which is neither caught h… | rechazo: 'Fail' throws 'type:TC13CatchBody::MyError', which is neither caught h… |
| C-13 | `TC13LambdaReject` | rechazo: 'Fail' throws 'type:TC13LambdaReject::MyError', which is neither caugh… | rechazo: 'Fail' throws 'type:TC13LambdaReject::MyError', which is neither caugh… |
| C-13 | `TC13Wrong` | rechazo: 'Fail' throws 'type:TC13Wrong::MyError', which is neither caught here … | rechazo: 'Fail' throws 'type:TC13Wrong::MyError', which is neither caught here … |
| C-13 | `TC13AwaitUncaught` | rechazo: 'await' throws 'type:TC13AwaitUncaught::MyError', which is neither cau… | rechazo: 'await' throws 'type:TC13AwaitUncaught::MyError', which is neither cau… |
| C-13 | `TC13AsyncLocalThrow` | exit 10 | exit 10 |
| C-13 | `TC13AsyncAwaitCaught` | exit 13 | exit 13 |
| C-13 | `TC13AsyncSuspendedError` | exit 1107 | exit 1107 |
| C-13 | `TC13Accepted` | exit 11111 | exit 11111 |
| C-13 | `TC13Lambda` | exit 1011 | exit 1011 |
| C-14 | `TC14Discard` | exit 4 | exit 4 |
| C-14 | `TC14Scope` | exit 3 | exit 3 |
| C-14 | `TC14Overwrite` | exit 212 | exit 212 |
| C-14 | `TC14Await` | exit 505 | exit 505 |
| C-14 | `TC14MovedFuture` | exit 808 | exit 808 |
| C-14 | `TC14UnusedFutureParameter` | exit 308 | exit 308 |
| C-14 | `TC14Hold` | exit 1009 | exit 1009 |
| C-14 | `TC14Spawn` | exit 7 | exit 7 |
| C-14 | `TC14Copy` | rechazo: [NS3020]: move-only value of type 'future:builtin:int' cannot… | rechazo: [NS3020]: move-only value of type 'future:builtin:int' cannot b… |
| C-14 | `TC14DoubleAwait` | rechazo: [NS3020]: use of moved-from move-only local 'f'… | rechazo: [NS3020]: use of moved-from move-only local 'f'… |
| C-16 | `TC16Cond` | exit 13 "d3d1\|d2d1\|d2d3d1" | exit 13 "d3d1\|d2d1\|d2d3d1" |
| C-16 | `TC16Shapes` | exit 0 "d1d2\|sd5\|d5\|kg7\|g7" | exit 0 "d1d2\|sd5\|d5\|kg7\|g7" |
| C-16 | `TC16Panic` | exit 142 "d2d1" | exit 142 "d2d1" |
| C-16 | `TC16PanicCond` | exit 142 "g3d1" | exit 142 "g3d1" |
| C-16 | `TC16PanicAsync` | exit 142 "d2d1" | exit 142 "d2d1" |
| C-16 | `TC16AsyncGenPanic` | exit 142 "d2d1" | exit 142 "d2d1" |
| C-17 | `C17Catch` | exit 43721 | exit 43721 |
| C-17 | `C17First` | exit 53 | exit 53 |
| C-17 | `C17After` | exit 2001 | exit 2001 |
| C-17 | `C17Finally` | exit 6281 | exit 6281 |
| C-17 | `C17Chain` | exit 91 | exit 91 |
| C-17 | `C17AsyncMain` | exit 43721 | exit 43721 |
| C-17 | `C17SyncDrop` | exit 415 | exit 415 |
| C-17 | `C17AsyncDrop` | exit 415 | exit 415 |
| C-17 | `C17AsyncAwaitDrop` | exit 405 | exit 405 |
| C-17 | `C17Rerun` | exit 1100 "sc1\|m\|ed1\|\|" | exit 1100 "sc1\|m\|ed1\|\|" |
| C-17 | `C17Capture` | exit 128 | exit 128 |
| C-17 | `T9DropParked` | exit 111 | exit 111 |
| C-17 | `T9CancelFresh` | exit 0 | exit 0 |
| C-17 | `T9Cancel1` | exit 0 | exit 0 |
| C-17 | `T9Cancel2` | exit 0 | exit 0 |
