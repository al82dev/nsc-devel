# 395 — Layout y ABI completamente en MIR / Layout and ABI fully represented in MIR

El MIR OOP 1.8 contiene el layout físico completo de structs y clases, incluidos alineación
mínima, tamaño/alineación de referencias y offsets finales. Cada struct lleva además su modo de
paso Win64 (`direct` o `indirect`). / OOP MIR 1.8 contains the complete physical layout of structs
and classes, including minimum alignment, reference size/alignment, and final offsets. Every
struct also carries its Win64 passing mode (`direct` or `indirect`).

Cada función congela en MIR el modo de retorno, la posición del `sret` oculto, el modo de cada
parámetro y su ordinal físico. LLVM consume ese contrato; ya no vuelve a decidir la ABI a partir
del tamaño del tipo ni recalcula dónde quedan `this`, `sret` y los parámetros declarados. / Every
function freezes its return mode, hidden `sret` position, each parameter's mode, and physical
ordinal in MIR. LLVM consumes that contract instead of deciding ABI from type size or recomputing
the placement of `this`, `sret`, and declared parameters.
