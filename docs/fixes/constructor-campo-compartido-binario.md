# Limpieza de campos compartidos en constructor precompilado

Si `Holder` se compila en un módulo separado que consume Core binario, su
constructor puede recibir un `Rc`, `WeakRc`, `Arc` o `WeakArc`, moverlo a un
campo mediante un método y lanzar antes de completar la construcción. Antes de
esta corrección, el campo no se limpiaba y el contador conservaba una referencia
de más. Las 32 combinaciones de productor y consumidor O0/O2 reprodujeron el
fallo; las excepciones sí se capturaron.

El reconocimiento del estado vacío seguro exigía una función MIR de la
especialización de la envoltura. Cuando ningún método de esa envoltura se
utilizaba en el productor, no existía esa función aunque el tipo sí estaba
especializado. El backend ahora consulta también la identidad de especialización
de la declaración, manteniendo las comprobaciones previas del layout y del
destructor protegido por null. No cambia la regla de estructuras ordinarias.

`tests/integration/shared_binary_owner_constructor_failure.py` crea el productor
y el consumidor por separado. Recibe como argumentos el compilador y las rutas
del Core binario y su metadata. Con el compilador aislado SHA-256
`1b7d83d0e089fbe073942a63a0035c3de63e382906692709221a865e24ade4cd` y el Core
preexistente SHA-256 `f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`,
las 32 combinaciones ejecutaron 42; los 16 controles de homónimos pasaron.
También pasaron las selecciones dirigidas de construcción fuente y de consumo
binario. Los comandos y logs están en `A:/nsc-fixes-evidencia` con el prefijo
`shared-binary-owner-identity-`.

La validación utilizó un árbol con otros cambios locales, no un checkout limpio.
Core no se regeneró para esta ejecución. Los contadores no prueban por sí solos
la liberación física del bloque; DLL y otros layouts no se comprobaron.

El control adicional `shared_binary_owner_empty_constructor.py` deja el campo
de cada familia vacío y fuerza la excepción desde un constructor precompilado.
Incluye un campo de estructura ordinaria sin inicializar cuyo destructor no
debe ejecutarse. Las 16 combinaciones de productor y consumidor O0/O2
terminaron con 42 (`A:/nsc-fixes-evidencia/shared-binary-owner-empty-current.cmd`
y `.log`), usando el mismo compilador y Core indicados arriba. La prueba verifica
la ausencia de una destrucción observable del campo ordinario y de fallos en el
estado vacío; por sí sola no demuestra que se invoque el cleanup vacío del
puntero compartido. No cubre DLL ni inicializadores explícitos de ese campo.
