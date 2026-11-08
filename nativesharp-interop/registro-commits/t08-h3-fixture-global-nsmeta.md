# T08: prueba de global C en una plantilla precompilada

La prueba construye un productor C, una biblioteca NativeSharp con plantilla pública y un consumidor separado. La plantilla incrementa `fixture_counter` y una función C devuelve su valor, verificando que ambos lenguajes observan el mismo almacenamiento. El consumidor recibe las bibliotecas y el NSMETA, sin copiar el encabezado.

El modo `--inspect-template-global` del ejecutable de pruebas comprueba que el catálogo de dependencias contenga la global con su símbolo. La ejecución dirigida a O0 terminó con código 0. La cobertura de O1 a O3 y la validación completa de la tanda siguen pendientes.
