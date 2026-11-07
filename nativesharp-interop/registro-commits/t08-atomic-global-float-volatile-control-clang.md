# T08: comparación reproducible del IR volatile con Clang

La sonda C observó cargas acquire y escrituras release de float y long C
volatile en el modo MS. El validador agrupado ahora compila esa sonda y
comprueba su IR antes del IR de NSC. El control actualizado aún no se ha
ejecutado; no acredita la corrección propuesta ni cierra T08-004.
