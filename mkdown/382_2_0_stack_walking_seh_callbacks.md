# 382 — Stack walking de SEH y callbacks / SEH and callback stack walking

## Español

El corpus de depuración O0 ejecuta ahora recorridos físicos de pila dentro de dos caminos que
requieren metadatos Win64 completos. El primero lanza una excepción NativeSharp desde la biblioteca
productora, entra a un `catch` tipado y llama allí a `Debug.CaptureStackFrameCount()`. El segundo
convierte un método a `extern C delegate`, atraviesa su thunk de contención de excepciones y captura
la pila desde el destino. Ambos conteos deben superar al capturado por el consumidor antes de entrar
a la biblioteca.

La llamada de diagnóstico termina en `RtlCaptureStackBackTrace`, por lo que la comprobación recorre
las tablas reales del ejecutable y no una pila lógica inventada por el compilador. La inspección del
PDB exige además los procedimientos de origen, el callback publicado como `S_THUNK32`, el filtro de
catch artificial y los locales observables dentro de ambos caminos.

Finalmente, `llvm-readobj` certifica que el PE enlazado conserva `.pdata`, entradas
`RuntimeFunction`, sus payloads `UnwindInfo` (la información `.xdata` que LLD integra en `.rdata`)
y un handler SEH. En conjunto, la ejecución, CodeView/PDB y unwind describen el mismo recorrido
físico a través de excepciones y callbacks.

## English

The O0 debug corpus now performs physical stack walks inside two paths that require complete Win64
metadata. The first throws a NativeSharp exception from the producer library, enters a typed
`catch`, and calls `Debug.CaptureStackFrameCount()` there. The second converts a method to an
`extern C delegate`, crosses its exception-containment thunk, and captures the stack from the
target. Both counts must exceed the one captured by the consumer before entering the library.

The diagnostic call ultimately reaches `RtlCaptureStackBackTrace`, so the check walks the real
executable tables rather than a compiler-invented logical stack. PDB inspection also requires the
authored procedures, the callback published as `S_THUNK32`, the artificial catch filter, and the
observable locals in both paths.

Finally, `llvm-readobj` certifies that the linked PE retains `.pdata`, `RuntimeFunction` entries,
their `UnwindInfo` payloads (the `.xdata` information LLD folds into `.rdata`), and an SEH handler.
Execution, CodeView/PDB, and unwind therefore describe the same physical walk through exceptions
and callbacks.
