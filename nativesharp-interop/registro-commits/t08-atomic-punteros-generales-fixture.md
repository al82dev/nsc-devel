# T08-004: controles para punteros C atómicos generales

La matriz existente incorpora punteros atómicos a un struct C y a `_Bool`,
uno global y otro TLS. El control C y NativeSharp actualizan los punteros y
una función C verifica que observan el mismo almacenamiento. Se probarán en
las variantes de referencia estática y DLL en O0–O3. Esta preparación no
afirma que NSC ya los soporte ni cubre todas las operaciones posibles.
