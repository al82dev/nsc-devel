# Callback NativeSharp con puntero a record invocado por C

Se preparó un callback NativeSharp sin capturas `int(CallbackPayload*)`, que
una función C recibe y llama. El mismo callback se asigna a un campo y a un
global C, y otras funciones C los invocan. El fixture comprueba el resultado y
está incluido en la matriz agrupada de callbacks O0–O3.

Esta preparación no demuestra que el caso compile o ejecute. No cubre
capturas ni callbacks conservados después del tiempo de vida comprobado.
