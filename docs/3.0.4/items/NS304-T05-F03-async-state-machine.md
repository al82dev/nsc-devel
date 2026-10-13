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
