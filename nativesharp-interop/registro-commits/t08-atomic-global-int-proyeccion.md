# T08: proyección de global C atómico int no TLS

`pruebas-tanda-08-atomic-global/diagnosticar.cmd` compiló la biblioteca C y
NSC rechazó las tres referencias a `atomic_global` con `C/C++ global type is
not yet mapped`. En `projectGlobalWithLookup` el reconocimiento de
`_Atomic(int)` exigía `global.threadLocal`. Se retira esa única condición para
el tipo C int de 32 bits con el tamaño y alineación ya exigidos; los demás
tipos y la proyección TLS conservan sus condiciones anteriores.

La corrección está preparada, sin reconstrucción ni validación posterior.
Faltan comprobación del IR atómico, O0–O3 y regresión TLS; no se acredita el
contrato por el cambio de clasificación. No se modifica NSLink.
