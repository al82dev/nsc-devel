# Declaración LLVM del global C TLS

El indicador de TLS del estado de namespace se transporta a MIR y hace que la
declaración del global C externo se emita como `thread_local` en LLVM. No se
definen almacenamiento ni accesores nuevos: la biblioteca C sigue siendo la
propietaria. La ejecución, el aislamiento entre hilos y los casos negativos
están pendientes de validación agrupada. No se habilita TLS de DLL.
