# Referencia TLS con objeto NativeSharp y LLD

Se preparó una matriz reproducible O0–O3 que genera el objeto NativeSharp,
lo enlaza con la biblioteca C TLS mediante LLD y ejecuta el mismo control de
aislamiento entre hilos. Un punto de entrada auxiliar invoca el símbolo Main
del objeto; se usa solo como instrumento y no sustituye la validación del
arranque y enlace normales de NSC.

El script `pruebas-tanda-08-tls-c/lld/validar.cmd` pasó O0–O3 con NSC build
`a02f3209efef196f`: cada ejecutable comprobó el valor inicial y el aislamiento
entre el hilo principal y otro hilo. La ruta normal de NSC y el TLS directo
de DLL siguen sin resolver; la tanda 08 permanece abierta.

Un ensayo adicional O0 enlazó el mismo objeto NativeSharp con `libcmt.lib` y
`kernel32.lib` como únicas bibliotecas de la línea de enlace, usando
`/include:_tls_used`; ejecutó correctamente el control entre hilos. El
comando está en `A:/nsc-interop-build/tanda08-tls-c/probe-min-crt.cmd`. Esto
no prueba que esas dependencias basten para otras bibliotecas C o C++.
