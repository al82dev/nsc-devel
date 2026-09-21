# Commit Tanda 47 / SH45

Estado: `STAGED-COMMITTED`; el commit vive sólo en `t47-staging` hasta cerrar T46/SH44.

Título previsto: `selfhost(tanda-47): close NativeSharp 2.0 low-level parity`

Incluye parser, semántica, símbolos, backend LLVM, bindings LLVM C, contrato, fixtures, gate CMake,
verificador, expediente y resumen de evidencia para volatile, atomics, unions, layout, bitfields,
placement/destrucción, noheap y escape analysis. No incluye `selfhost/out/`, `build-sh45/`, objetos,
ejecutables, bibliotecas, `.rsp`, logs ni temporales.

Gate dirigido: SH41 O0–O3 LLD/NSLink con punto fijo 4/4; SH45 160/160 ejecuciones, 64/64 rechazos,
46/46 pruebas de infraestructura y gate CMake aprobado. Baseline aislado
`tanda-47-sh45-provisional-r3-20260921`: 479/479 CTests y 166/166 fixtures por linker, triage y
Gate 0 aprobados, cero fallos, 3866.614 s.

Antes de integrar en `development-claude`:

1. cerrar T46 con la atestación física;
2. integrar T47 sobre el commit T46;
3. ejecutar el baseline completo con ambos linkers;
4. actualizar expediente y evidencia de `STAGED-COMMITTED` a `LANDED` con corrida y tiempos reales;
5. enmendar este único commit y comprobar `git diff --check`.
