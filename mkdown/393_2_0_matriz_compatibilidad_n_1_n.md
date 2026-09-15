# 393 — Matriz N-1/N de compatibilidad / N-1 and N compatibility matrix

El compilador publica una matriz canónica `NSCOMPAT-MATRIX 1` para compiler release, NSMETA, ABI
nativa y ABI de runtime. En cada superficie, el consumidor N certifica productores N-1 y N; un
consumidor N-1 no acepta artefactos N. Las identidades concretas se derivan de las constantes del
toolchain, no se duplican a mano. / The compiler publishes a canonical `NSCOMPAT-MATRIX 1` covering
compiler release, NSMETA, native ABI, and runtime ABI. On every surface, consumer N certifies N-1
and N producers; an N-1 consumer does not accept N artifacts. Concrete identities derive from the
toolchain constants rather than being duplicated.

La matriz puede imprimirse, escribirse y verificarse; cualquier mutación invalida la certificación.
/ The matrix can be printed, written, and verified; any mutation invalidates certification.
