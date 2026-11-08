# Tanda 8: prueba de callback y llamada indirecta con bool C

El fixture compara el IR de Clang y NSC para una función externa con firma `(int, _Bool) -> _Bool`. El programa llama desde NativeSharp a un puntero de función de C y entrega a C un callback de NativeSharp. Esta entrega de callback es parte del consumo de la API C, no implementa una biblioteca NativeSharp consumible desde C++.

La línea base detecta que el thunk de NSC carece de `zeroext` en retorno y parámetro; se detiene antes de ejecutar O0–O3. Evidencia: `A:\nsc-interop-build\tanda08-bool-callback-c-15432-2365`. La corrección y las pruebas posteriores se registran por separado.
