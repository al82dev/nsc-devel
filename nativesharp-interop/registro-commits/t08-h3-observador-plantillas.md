# T08 H3: demandas del observador en plantillas

El control de candidatos compila hasta el modelo de miembros y construye el
observador de funciones y valores sobre las mismas unidades sintácticas.
Contrasta por identidad del nodo y proyecto las demandas del observador con
las referencias capturadas de dos plantillas sin instanciar.

El control comprueba la llamada `cid_function`, la lectura de `cid_global` y
una lectura de un local con el mismo nombre que la global. Las tres aparecen
como demandas; por ello una demanda o una coincidencia de nombre no demuestra
por sí sola que el cuerpo se refiera a una declaración C. El alias de tipo no
se resuelve mediante este observador de valores.

Se compiló incrementalmente `nsc_cpp_c_identity_tests` y la ejecución con
Clang 23.1 y temporales en A: terminó con código 0. No se ejecutó la batería
completa ni se modificó NSMETA. La identificación semántica de las referencias
y el transporte hacia consumidores precompilados siguen pendientes.
