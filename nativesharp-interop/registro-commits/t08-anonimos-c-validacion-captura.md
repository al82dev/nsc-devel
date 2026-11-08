# Validación agrupada de captura y comparación de agregados C11

En `A:/nsc-interop-build/tanda1-ON` se construyeron incrementalmente los objetivos
`nsc_cpp_c_identity_tests` y `nsc_cpp_session_tests`; la construcción terminó con
código 0. Emitió advertencias de inicializadores, incluidas 33 en la suite de sesión.

Con Clang de `A:/al82dev-clang`, la prueba de identidad terminó con código 0 usando
`A:/nsc-interop-build/t08-anonimos-c-captura` como salida. La prueba de sesión
terminó también con código 0. Las pruebas cubren la captura de struct y union
anónimos anidados y la comparación de definiciones con campos distintos.

Una primera invocación a `A:/nsc-devel/build-clang` no compiló: esa configuración
tiene interoperabilidad desactivada y no conoce el objetivo. La construcción válida
se hizo en `tanda1-ON`. Esta evidencia no verifica la proyección, la persistencia en
metadata ni la batería completa de la tanda 8; T08-010 y T08-011 continúan abiertos.
