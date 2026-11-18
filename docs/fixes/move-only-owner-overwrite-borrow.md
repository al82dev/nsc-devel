# Reemplazo de un propietario con préstamo activo

Un `Rc` o `Arc` local podía reemplazarse por otro mientras una vista o alias
de `Get()` del valor anterior seguía vivo. Antes del cambio el análisis aceptó
la reproducción con `Borrow()` tanto para `Rc` como para `Arc`; no se ejecutó
el uso del alias posiblemente inválido. La salida se observó en CTest, pero no
se conservó un archivo de log independiente de esa primera ejecución.

`classifyDirectAssignmentTarget` reconoce los structs locales move-only, pero
`prepareAssignmentValue` consultaba la protección de préstamo solo para el
destino RAII. Ahora también consulta el destino move-only directo cuando no es
RAII, usando el mismo seguimiento de préstamos y diagnóstico. El cambio no
afirma protección de destinos que no se resuelvan como ese local directo.

Tras una compilación incremental de `nsc`, el selector
`A:/nsc-fixes-evidencia/readonly-owner-overwrite-check.cmd` pasó para los
controles internos local y atómico: rechazó el reemplazo con alias vivo y
ejecutó el reemplazo después del fin del préstamo con salida 42 en O0/O2.
El selector de la prueba pública, desde fuentes, también rechazó los reemplazos
con vista, alias de `Get()`, movimiento desde otro propietario y vista viva
después de `await`, para `Rc` y `Arc`. El reemplazo tras el ámbito de la vista
ejecutó con salida 42 en O0/O2. Las pruebas públicas dependen de la superficie
de punteros aún en desarrollo y no acreditan consumo binario ni concurrencia.
