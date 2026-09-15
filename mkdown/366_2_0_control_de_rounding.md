# 366 — Control de rounding / Rounding control

## Español

NativeSharp acepta `--fp-rounding=dynamic|nearest-even|toward-zero|upward|downward`.
`dynamic` preserva el entorno de ejecución y es el valor predeterminado de los modos normales;
`deterministic` fija `nearest-even` cuando no se especifica otra política. Las operaciones
aritméticas escalares con una política explícita se emiten mediante los intrínsecos
`llvm.experimental.constrained.*`, incluyendo los metadatos de redondeo y excepción estricta.

La prueba `nsc.item366.rounding-control` valida el contrato de nombres, el diagnóstico y que el IR
contenga operaciones restringidas con el modo solicitado.

## English

NativeSharp accepts `--fp-rounding=dynamic|nearest-even|toward-zero|upward|downward`.
`dynamic` preserves the execution environment and is the normal-mode default; `deterministic`
selects `nearest-even` unless another policy is explicit. Scalar arithmetic under an explicit
policy is emitted through `llvm.experimental.constrained.*` intrinsics with strict rounding and
exception metadata.

The `nsc.item366.rounding-control` test covers name mapping, diagnostics, and the constrained IR
contract.
