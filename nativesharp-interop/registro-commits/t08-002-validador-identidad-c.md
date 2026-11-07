# T08-002: validador de identidad C

Nuevo `pruebas-tanda-08-identidad-c/validar.cmd`. Ejecuta las sondas de
`nsc_cpp_c_identity_tests` y, para tres productores existentes del fixture
`pruebas-tanda-08-meta-c` (`Producer.ns`, `ProducerValue.ns` y
`ProducerUnion.ns`) en O0, O1, O2 y O3, compila `Api.c` con Clang, genera la
biblioteca con `nsc --emit-project-oop-lib` y comprueba la restauración de USR
desde su NSMETA. Copia los fixtures sin modificarlos.

Rutas: por omisión usa las del gate (`A:\nsc-interop-build\tanda1-ON`,
`A:\nsc-interop-build\tanda08-identidad-c`, `A:\al82dev-clang` y el `nslink.exe`
del gate). `NSC_T08_BUILD`, `NSC_T08_OUT`, `NSC_T08_CLANG` y `NSC_LINKER`
permiten apuntar a otro build; `TEMP` y `TMP` quedan bajo la carpeta de salida y
el `PATH` solo se modifica dentro del proceso. No está incluido en
`validar-tanda-08.cmd`: su alta pertenece a la integración.
