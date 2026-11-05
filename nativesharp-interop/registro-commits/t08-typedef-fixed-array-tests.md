# Tanda 08: pruebas preparadas de arrays de typedef

La prueba de sesión conserva las dimensiones de `Matrix[2][3]` en orden C y
rechaza un límite incompleto. La prueba integrada comprueba que la expansión en
`TypeSyntax` almacena las dimensiones en el orden interno correspondiente.
Ambas esperan la validación agrupada de la tanda y no demuestran el ABI de paso.
