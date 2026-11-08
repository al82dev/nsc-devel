# Campos y enumeradores en records anónimos anidados

La muestra contiene una estructura, una union y otra estructura anónimas
anidadas, con un campo de enum anónimo y otro campo entero. El validador consulta
los USR de ambos campos y los enumeradores mediante `c-index-test` de Clang;
después compila y ejecuta el acceso cruzado C/NativeSharp tanto directamente
como desde una biblioteca NativeSharp precompilada. El validador pasó en O0.

La sonda existente `--verify-driver-restored-c-usr` no es un oráculo válido
para esta muestra: busca records por su nombre frente a Clang, mientras NSC
materializa los anónimos con nombres internos, y también intenta localizar el
campo portador sin nombre. Esa sonda falló para la muestra. Aún falta comparar
explícitamente las identidades de los campos y enumeradores proyectados y
restaurados; la ejecución correcta no demuestra esa propiedad por sí sola.
