# T09-003: parámetro C++ noescape en planes ABI

Una función C++ `preserve_none` con parámetro `noescape` era rechazada porque Clang representa la información mediante el atributo LLVM `captures`. NSC reconoce ese atributo en parámetros y lo omite conservadoramente: no cambia la firma física ni supone una garantía de captura o de duración que no haya demostrado. Los atributos ABI no representados siguen rechazándose; la sonda comprueba el rechazo de `nest`.

En llamadas SysV variádicas, `noescape` también puede formar parte de la información extendida del prototipo Clang. La observación acepta únicamente ese bit, o la ausencia de información; rechaza cualquier otro bit, incluidos tratamientos ABI especiales y `pass_object_size`. La prueba del plan confirma tanto el nuevo caso como el rechazo de un parámetro con `pass_object_size` (`A:/temp/t09-noescape-variadic-after.out`).

La muestra `validar-noescape.py` comprueba que la definición Clang contiene `captures` y que el IR de NSC no agrega esa promesa. Pasaron ocho comparaciones de llamada directa y callback en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-cpp-puntero-escalar-3_hyk_0d`) y 16 consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-puntero-escalar-precompilado-_pgfal43`). Los registros del fixture están en `A:/temp/nsc-t09-preserve-noescape-toocwb0d`.

Una función C++ variádica SysV con parámetro `noescape` pasó ocho comparaciones vivas (`A:/temp/nsc-t09-cpp-variadica-sysv-_6qzpxch`) y 16 consumidores precompilados (`A:/temp/nsc-t09-cpp-variadica-sysv-precompilado-g3o4plyp`); Clang confirma la declaración en `A:/temp/nsc-t09-variadic-noescape-6pjjoac9/oracle-ast.log`. Es una muestra concreta, no certifica todos los argumentos o callbacks variádicos.

Pasaron la compilación incremental, las tres pruebas dirigidas de planes y observación (`A:/temp/t09-noescape-variadic-after.out`), la regresión `nonnull` (`A:/temp/nsc-t09-preserve-nonnull-psrfy7w6`) y el record C variádico precompilado (`A:/temp/nsc-t09-sysv-indirecta-record-pre-p35t0zcx`). T09-003 y la tanda 9 siguen abiertos.
