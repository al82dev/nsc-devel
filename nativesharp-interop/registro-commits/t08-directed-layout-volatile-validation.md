# Validación dirigida de layout y volatile

La construcción incremental sobre `99716724c` y el caso layout terminaron con
código 0. Record C rechazó coalescencia; `831f998d3` corrigió el análisis.
Tras recompilar incrementalmente, el IR tenía el orden esperado pero el
verificador rechazó etiquetas numeradas con punto; `db2a2a352` lo corrigió.
La repetición de record C terminó con código 0 en O0 a O3, IR y negativos.
No se ejecutó la batería completa ni se cerró la tanda 08.
