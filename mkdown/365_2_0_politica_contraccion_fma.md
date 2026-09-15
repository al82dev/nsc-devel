# 365 — Política de contracción FMA / FMA contraction policy

## Resultado / Result

`--fp-contract=off|on|fast` controla separadamente la contracción de multiplicación y suma. `off`
es el default estricto; `on` agrega `contract` sólo a operaciones contraíbles y `fast` acompaña al
modo rápido. / The option independently controls multiply/add contraction. Off is strict by
default, on marks only contractable operations, and fast accompanies the fast floating-point mode.

## Evidencia enfocada / Focused evidence

`nsc.item365.fma-contract-policy` compara IR con contracción apagada y encendida y valida errores de
opciones. / compares contraction-off and contraction-on IR and validates option diagnostics.
