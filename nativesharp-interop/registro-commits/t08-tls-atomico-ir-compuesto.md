# Actualización atómica preparada

La ruta LLVM preparada para un global C atómico `int` usa `atomicrmw add`
`seq_cst` para suma o resta, y devuelve el valor anterior o actualizado según el
nodo. Otras actualizaciones se rechazan en esa ruta. No se habilita aún la
proyección, y faltan pruebas de IR y operaciones adicionales.
