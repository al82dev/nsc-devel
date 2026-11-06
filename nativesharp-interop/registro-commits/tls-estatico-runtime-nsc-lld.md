# TLS con arranque de runtime NativeSharp y LLD

La prueba O0 aislada generó el IR del runtime mediante `nsc --dump-runtime-llvm`,
lo compiló con Clang y enlazó con LLD el punto de entrada `__nativesharp_start`,
el objeto NativeSharp y la biblioteca C TLS. Un adaptador C enlaza el nombre
`Main` del runtime de referencia con el símbolo generado por el proyecto.
El programa pasó el control de aislamiento entre dos hilos. Se requirieron
bibliotecas estáticas del runtime y bibliotecas de importación de Windows.

La matriz reproducible `validar-runtime.cmd` pasó O0–O3 con NSC build
`a02f3209efef196f`; Clang emitió un aviso de normalización del triple objetivo.
Esta prueba todavía enlaza fuera del
flujo normal de NSC y no demuestra inicialización global C++ ni TLS directo de
DLL. NSLink y LLVM permanecen sin modificar.
