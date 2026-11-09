# Limpieza de estados vacíos de punteros compartidos

El compilador puede destruir el estado vacío de `Rc`, `WeakRc`, `Arc` y
`WeakArc` al abandonar un constructor que falla. Esta regla permite limpiar un
campo inicializado indirectamente por un método del constructor. Antes de esta
corrección, un tipo de usuario con el mismo nombre y namespace podía recibir
el mismo tratamiento aunque su destructor no fuera seguro sobre ceros.

La reproducción `tests/integration/shared_default_identity.py` crea dos
variantes por familia: un destructor observable directamente en el tipo y otro
en un campo interno de forma similar al SDK. En la compilación anterior,
`Rc` en O0 devolvió 1 en ambas variantes: el destructor se ejecutó sin haberse
inicializado el campo. La reproducción aislada conserva registros en
`A:/nsc-fixes-evidencia/shared-default-identity-before.log` y
`A:/nsc-fixes-evidencia/shared-default-identity-nested.log`.

La selección exige además una forma de almacenamiento y un destructor interno
cuyo único bloque está protegido por una comparación del campo `block` contra
null. Así, el destructor de un homónimo arbitrario no queda autorizado solo
por su nombre. Esta comprobación es deliberadamente conservadora: si cambia la
forma de las implementaciones del SDK, habrá que volver a acreditar su estado
vacío antes de conservar esta optimización.

Validación con `A:/nsc-fixes-evidencia/build/bin/nsc.exe`, build
`e68e06035b85fe9f`, sin modificar la instalación del sistema:

* La regresión de homónimos devuelve 42 en las 16 combinaciones: cuatro
  familias, destructor directo o anidado, O0 y O2.
* Los controles dirigidos de `shared-public-pointers` pasan 48 ejecuciones,
  incluidos constructor fallido, inicialización indirecta, movimiento y
  controles sin relación, en O0 y O2.
* El consumidor del SDK binario aislado `generic-owner-sdk42` pasa las
  pruebas existentes de propietarios, expiración y solo lectura y las nuevas
  pruebas de limpieza de campos débiles `Rc` y `Arc` tras una excepción, en
  O0 y O2. El primer intento usó un constructor implícito no público y no
  compiló; la reproducción definitiva declara el constructor público.

Registros: `A:/nsc-fixes-evidencia/shared-default-identity-check.log`,
`shared-field-default-controls.log` y `shared-binary-public-pointers.log`.
Las pruebas del SDK binario cubren estos escenarios concretos; no acreditan
DLL, dependencias externas ni todas las rutas posibles de excepciones. Esta
corrección no cierra las limitaciones restantes de las cuatro familias.
