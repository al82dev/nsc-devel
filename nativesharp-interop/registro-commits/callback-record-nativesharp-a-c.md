# Callback NativeSharp con puntero a record invocado por C

Se preparó un callback NativeSharp sin capturas `int(CallbackPayload*)`, que
una función C recibe y llama. El mismo callback se asigna a un campo y a un
global C, y otras funciones C los invocan. El fixture comprueba el resultado y
está incluido en la matriz agrupada de callbacks O0–O3.

El fixture no cubre
capturas ni callbacks conservados después del tiempo de vida comprobado.

Con NSC build `3f82235fd35e7b9a`, `--dump-project-oop-llvm` terminó con
código 0. En el IR se observan la llamada a `apply_record_callback`, la
escritura en `record_callback_global` y las llamadas a
`run_record_callback_holder` y `run_record_callback_global`. Esta comprobación
no ejecuta el programa.

La validación agrupada posterior con el mismo build terminó con código 0 en
O0–O3. C invocó el callback NativeSharp tanto como argumento como a través del
campo y del global C. No se probaron capturas ni conservación posterior.
