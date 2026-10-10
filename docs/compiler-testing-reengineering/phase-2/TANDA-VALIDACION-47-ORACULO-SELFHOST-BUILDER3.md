# Tanda de Validación 47: oráculo self-host de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

La ruta self-host leía `exit_code` mediante `int(...)` y utilizaba valores
predeterminados. Una cadena o un booleano podían convertirse silenciosamente en
un oráculo válido; además, la forma del JSON esperado no se validaba antes de
preparar y ejecutar el fixture.

## Corrección mínima

`parse_selfhost_oracle` acepta sólo el esquema observado en el manifiesto vivo:

- `exit_code` entero obligatorio, nunca booleano;
- `description` textual opcional;
- `json` opcional y necesariamente objeto;
- ningún campo desconocido.

`_run_selfhost` valida el oráculo antes de preparar el productor y usa el exit
code exacto, sin conversiones ni valores predeterminados.

## Evidencia

- Todos los `192` oráculos self-host vivos: aceptados.
- Exit ausente, booleano o textual: rechazado.
- Descripción no textual: rechazada.
- JSON nulo, lista, cadena o entero: rechazado.
- Campo desconocido: rechazado.
- Pruebas dirigidas: `5/5` PASS.
- Suite Python completa del controlador: `149/149` PASS.

No se preparó ni ejecutó ningún fixture, no se abrió la GUI y no se configuró,
compiló o ejecutó producto. No hubo baseline, despliegue ni escritura en `C:`.
