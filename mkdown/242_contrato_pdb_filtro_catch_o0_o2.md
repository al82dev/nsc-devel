# 242 — Contrato PDB del filtro catch en O0/O2 / Catch-filter PDB contract at O0/O2

## Resultado / Outcome

**ES.** El símbolo de procedimiento generado para el filtro `catch` es obligatorio en O0, donde el
callable físico existe. En O2 puede desaparecer si LLVM integra el filtro en el funclet; toda la
evidencia de EH, unwind, líneas, variables, tipos, bloques y rangos continúa siendo obligatoria.

**EN.** The generated `catch`-filter procedure symbol is mandatory at O0, where the physical
callable exists. At O2 it may disappear when LLVM folds the filter into its funclet; all EH,
unwind, line, variable, type, block, and range evidence remains mandatory.

## Causa / Cause

La verificación aplicaba una única conjunción a O0 y O2. El PDB optimizado preservaba la superficie
de depuración y unwind comprobada por el resto del script, pero no contenía un `S_LPROC32` para un
helper que ya no existía como procedimiento independiente. La ausencia se interpretaba como PDB
incompleto aunque describía honestamente el binario optimizado.

Verification applied one conjunction to both O0 and O2. The optimized PDB preserved the debug and
unwind surfaces checked by the rest of the script, but contained no `S_LPROC32` for a helper that
no longer existed as an independent procedure. Its absence was treated as incomplete debug data
even though it honestly described the optimized binary.

## Decisión / Decision

Sólo la expectativa del procedimiento optimizable se mueve al bloque O0. No se omite la función EH
contenedora ni se relaja ninguna comprobación general u optimizada. Si O0 pierde el filtro, el test
sigue fallando con un diagnóstico específico.

Only the optimizable procedure expectation moves into the O0 block. The containing EH function and
all general and optimized checks remain intact. If O0 loses the filter, the test still fails with a
specific diagnostic.

## Archivos / Files

- `cmake/VerifyNativeSharpDebugArtifacts.cmake`
- `mkdown/242_contrato_pdb_filtro_catch_o0_o2.md`

## Verificación dirigida / Targeted verification

Se ejecuta únicamente `nsc.debug.codeview-pdb`, que recorre sus perfiles line-only, O0 y O2. La
entrega termina con una reconstrucción limpia del compilador sin baseline.

Only `nsc.debug.codeview-pdb` is executed across its line-only, O0, and O2 profiles. The delivery
ends with a clean compiler rebuild and no baseline.

## Commit de referencia / Reference commit

**Título ES:** Distingue el filtro catch PDB entre O0 y O2

**Title EN:** Distinguish the PDB catch filter between O0 and O2

**Descripción ES:** Exige el procedimiento generado en O0 y permite su eliminación honesta en O2
sin relajar el resto del contrato de depuración extrema.

**Description EN:** Require the generated procedure at O0 and allow its honest removal at O2
without weakening the rest of the extreme-debug contract.
