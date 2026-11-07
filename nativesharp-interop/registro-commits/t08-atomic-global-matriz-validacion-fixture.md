# T08: control agrupado de globales C atómicos no TLS

Se prepara una matriz de lecturas y escrituras cruzadas con C en O0–O3 y una
comprobación independiente del IR producido por Clang y NSC para cada global,
exigiendo lecturas y escrituras atómicas `seq_cst`. El fixture no se ha
ejecutado todavía; no acredita la corrección ni cubre operaciones compuestas,
bibliotecas precompiladas o DLL.
