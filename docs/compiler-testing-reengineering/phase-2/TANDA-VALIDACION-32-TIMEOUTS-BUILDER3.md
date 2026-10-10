# Tanda de Validación 32: contrato exacto de timeouts de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

El esquema central publica siete clases de timeout, pero Builder 3 sólo
reconocía cinco. `_timeout_class` convertía silenciosamente cualquier valor no
reconocido en `normal`:

- `49` casos `short`: `15 s` se convertían en `30 s`;
- `398` casos `medium`: `60 s` se convertían en `30 s`.

El mismo fallback ocultaba cualquier clase inválida futura.

## Corrección mínima

Se añadieron `short=15.0` y `medium=60.0` al mapa de Builder 3. El lector ya no
aplica un valor sustituto: exige una cadena perteneciente al contrato y lanza
un error explícito en otro caso.

## Evidencia

- Clases del esquema central: `7`.
- Clases de Builder 3 después del cambio: `7`.
- Diferencias de nombre o duración: `0`.
- Filas vivas cotejadas: `5,695`.
- Filas degradadas antes del cambio: `447`.
- Filas degradadas después del cambio: `0`.
- Pruebas dirigidas: `3/3` PASS.
- Suite Python completa del controlador: `91/91` PASS.

No se modificaron manifiestos ni casos. No se configuró, compiló ni ejecutó
producto; no hubo baseline, despliegue ni escritura en `C:`.
