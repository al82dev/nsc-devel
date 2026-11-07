# T08: global C atómico precompilado y DLL

`pruebas-tanda-08-atomic-global/validar-precompilado.cmd` terminó con código 0
y marcadores O0–O3. El productor NativeSharp se compiló contra el header C;
con ese header oculto se construyeron y ejecutaron el consumidor directo y el
consumidor mediante DLL intermedia. En cada ruta se comprobaron lectura,
incremento y observación del valor por la función C.

La prueba cubre `_Atomic(int)` no TLS, no los demás tipos ni acceso directo a
un global exportado por DLL C. Se agregó al gate agrupado, pendiente de repetir.
T08-004 y la tanda 8 siguen abiertas. NSLink no se modificó.
