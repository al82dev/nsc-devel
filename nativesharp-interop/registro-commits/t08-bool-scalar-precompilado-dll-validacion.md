# T08: validación de global C bool precompilado mediante DLL

Se ejecutó `nativesharp-interop\pruebas-tanda-08-record-c\precompiled-bool\validar.cmd`
desde `A:\nsc-devel`. El productor estático NativeSharp, la DLL intermedia y
el consumidor independiente pasaron en O0, O1, O2 y O3. Durante la compilación
de la DLL y el consumidor, `Api.h` permaneció oculto; se verificaron lecturas y
escrituras cruzadas con las funciones C y la modificación compuesta.

La prueba también ejecutó el control anterior de biblioteca estática en los
cuatro niveles. Este resultado no acredita un global importado directamente de
una DLL C ni cierra T08-004, T08-015 o la tanda 8.
