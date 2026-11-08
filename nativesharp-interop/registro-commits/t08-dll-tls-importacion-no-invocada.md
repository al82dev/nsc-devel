# Control de inicialización TLS e importación no invocada

La comparación se repitió en Windows `10.0.28000.3231`, con Clang y LLD 23.1.0 del árbol de A:. En la DLL sin importaciones y con punto de entrada, un contador exportado confirma que `DllMain` se ejecutó una vez, aunque la función TLS siguió leyendo un valor incorrecto. Esto descarta que la ausencia de ejecución de `DllMain` explique por sí sola el resultado.

Se añadió otra variante: la función que lee TLS no cambia, pero otra función exportada contiene una referencia a `GetCurrentThreadId` que el control no llama. `llvm-readobj` confirma la importación. Con esta variante, tanto el control enlazado como el de carga dinámica observaron el valor inicial 7; sin importaciones, ambos observaron un valor distinto. El script contrasta las cuatro variantes y valida sus resultados.

La observación está limitada a este entorno y a estos binarios. No demuestra la causa interna del cargador ni establece que añadir una importación sea una solución aceptable. La investigación y la corrección del caso sin importaciones siguen pendientes.
