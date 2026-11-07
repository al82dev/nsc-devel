# T08: USR en metadatos reales de productor C

La prueba de publicación de tipos C ejecuta un productor NSC real y, en un
proceso separado, compara el USR del registro y los USR de todos sus campos
en el catálogo del NSMETA con los observados por Clang sobre el mismo header.
Después compila y ejecuta el consumidor independiente existente.

La construcción incremental de `nsc_cpp_session_tests` y la suite de sesión
pasaron. `pruebas-tanda-08-meta-c/validar.cmd` pasó con productor y consumidor
en O0, O1, O2 y O3; en cada configuración pasó la comparación de USR.
Registro: `A:/nsc-interop-build/t08-usr-productor-consumidor.log`.

Esto comprueba la persistencia en un NSMETA real y el funcionamiento del
consumidor probado, pero no demuestra por sí solo que el consumidor restaure
cada USR. Tampoco cubre la identidad de las demás categorías de declaraciones.
T08-002 y la validación integral de la tanda permanecen abiertos.
