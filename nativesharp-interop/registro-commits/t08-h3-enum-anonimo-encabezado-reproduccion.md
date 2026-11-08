# T08 H3: enumerador anónimo con encabezado y biblioteca

El consumidor incluye el mismo `Gen.h` del productor y también utiliza la plantilla de su biblioteca precompilada. Comprueba tanto el valor devuelto por la plantilla como `GenLoose` directamente.

La ejecución dirigida falló en O0 con `ambiguous namespace state reference 'GenLoose'`: el enumerador vivo y el restaurado permanecen separados. La prueba queda como expectativa positiva pendiente de corrección; no se ha ejecutado la batería completa. Registro: `A:/nsc-interop-build/tanda08-identidad-enum-anonimo-header.log`.

Los USR de declaraciones anónimas pueden colisionar entre encabezados distintos, por lo que no basta unir nombres y USR sin una prueba de origen y equivalencia.
