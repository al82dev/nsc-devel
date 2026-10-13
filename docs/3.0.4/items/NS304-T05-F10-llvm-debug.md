# NS304-T05-F10 — Emitir LLVM y debug metadata de state, locals, sequence points y logical frames

- **Estado**: PARCIAL (identidad del poll distinguida; state@0 y logical frames pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *emitir LLVM y debug metadata de state, locals, sequence points y logical frames*.

Estado real tras F03 (cerrado). Evidencia en `src/LLVM/ProjectOopLlvmLowerer.cpp`:

- **Subprogram**: el constructor (`F`) y el poll (`F$poll`) reciben subprogram via
  `createDebugSubprogram` (línea ~3370) sólo con `-g` (`debugBuilder_ != nullptr`). PERO para un
  método de clase el nombre de debug se deriva de `member.name` (línea ~923-925), NO de
  `debugDisplayName`. Como el poll comparte `memberOrdinal` con el constructor (F03:
  `poll->memberOrdinal = function.memberOrdinal`), **ambos** quedan con el display name
  `FQN.F` — la identidad `$poll` se pierde y colisiona con el constructor. (Para una función de
  namespace sí se usa `debugDisplayName` → `$poll`; la inconsistencia es sólo en métodos.)
- **State (discriminante)**: el campo `state@0` del future no tiene metadata de debug; no se emite
  como variable ni como `DIFlagArtificial`. Un depurador no puede ver el estado 0/-1/1..N.
- **Locals**: SÍ, heredados de la infraestructura general (`emitDebugLocal`, línea ~2039): los
  locals del cuerpo async (que viven en el poll) se emiten con `localOrdinal` + rango fuente.
- **Sequence points**: SÍ, heredados (`debugLocation`, línea ~1094): las instrucciones del cuerpo
  del poll llevan `DILocation` con el rango fuente original (el return-wrap reescribe pero conserva
  `syntax`).
- **Logical frames**: NO. El poll es una función LLVM plana, no un "logical frame" hijo del
  constructor; no hay `DILexicalBlock`/`DISubprogram` anidado que modele "F → F$poll (await)".

## Gap demostrable

Con `-g` (PDB) se observa: el poll aparece con el **mismo nombre que el constructor** (colisión
`F`/`F`), y el **estado del future no es inspeccionable**. Es demostrable con un dump del
subprogram (o inspección PDB) de `async int F(){ int x = await G(); return x+1; }` compilado con
`-g`: dos `DISubprogram` de nombre `Program.F` (constructor y poll) y sin variable `state`.

## Plan

1. En `createDebugSubprogram` (camino de miembro), usar `function.debugDisplayName` (que ya es
   `F$poll` para el poll) cuando `isAsyncPoll`/`$poll` esté presente, en vez de `member.name`.
2. Emitir el campo `state@0` como variable de debug (offset 0, `DIFlagArtificial`) en el
   subprogram del poll, para que el depurador muestre 0/-1/1..N.
3. (Opcional, "logical frames") modelar el poll como `DILexicalBlock` anidado del constructor, o
   al menos marcarlo `isAsyncPoll` en el `DISubprogram` para distinguirlo del constructor.

## Prueba puntual

`async int F(){ int x = await G(); return x+1; }` compilado con `-g` debe emitir DOS subprograms
distinguibles (`F` y `F$poll`) y una variable `state` en el poll; los sequence points y locals ya
deben persistir.

## Nota

F10 es independiente de la suspensión runtime (F09): la metadata de debug se emite con `-g` aunque
el poll complete sincrónicamente. No depende de la primitiva pendiente.

### Cierre parcial (identidad del poll) — verificado 2026-10-13

`createDebugSubprogram` (camino de miembro) ahora apende `$poll` al display name cuando
`mangledName` termina en `$poll`. Verificado: `--emit-project-oop-exe -g` produce `probe.pdb` que
contiene `Program.F$poll` (poll) distinto de `Program.F` (constructor). Commit `4f3f70fd`.

Queda: emitir `state@0` como variable de debug (offset 0, `DIFlagArtificial`) y "logical frames"
(opcional).
