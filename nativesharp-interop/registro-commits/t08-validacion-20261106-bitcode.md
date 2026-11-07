# Tanda 8: validación agrupada tras detección de TLS en bitcode

Se ejecutó `nativesharp-interop/validar-tanda-08.cmd` en A: con el NSC de desarrollo. El registro completo está en `A:/nsc-interop-build/tanda08-validacion-completa.log`. El comando terminó con código 1: la tanda sigue abierta.

- La prueba TLS de la biblioteca precompilada con bitcode ThinLTO y LTO completo pasó, incluido el diagnóstico `_tls_index` al omitir el CRT.
- El aislamiento TLS entre dos hilos pasó de O0 a O3; también los controles de selección automática, explícita y biblioteca precompilada.
- Fallaron `directo/validar.cmd` al enlazar el consumidor C, `directo/validar-nsc-escalares.cmd` y `directo/validar-nsc-escalares-con-entry.cmd` al ejecutar el consumidor NS en O0, y el control C `directo/validar-escalar-carga-dinamica.cmd`.
- La variante `directo/validar-nsc-escalares-con-import.cmd` pasó de O0 a O3. Las observaciones previas de `tls-indice-cargador.md` y `tls-c-dll-directo.md` documentan los límites de esos controles; esta ejecución no acredita una solución para los cuatro fallos.

No se modificó NSLink ni se avanzó a la siguiente tanda. Los resultados de la batería se conservan sin eliminar ni desactivar pruebas.
