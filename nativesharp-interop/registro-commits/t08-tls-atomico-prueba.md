# Prueba positiva de TLS C atómico

El fixture compara lectura, escritura y `+=` de `_Thread_local _Atomic(int)`
con Clang y exige cargas, escrituras y actualización atómicas `seq_cst` en IR,
además de ejecución NativeSharp O0–O3. El control C se ejecuta primero; NSC
actualmente rechaza la declaración, por lo que la prueba no está aprobada.
