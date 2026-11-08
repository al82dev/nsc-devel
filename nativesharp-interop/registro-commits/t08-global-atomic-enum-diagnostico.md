# Tanda 8: diagnóstico de global C con enum atómico

Se agrega una prueba de una global `_Atomic(enum State)` definida en C y consumida
desde NativeSharp. El consumidor verifica el valor inicial, escribe otro enumerador
y pide a C verificar el resultado. El validador está preparado para O0–O3.

En la línea base, Clang compiló la biblioteca C, pero NSC rechazó la importación
en O0 con `C/C++ global type is not yet mapped`; por eso no se ejecutaron las
variantes O1–O3. Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-25229-3872`.
La prueba queda pendiente de corrección: su presencia no acredita soporte ni cierre
de la tanda. La solución debe preservar la identidad del enum y la semántica
atómica, y comprobar también la alineación y el IR frente a Clang.
