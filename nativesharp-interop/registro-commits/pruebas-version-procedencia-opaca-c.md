# Controles de versiones del catálogo C

Los controles históricos de catálogo 1/2 se preparan sin procedencia en el
record adelantado para reproducir sus datos originales. Se agregan controles
para la variante 3 con NSMETA 1.45 y rechazo en 1.44, y para la variante 4 con
formato flotante y proyección ABI equivalente a la variante 2. Falta construir
y ejecutar la suite; no se considera verificada todavía la compatibilidad.

La suite `nsc.cpp-interop.session` pasó 1/1 con build `ab06802a4b42fffe`,
incluidos controles históricos de metadata 1.41–1.44 y los nuevos de catálogo
3/4 y metadata 1.45. La matriz externa de metadata C también pasó O0–O3.
Estas pruebas no certifican todos los archivos de metadata históricos.

## Comprobación independiente de compatibilidad

Se construyeron incrementalmente los targets de pruebas de metadata en
`A:/nsc-interop-build/tanda1-ON` y se ejecutaron las pruebas 456–458 con
temporales en A: y sin activar la actualización de referencias históricas.
El enlace de metadata pasó; la prueba de tolerancia falló 1 de 16 controles y
la de metadata de la etapa 48.2 falló 13 de 423. No se considera validada la
compatibilidad histórica general.

Faltan en `tests/fixtures/nsmeta` las referencias archivadas 1.40 a 1.44:
1.40–1.43 ya faltaban antes de elevar el minor, mientras que 1.44 también pasa
a ser requerida como histórica con el cambio a 1.45. No se generaron archivos
nuevos ni se modificaron referencias antiguas para ocultar la falta de evidencia.
La prueba de tolerancia espera que el archivo 1.37 sea compatible, pero el
verificador lo rechaza por el cambio de Object ABI 2 a 3. La prueba de la etapa
48.2 además espera el minor global en una emisión que reporta 1.40, y falla el
control que cambia la etiqueta a una versión futura. Estos resultados requieren
diagnóstico antes de atribuirlos a la modificación de interop o considerarlos
problemas anteriores; sus causas no se han demostrado todas.
