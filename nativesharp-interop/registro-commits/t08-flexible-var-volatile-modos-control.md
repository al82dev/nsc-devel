# T08-010/015: comparador de acceso por alias local

El diagnóstico genera IR aislado con Clang y NSC en modo Microsoft e ISO y
compara la única lectura y escritura `volatile i32` de cada función, incluyendo
atomicidad, orden y alineación. Salida 0 significa coincidencia, 1 diferencia y
2 error de preparación o de lectura del IR. No se conecta al gate hasta que la
discrepancia esté resuelta. Su ejecución queda pendiente.
