# T08: callback con volatile heredado de un record C

Con ABI Microsoft, un campo de puntero a funcion declarado volatile usa accesos atomicos acquire/release cuando esta alineado; si la volatilidad proviene exclusivamente del record contenedor, Clang usa accesos volatile simples. NSC aplicaba acquire/release tambien en el segundo caso.

El lowering consulta ahora si el propio campo fue declarado volatile antes de aplicar el orden atomico. El caso empaquetado y el campo declarado volatile mantienen sus reglas previas.

Se agrego una global C de record volatile, lecturas y escrituras desde NativeSharp y verificacion cruzada con C. `pruebas-tanda-08-callback/validar.cmd` termino con codigo 0 en O0-O3; el comparador de IR exige accesos simples para la global y conserva los controles de campo volatile explicito, global y campo empaquetado. No se ha ejecutado la validacion completa de la tanda 8.
