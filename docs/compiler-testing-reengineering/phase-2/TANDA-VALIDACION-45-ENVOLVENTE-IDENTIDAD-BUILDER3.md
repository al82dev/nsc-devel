# Tanda de Validación 45: envolvente de identidad de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

La ruta `_run_identity` aceptaba cualquier objeto con una lista `results` de
cardinalidad uno. No comprobaba el esquema, la identidad de la corrida, el caso,
el estado, los tipos de evidencia, los conteos ni la concordancia entre el
reporte y el código de salida. Un reporte ajeno o malformado podía convertirse
en un resultado terminal de la variante solicitada.

## Corrección mínima

`parse_identity_envelope` valida el reporte canónico producido por
`run_suite.py`:

- esquema `1` y objeto superior;
- identidad exacta de corrida, revisión, modo y optimización;
- alcance enfocado con exactamente un caso seleccionado;
- exactamente un resultado del caso solicitado;
- estado, duración, fallos y bloqueadores con su forma contractual;
- conteos completos y coherentes con el resultado;
- exit `0` para `PASS`/`KNOWN_FAILURE` y exit `1` para los demás estados.

`_run_identity` fija una sola revisión para el comando y la validación. Toda
violación conserva `results.json` como evidencia y termina como
`INFRASTRUCTURE_ERROR`.

## Evidencia

- Envolventes `PASS` y `KNOWN_FAILURE` válidas: aceptadas.
- Cuatro dimensiones de identidad de corrida incorrectas: rechazadas.
- Identidad de caso ajena: rechazada.
- Esquema, cardinalidad, duración y fallos malformados: rechazados.
- Estados, bloqueadores y conteos contradictorios: rechazados.
- Dos contradicciones de exit code: rechazadas.
- Pruebas dirigidas: `5/5` PASS.
- Suite Python completa del controlador: `139/139` PASS.

No se ejecutó `run_suite.py`, no se abrió la GUI y no se configuró, compiló o
ejecutó producto. No hubo baseline, despliegue ni escritura en `C:`.
