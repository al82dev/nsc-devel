# NS304-T06-F04 — Locals vivos, captures, ownership y drop flags por suspensión

- **Estado**: PARCIAL conservativo (frame + save/restore de locals cerrados; captures/ownership/drop flags + liveness precisa pendientes)
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

### Cierre parcial (save/restore) — verificado 2026-10-13

- MIR: el frame del MoveNext = los locals declarados (conservativo), reutilizando `asyncFrameLocals`.
- `YieldValue` LLVM: entry asigna el frame una vez (null check + PHI) + guarda los locals vivos;
  resume los restaura.
- Verificado: `int x=10; yield return x; yield return x+1;` → `MoveNext` produce 10, 11 (exit 21).
  Registrado `TL-LANG-382` (PASS). Commit `8206f68b`.

Queda (cola): captures/parámetros con ownership, drop flags por inicialización y liveness precisa
def-use (hoy conserva todos los locals declarados).

### Cierre (parámetros + liveness precisa) — verificado 2026-10-14

- **Liveness precisa def-use**: el frame del MoveNext ahora sólo guarda los locals vivos-entre-yields
  (def-use), omitiendo los muertos (análogo a T05-F04).
- **Captura de parámetros**: el constructor del generador aloca el frame (heap) y guarda los
  parámetros originales; el `$moveNext` los lee del frame (nodo `GeneratorFrameInit` tras el
  `self = default`, que antes los pisaba). El parámetro sobrevive al yield.
- Verificado: `Iterator<int> Generate(int x){ int dead=100; yield return x; yield return x+1; }`
  → 10, 11 (exit 21). Registrado `TL-LANG-439` (PASS). Guards `TL-LANG-382/383` PASS.

Queda (cola): drop flags por inicialización (sólo dropear storage inicializado en early stop) — es
el camino de destrucción de F06-F07.
