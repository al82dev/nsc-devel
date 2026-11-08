# Tanda 8: prueba del ABI de bool C

Se prepara un caso de funciones C con `_Bool` en retornos y parámetros, incluido un parámetro entre enteros. La prueba contrasta los atributos LLVM de las definiciones de Clang con las declaraciones y llamadas de NativeSharp, y después ejecuta el consumidor en O0–O3. El resultado funcional por sí solo no demuestra que los atributos ABI estén presentes.

La línea base falla como se esperaba: Clang emite `define dso_local zeroext i1 @c_bool_identity(i1 noundef zeroext ...)`, mientras NSC emite `declare i1 @c_bool_identity(i1)`. Evidencia: `A:\nsc-interop-build\tanda08-bool-abi-c-13992-13678`. La comprobación se detiene al detectar la primera diferencia, antes de ejecutar O0–O3; la reparación y su validación se registrarán por separado.
