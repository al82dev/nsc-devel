# NS304-T06-F03 — Transformar cuerpo en frame y state machine MIR verificable

- **Estado**: AUDITADO (pendiente: lowering del cuerpo generator a HIR/MIR/LLVM; el modelo ya existe)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Transformar cuerpo en frame y state machine MIR verificable*.
Plan T06-F03 (DeepSeekDocs): *crear estados por punto de yield, entrada de resume/move-next y
terminal; el valor producido permanece válido según su modo hasta la siguiente demanda; verificar
CFG, estado terminal y ausencia de materialización intermedia*.

Estado real:

- **Semántica** (F02, cerrado): `Iterator<T>` resoluble, `Effect::Generator` marcado, item unificado
  y `return v` rechazado. El yield en un generator aún rechaza con `generator state machine lowering
  is not yet implemented (NS304-T06-F03)` en `ProjectMethodBodyModel.cpp`.
- **Modelo disponible pero NO cableado**: `IterationSemantics.h` (`SynchronousGeneratorPlan`,
  `buildSynchronousGeneratorPlan`, `GeneratorSuspension`, `GeneratorFrameSlot`, `generatorResumeState`,
  `generatorCancellationDropOrder`) está unit-tested y es el contrato del frame/state machine. Igual
  que el `AsyncFunctionPlan` de T05 antes de cablearlo: sin consumidor productivo.
- **HIR/MIR**: no hay nodos `YieldReturn`/`MoveNext`/`Current` en `ProjectOopHirModel`/`ProjectOopMirModel`
  (confirmado por grep). El lowering productivo no existe.
- **LLVM**: no hay emisión del iterator ABI.
- **Consumidor**: `foreach` productivo no existe (eso es F08). El protocolo iterator (MoveNext/Current)
  es la surface a congelar en F08.

## Gap

El cuerpo de un generator no se transforma en una state machine: no hay entrada de resume/MoveNext, ni
estados por yield, ni terminal; el valor producido no se materializa ni se retiene entre demandas. El
`buildSynchronousGeneratorPlan` está listo como contrato pero ningún productor (Sema→HIR) lo invoca.

## Dependencia material (F03 ↔ F08)

El generator no es observable sin un consumidor. El `foreach` (F08) es el consumidor canónico, pero F08
congela el ABI (MoveNext/Current/cleanup). Para demostrar F03 en O0 sin adelantar F08, hace falta una
**primitiva de consumo mínima** (p.ej. un par `MoveNext()/Current` emitido por el propio generator y
ejercido desde un `Main` de prueba), análogo al blocking `await` que F03 usó para demostrar el poll async
antes de que existiera `await` sobre variable.

## Opciones de surface (material, para Alberto)

- **(A) Iterator con `MoveNext()/Current`** (recomendada): el generator baja a un struct opaco con
  `bool MoveNext()` + `T Current` (+ `drop`), consumible por un `foreach` de F08 o por llamadas
  directas en la prueba puntual. Consistente con el `Future<T>`/poll de T05.
- **(B) Iterator por-función / NSMETA-only**: diferir todo a F08; F03 no tendría prueba O0 observable
  sin tocar F08.

Recomendación: **(A)** — es la forma canónica del iterator opaco y desbloquea una prueba O0 de F03 sin
congelar el ABI completo (eso queda en F08).

## Plan (tras fijar la surface)

1. Bajar `yield return v` → HIR `YieldValue` (guarda el item en el frame + `state = N` + `return`);
   `yield break` → `state = completed` + `return`.
2. `splitGeneratorFunction`: construir el constructor (devuelve el iterator con el frame caller-alloc)
   y `MoveNext` (reanuda por estado), consumiendo `buildSynchronousGeneratorPlan`.
3. Emitir el iterator ABI mínimo (MoveNext/Current) y bajar a LLVM O0.
4. Prueba puntual: `Iterator<int> Gen(){ yield return 1; yield return 2; }` + consumo directo en `Main`
   (sin `foreach`) → secuencia 1,2 y terminal.

## Nota

F03 hereda el caller-stack/liveness del marco async (T05-F04) y reutiliza `IterationSemantics`; el
consumo intermodular y el `foreach` son F08.
