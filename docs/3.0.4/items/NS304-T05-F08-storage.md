# NS304-T05-F08 — Storage en stack, caller, arena o allocator explícito; nunca heap obligatorio

- **Estado**: ABIERTO (auditado; caller-stack hecho, arena/allocator no cableado)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *permitir storage en stack, caller, arena o allocator explícito; nunca heap obligatorio*.

Modelo (`include/nativesharp/Language/CoroutineSemantics.h`):

- `enum class FrameStorage { CallerStack, CallerArena, ExplicitHeap }` (línea 20).
- `AsyncFunctionPlan.storage = FrameStorage::CallerStack` (default, línea 58).
- `asyncFrameHasImplicitHeap(FrameStorage)` devuelve **siempre `false`** (`CoroutineSemantics.cpp:12`):
  ningún storage promueve a heap implícito. La selección es explícita del caller/runtime.
- `buildAsyncFunctionPlan` mapea `CallerStack→Stack`, `CallerArena→Arena`, `ExplicitHeap→ExplicitHeap`
  (`generatorStorage`, `CoroutineSemantics.cpp:20-29`) y escribe el storage en el identity hash
  (`asyncFutureIdentity`, línea 47-52). **Unit-tested pero NO cableado productivamente** (mismo
  estado que F03 documentó para el plan builder).

Estado real del lowering productivo (tras F03/F04):

- **Caller-stack**: HECHO. El `await` (sync y async) aloca el frame con `entryBlockAlloca`
  (`ProjectOopLlvmLowerer.cpp:9086/9160`, un `alloca` de stack) y lo guarda en `future.frame@8`.
  El tamaño por-función es `asyncFrameSize`. Es el único storage que existe en el lowering.
- **CallerArena**: NO CABLEADO. El modelo tiene el valor y el mapeo a `GeneratorStorage::Arena`,
  pero el lowering no emite ninguna alocación de arena; no hay ruta de selección.
- **ExplicitHeap / allocator explícito**: NO CABLEADO. Mismo estado que CallerArena: el lowering no
  consulta `FrameStorage` en ningún punto (el frame es siempre `alloca`).
- **Nunca heap obligatorio**: SATISFECHO. `asyncFrameHasImplicitHeap` es `false` y el default es
  caller-stack; no hay heap implícito.

## Gap

El lowering está **hardcodeado a caller-stack**: no existe selección de storage (no hay parámetro,
atributo ni surface de usuario que elija `CallerArena`/`ExplicitHeap`), y por tanto no hay ruta que
emita una alocación de arena o de allocator explícito. El contrato pide "permitir storage en …
arena o allocator explícito", y eso hoy es **no demostrable**: no hay superficie de usuario para
ejercerlo.

## Plan

1. Definir la superficie de selección de storage (p.ej. atributo `[FrameStorage(Arena)]`/`[FrameStorage(Heap)]`
   o parámetro del constructor del future) — decisión de surface a fijar con Alberto.
2. Cablear `buildAsyncFunctionPlan.storage` (hoy no consumido) hacia el lowering: el `AwaitFuture`
   aloca el frame según el storage elegido (stack = `alloca`; arena/allocator = llamada al runtime
   con el allocator/arena correspondiente), sin heap implícito.
3. Mantener el default caller-stack y el invariante `asyncFrameHasImplicitHeap == false`.

## Prueba puntual

Un async con storage caller-stack (default) debe seguir sin heap; un async con storage arena/allocator
explícito debe alocar el frame en ese storage y no en el stack del caller. (La selección explícita no
tiene surface de usuario hoy, así que el positivo requiere definirla primero.)

## Nota

F08 hereda el caller-stack de F03/F04 (ya verificado). El resto (arena/allocator) es una ampliación
de surface no cableada, independiente de la primitiva pendiente de F09.
