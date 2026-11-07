# T08: restricción de caché para el productor con referencia C

Al ejecutar `pruebas-tanda-08-record-c/precompiled-bool/validar-cache.cmd`, NSC
rechazó el productor con el diagnóstico `C/C++ project references require
--no-artifact-cache until transitive headers are tracked`. No hubo hit ni
evidencia de ejecución para ese productor con caché. El control se ajustó para
comprobar ese diagnóstico, generar el productor sin caché y evaluar por
separado si el consumidor de la biblioteca precompilada admite caché. El
control ajustado aún no se ha ejecutado; no se relajó la protección de NSC.
