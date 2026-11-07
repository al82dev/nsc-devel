# T08: control de global C atómico int no TLS

Se prepara un control que compara la lectura y el incremento atómico del IR de
Clang y NSC, ambos con `seq_cst`, y ejecuta el acceso cruzado entre C y
NativeSharp en O0–O3. Se usa un directorio de salida aislado. Todavía no se ha
construido el compilador corregido ni se ha ejecutado esta validación.
