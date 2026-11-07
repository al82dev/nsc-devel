# T08: ejecución integral del 2026-11-07

Se ejecutó `validar-tanda-08.cmd` después de agrupar los cambios de TLS atómico y sus pruebas de biblioteca precompilada. La construcción fue incremental en `A:/nsc-interop-build`, incluyendo las configuraciones ON y OFF; no se modificó ni desplegó en C: y no se modificó NSLink. El gate terminó con código 1. Salida completa: `A:/nsc-interop-build/tanda08-integral-20261107.log`.

El validador registró cuatro fallos:

1. `directo/validar.cmd`: el consumidor C no enlaza; LLD informa `undefined symbol: tls_exported` en `probe-link.log`.
2. `directo/validar-nsc-escalares.cmd`: ejecución O0 de escalares TLS en DLL.
3. `directo/validar-nsc-escalares-con-entry.cmd`: ejecución O0 de escalares TLS en DLL.
4. `directo/validar-escalar-carga-dinamica.cmd`: fallo del control C de carga dinámica.

Los casos `validar-dll`, acceso directo NSC y acceso NSC desde otro hilo informaron O0–O3 correctos. También pasaron los controles atómicos y precompilados incorporados al gate. La salida impresa identifica esos resultados puntuales, no prueba que las capacidades no incluidas funcionen. Se conserva la evidencia previa sobre las diferencias de arranque y restricciones de Clang; las pruebas DLL originales no se modificaron. La tanda 8 no está validada y no se avanza a la 9.
