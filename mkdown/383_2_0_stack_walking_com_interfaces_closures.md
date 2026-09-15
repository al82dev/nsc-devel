# 383 — Stack walking de COM, interfaces y closures / COM, interface, and closure stack walking

## Español

El corpus O0 amplía la medición física de pila a tres formas de dispatch. La ruta COM construye una
vtable cruda, publica un método NativeSharp como `extern C delegate`, recupera el slot tipado y lo
invoca con `self`; la captura ocurre dentro del destino después de atravesar el thunk de contención.
La ruta de interfaz pasa una clase por una vista prestada y captura desde la implementación elegida
por el mapa de interfaces. La ruta de closure materializa un contexto que contiene el baseline y
captura desde el cuerpo lambda.

Cada ruta compara `RtlCaptureStackBackTrace` contra una medición tomada por el consumidor antes del
dispatch. El resultado ejecutable permanece en 42 únicamente si COM, interfaz y closure añaden
frames recorribles. El PDB exige los procedimientos autores, el thunk COM como `S_THUNK32`, el
método de interfaz como procedimiento normal —nunca como thunk inventado—, el lambda, su destructor
de contexto artificial y los locales relevantes.

La misma inspección de `.pdata` y `UnwindInfo` del ítem 382 cubre el ejecutable final que contiene
estas rutas. Esto vincula el comportamiento del stack walker con las identidades que Visual Studio
y WinDbg mostrarán.

## English

The O0 corpus extends physical stack measurement to three dispatch forms. The COM path builds a raw
vtable, publishes a NativeSharp method as an `extern C delegate`, recovers the typed slot, and
invokes it with `self`; capture happens inside the target after crossing the containment thunk. The
interface path passes a class through a borrowed view and captures from the implementation selected
by the interface map. The closure path materializes a context holding the baseline and captures from
the lambda body.

Every path compares `RtlCaptureStackBackTrace` with a measurement taken by the consumer before
dispatch. The executable remains at 42 only when COM, interface, and closure add walkable frames.
The PDB requires authored procedures, the COM thunk as `S_THUNK32`, the interface method as a normal
procedure—never an invented thunk—the lambda, its artificial context destructor, and the relevant
locals.

The same `.pdata` and `UnwindInfo` inspection introduced by item 382 covers the final executable
containing these paths. This ties stack-walker behavior to the identities Visual Studio and WinDbg
will display.
