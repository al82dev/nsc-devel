# Instrumento de acceso TLS de DLL desde NativeSharp

Se construyó la DLL del fixture existente con Clang/LLD y un adaptador C
estático separado que toma la dirección de importación PE del dato, encuentra
su módulo e interpreta el directorio TLS. Un proyecto NativeSharp obtiene la
dirección mediante `tls_bridge()`, comprueba el valor, escribe 41 y verifica
con funciones de la DLL tanto la lectura como el aislamiento del otro hilo.

Con `nsc` de desarrollo `deffd2b4770d5278`, el script `validar-bridge.cmd`
pasó O0, O1, O2 y O3 con NSLink sin cambios. El adaptador usa detalles
internos de Windows x64 y es solo instrumento: la llamada explícita no es la
interoperabilidad directa solicitada. No sustituye la prueba directa original
ni acredita otros tipos, DLL o plataformas. El siguiente paso es producir la
dirección desde el descenso de NSC preservando la identidad de la biblioteca.
