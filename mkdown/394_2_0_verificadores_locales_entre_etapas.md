# 394 — Verificadores locales entre etapas / Local stage-boundary verifiers

El pipeline frontal aplica el verificador que pertenece al productor justo antes de entregar su
salida al consumidor siguiente. Las seis fronteras HIR→MIR, MIR→CFG, CFG→dominancia,
dominancia→SSA, SSA→optimización y optimización→backend quedan ordenadas, registradas y son
obligatorias también en compilaciones normales. / The frontend pipeline applies the verifier owned
by each producer immediately before handing output to its next consumer. The six HIR→MIR,
MIR→CFG, CFG→dominance, dominance→SSA, SSA→optimization, and optimization→backend boundaries are
ordered, recorded, and mandatory during normal compilation as well.

Un rechazo informa tanto la frontera exacta como el invariante local que falló; esto evita que un
artefacto inválido avance y que el diagnóstico aparezca varias etapas después. / A rejection names
both the exact boundary and the failed local invariant, preventing invalid artifacts from advancing
and diagnostics from surfacing several stages later.
