# 364 — Modos de punto flotante / Floating-point modes

## Resultado / Result

`nsc` acepta `--fp-mode=precise|fast|deterministic`. `precise` continúa como valor predeterminado;
`fast` agrega el conjunto LLVM `fast` a operaciones escalares y SIMD, mientras `deterministic`
mantiene semántica estricta como base para los siguientes controles. / `nsc` now accepts three
explicit modes. Precise remains the default, fast opts into LLVM fast-math, and deterministic stays
strict as the basis for the following controls.

## Evidencia enfocada / Focused evidence

`nsc.item364.floating-point-modes` compara el IR de los tres modos y valida el diagnóstico del modo.
/ compares the IR produced by all three modes and validates the reported mode.
