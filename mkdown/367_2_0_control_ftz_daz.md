# 367 — Control FTZ y DAZ / FTZ and DAZ control

## Español

La opción `--fp-denormal=preserve|ftz|daz|ftz-daz` controla por separado los subnormales de
entrada y de resultado. El compilador traduce la política al atributo LLVM
`denormal_fpenv`: `ftz` afecta las salidas, `daz` las entradas y `ftz-daz` ambas. `preserve`
mantiene IEEE y continúa siendo el valor predeterminado estricto.

`nsc.item367.denormal-control` inspecciona las cuatro combinaciones en el IR.

## English

`--fp-denormal=preserve|ftz|daz|ftz-daz` independently controls input and result subnormals. The
compiler maps it to LLVM's `denormal_fpenv` function attribute: `ftz` affects outputs, `daz`
inputs, and `ftz-daz` both. `preserve` retains strict IEEE behavior and remains the default.

`nsc.item367.denormal-control` inspects the emitted IR contract.
