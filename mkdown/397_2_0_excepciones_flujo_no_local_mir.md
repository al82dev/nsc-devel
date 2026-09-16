# 397 — Excepciones y flujo no local en MIR / Exceptions and nonlocal control flow in MIR

El MIR OOP 1.10 contiene por función la topología de regiones protegidas, conteos de `catch` y
`finally`, la necesidad de personality SEH y un ledger de cada salida no local (`return`, `break`,
`continue`, `throw`, `rethrow` y continuación de cleanup). / OOP MIR 1.10 contains, per function,
the protected-region topology, `catch` and `finally` counts, the SEH-personality requirement, and
a ledger of every nonlocal exit (`return`, `break`, `continue`, `throw`, `rethrow`, and cleanup
continuation).

Cada salida conserva su ruta ordenada de cleanup y sus destinos de ámbito y excepción. MIR valida
que esos destinos existan; LLVM consume `requiresPersonality` y deja de recorrer el árbol para
inferir si una función necesita SEH. / Every exit retains its ordered cleanup path and scope and
exception targets. MIR validates those targets; LLVM consumes `requiresPersonality` instead of
walking the tree to infer whether a function needs SEH.
