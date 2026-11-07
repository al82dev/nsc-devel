# T08: validación de apuntados calificados adicionales

La construcción incremental de `nsc` terminó con código 0. El control
`validar-mas-calificados.cmd` pasó O0–O3 para apuntados const y volatile de
float, long y unsigned long C no TLS, con accesos directos e indexados y
observación desde C. El verificador comprobó IR atómico para los punteros,
accesos volatile acquire/release en el IR de Clang y NSC, y tres rechazos por
cada grupo de escritura const y pérdida de const o volatile.

También pasaron las regresiones `validar-mas-punteros.cmd`,
`validar-calificados.cmd` y `pruebas-tanda-08-tls-c/atomic/validar.cmd`. El
control nuevo se agregó al gate agrupado, aún pendiente de repetición. No se
acreditan otros tipos, TLS para estos apuntados ni todos los escapes. T08-004 y
la tanda 8 siguen abiertos; NSLink y metadata no se modificaron.
