# NS304-T05-F03 — Transformación productiva a state machine HIR/MIR

- **Estado**: ABIERTO (auditado; implementación pendiente)
- **Rama**: development

## Auditoría (línea base)

La vertical async NO está conectada productivamente:

- **Parser/AST** (F01, cerrado): `await` baja a `AwaitExpression`; `async` se consume sin `NS3049`.
- **Modelo de protocolo** (F02, cerrado): `FutureProtocol`/`PollOutcome`/`FutureState` en
  `Language/CoroutineSemantics.h`.
- **Plan builder**: `buildAsyncFunctionPlan` (`CoroutineSemantics.cpp:63`) reutiliza
  `SynchronousGeneratorPlan` (comparte `GeneratorLocal` para no bifurcar el ABI del frame con
  `yield`) y produce `AwaitSuspension`/`AsyncFunctionPlan`.
- **Hallazgo**: `buildAsyncFunctionPlan` NO se invoca desde `ProjectMethodBodyModel` ni
  `ProjectOopHirModel`/`ProjectOopMirModel`/`ProjectOopLlvmLowerer`. El lowering productivo de
  async (dividir por suspension points, estados de resume y terminales) no existe.

## Gaps F03

1. Sema: localizar los `AwaitExpression` de un método async, derivar `AwaitSuspension` por punto
   y construir el `AsyncFunctionPlan`.
2. HIR/MIR: transformar el cuerpo en state machine (entrada/resume/terminal) con aristas que
   conozcan locals vivos, drops y destino; no clonar el cuerpo en helpers desconectados.
3. LLVM: switch de estado + poll/resume/drop sin recursión ni allocation oculta.

## Plan

1. Cablear `buildAsyncFunctionPlan` en el análisis de cuerpos async (Sema).
2. Emitir el state machine en HIR/MIR (reutilizando la infraestructura de `SynchronousGeneratorPlan`).
3. Bajar a LLVM (switch de estados).

## Prueba puntual

Un programa async mínimo (`async int F() { await ...; return ...; }`) debe compilar, enlazar y
ejecutar con lazy start, pending→ready y cleanup.

## Nota

F03 es la primera transformación productiva desde cero de T05 (F01/F02 son modelo/AST). Requiere
vertical completa Sema→HIR→MIR→LLVM; no se introducen superficies desconectadas.

## Hallazgo adicional (2026-10-13)

El Sema **no tiene ninguna ruta para `AwaitExpression`** (`analyzeExpression` no lo contempla), y
el tipo de superficie `Future<T,E,C>` **no existe** (F02 es sólo modelo: `FutureProtocol` con claves
de tipo en string, no un tipo resoluble). Por tanto el operando de `await` no tiene tipo público que
resolver. La vertical F03 debe construir primero la superficie `Future` resoluble y después el
estado de la máquina; el plan builder (`buildAsyncFunctionPlan`) ya está unit-tested
(`TL-LANG-054..056`), pero nunca se invoca desde el pipeline productivo.

## Avance parcial (2026-10-13)

`analyzeExpressionCore` ahora reconoce `AwaitExpression` y diagnostica causalmente
`await requires the async state machine lowering (NS304-T05-F03)` (`TL-LANG-369`), reemplazando el
fallthrough silencioso que terminaba en el HIR con `unsupported Stage 34 OOP expression kind`.
Esto iguala el patrón de `yield` (T06-F03) y elimina un fallback silencioso. F03 sigue abierto:
falta la superficie `Future` + la state machine productiva.

## Auditoría de cierre (2026-10-13, segunda pasada)

Evidencia cruzada de capas (grep en `src/` e `include/`):

- **Plan builder sin consumidor productivo**: `buildAsyncFunctionPlan`, `awaitAllowed`,
  `validateFutureProtocol` y `FutureState` sólo se invocan desde tests
  (`tests/unit/NativeSharp3T10*`, `tests/compiler-suite/host/modules/LanguageCoroutinePlanningModule.cpp`).
  Ningún archivo de `src/Frontend` ni `src/LLVM` los llama.
- **Sin tipo de superficie `Future`**: no existe type-key `"Future"`/`"future:"`. Los type-keys son
  cadenas (`builtin:int`, `type:Ns::T`, `ReadOnlySpan<T>`, `Optional<T>`, …). El único `Future*` vive en
  `Language/CoroutineSemantics.h` como modelo de protocolo (F02), no como tipo resoluble.
- **Sin coroutine en HIR/MIR/LLVM**: `ProjectOopHirModel` (8.8k líneas), `ProjectOopMirModel` (3.4k) y
  `ProjectOopLlvmLowerer` (17.3k) no contienen suspend/resume/poll/estado/frame. La única
  infraestructura de state machine es el planificador semántico (`Language/CoroutineSemantics.*`,
  `Language/IterationSemantics.*`), unit-tested pero no cableado.
- **Marcador `async` sin leer en el member model**: el parser lo escribe como token `;async` en
  `auxiliaryText` (`Parser.cpp:2720` global, `Parser.cpp:4033-4035` método class/interface).
  `ProjectMemberModel.cpp:1852-1857` (declaredTypeKey) y `1980-1986` (effects) no lo consultan. Un
  método `async int F(){...}` hoy se tipa como `int F()` síncrono (semántica incorrecta: no Future lazy).
- **`await` diagnosticado causalmente**: `ProjectMethodBodyModel.cpp:9703-9711` (análogo a `yield` en
  5194-5201). Es el único punto productivo que toca `AwaitExpression`.

### Consecuencia de atomicidad

La vertical Sema→HIR→MIR→LLVM es atómica: cambiar sólo la superficie `future<T>` o sólo la resolución
de `await` en Sema, sin lowering, convierte un método `async` que hoy compila como síncrono en un
type-key `future:` no lowerable (regresión) y constituye una "superficie desconectada" (prohibida). No
existe un commit parcial seguro entre el diagnóstico causal (`4129ecc8`) y la vertical completa. F03 se
cierra con un único commit cuando `async int F(){ await …; return …; }` compile, enlace y ejecute
(lazy start, pending→ready, cleanup).

### Puntos de cableado exactos

1. `ProjectMemberModel.cpp:1852` — envolver `declaredTypeKey` en `future:<T>` cuando el marker
   `;async` está presente; `1980` — `effects.with(Effect::Async)`.
2. `ProjectMethodBodyModel.cpp:9703` — reemplazar el diagnóstico por resolución `future:<T> → T` +
   registro de `AwaitSuspension` + invocación de `buildAsyncFunctionPlan`.
3. HIR/MIR — estado de máquina (entrada/resume/terminal) reutilizando `SynchronousGeneratorPlan`
   (comparte `GeneratorLocal`).
4. LLVM — switch de estados + poll/resume/drop, sin recursión ni allocation oculta.
