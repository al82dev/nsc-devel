# DLL TLS homónimas: identidad y empaquetado

Cuando dos DLL C distintas exportan la misma TLS y comparten nombre de archivo,
NSC asigna nombres de carga derivados de la huella de cada binario. La generación
de código y el empaquetado usan la misma regla; se conserva una DLL preexistente
con el nombre original. Las DLL necesarias se registran como productos de caché
aun si ya estaban presentes con el mismo contenido durante la publicación.

`validar-mismo-nombre-optimizaciones.cmd` pasó en O0–O3: el consumidor comprueba
ambos valores, su aislamiento entre dos hilos y su ejecución desde otra carpeta.
También elimina los productos y comprueba su restauración desde caché, incluido
un paquete creado con las DLL ya presentes. El control de rutas Unicode previo
también pasó. Se incorporó el caso al validador del bloque de identidad.

La cobertura es parcial: si el ejecutable o una dependencia importa directamente
una de las DLL homónimas por su nombre original, NSC todavía lo diagnostica como
ambiguo. No es interoperabilidad y permanece pendiente, al igual que comprobar
una DLL NativeSharp como producto consumidor. La tanda 8 sigue abierta.
