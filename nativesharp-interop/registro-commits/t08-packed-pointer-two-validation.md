# Validación dirigida: puntero empaquetado y pack(2)

La construcción incremental de NSC sobre `ec926675f` terminó con código 0.
El fixture layout pasó O0 y, tras `e6f4cb130`, volvió a pasar O0–O3 con sus
comprobaciones IR y casos negativos. Incluye campo puntero `pack(1)` y
campos short, int y double `pack(2)`. No sustituye la batería de la tanda.
