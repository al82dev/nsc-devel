# Declaración LLVM del global C TLS

El indicador de TLS del estado de namespace se transporta a MIR y hace que la
declaración del global C externo se emita como `thread_local` en LLVM. No se
definen almacenamiento ni accesores nuevos: la biblioteca C sigue siendo la
propietaria. La ejecución, el aislamiento entre hilos y los casos negativos
están pendientes de validación agrupada. No se habilita TLS de DLL.

La matriz agrupada pasó O0–O3 con build `1f3bce0dac388677`, incluyendo el
control de IR y el rechazo de `volatile`. La suite de sesión Clang pasó 1/1;
la batería completa de la tanda sigue pendiente.
