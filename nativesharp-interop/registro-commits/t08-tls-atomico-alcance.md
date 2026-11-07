# Alcance: global TLS C atómico

Un global C `_Thread_local _Atomic(int)` es rechazado por NSC como no proyectado.
Se preparará una prueba positiva con control Clang y comprobación de orden
atómico. El rechazo es un límite pendiente, no interoperabilidad completada.
