# NS304-T06-F04 — Locals vivos, captures, ownership y drop flags por suspensión

- **Estado**: AUDITADO (pendiente: frame de locals + save/restore en el YieldValue)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Calcular locals vivos, captures, ownership y drop flags por suspensión*.
Plan T06-F04 (DeepSeekDocs): *persistir sólo locals vivos entre yields, con flags de inicialización;
captures y parámetros conservan ownership; early destruction recorre sólo storage inicializado en
orden inverso*.

Estado real:

- **Frame del iterator**: el layout `iterator:<T>` = `{state, frame, item, moveNextFn}` ya tiene el
  puntero `frame@8`, pero **nadie lo asigna** para los locals: el `YieldValue` (LLVM) guarda item +
  state + `return true`, y **no persiste ningún local**. El estado (discriminante) sí persiste, pero
  los locals de stack mueren al salir del MoveNext.
- **MIR**: no hay `generatorFrameLocals`/`generatorFrameSize` (sólo existen `asyncFrameLocals`/
  `asyncFrameSize` para el poll async, `ProjectOopMirModel.h:820-822`).
- **Liveness**: no hay cálculo de qué locals cruzan un `yield return`. El modelo `IterationSemantics`
  (`GeneratorLocal.livesAcrossYield`, `GeneratorFrame`, `buildSynchronousGeneratorPlan`) está
  unit-tested pero NO cableado al lowering.

## Gap

Un local vivo a través de un `yield return` se pierde (stack muerto al retornar el MoveNext). No hay
frame de locals ni save/restore en el `YieldValue`; captures/parámetros y drop flags tampoco se
persisten.

## Plan (reutilizando el marco async T05-F04)

1. Calcular en el MIR los locals vivos-entre-yields (`generatorFrameSize` + `generatorFrameLocals`
   = {localOrdinal, offset, typeKey}), análogo a `asyncFrameLocals`.
2. Asignar el frame en el `IteratorGetEnumerator`/`MoveNext` (caller-alloc) y guardarlo en
   `frame@8`.
3. En el `YieldValue` (LLVM): save de los locals vivos en el frame (camino entry) + restore en el
   camino resume, análogo al `SuspendPoint` async.

## Prueba puntual (pendiente)

`Iterator<int> Gen(){ int x = 10; yield return x; yield return x + 1; }` + consumo → 10, 11
(el local `x` debe persistir entre yields). Actualmente el segundo yield leería stack muerto.
