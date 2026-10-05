# NS3-BASELINE-SELFHOST-RESERVED-ALIASES

- Reporte: `20261004-211739-4bfcc1c2`.
- Resultado observado: 71 fallos registrados; 70 compartían una causa.
- Causa: `LlvmBackend.ns` usaba `i8`, `i32` e `i64` como identificadores aunque NativeSharp 3 ya los reserva como alias de tipo.
- Corrección: migración interna a `int8Type`, `int32Type` e `int64Type`, sin alterar los alias públicos ni la semántica LLVM.
- Commit funcional: `80e4e959a`.
- Validación: cero identificadores incompatibles, sintaxis 98/98 y emisión O0 con NSLink correcta en 2.73 s.
- Baseline: no ejecutado.
- Documento completo: `docs/3.0/items/NS3-BASELINE-SELFHOST-RESERVED-ALIASES.md`.
