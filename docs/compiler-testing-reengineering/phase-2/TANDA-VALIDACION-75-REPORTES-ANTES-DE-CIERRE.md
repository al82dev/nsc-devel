# Tanda de Validacion 75: reportes antes del cierre

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`SuiteRunner` emitia `RUN_FINISHED` y regresaba los resultados antes de que
`run_suite.py` publicara `results.json`, `summary.md` y `failures.log`. Si la
publicacion fallaba, el stream afirmaba una corrida terminada sin evidencia
terminal completa.

## Correccion

- el runner acepta un publicador terminal de resultados completos;
- el publicador se ejecuta antes de `RUN_FINISHED`;
- `run_suite.py` construye metadata y report writer antes de iniciar el runner;
- una excepcion de publicacion se propaga y no publica cierre terminal;
- los eventos progresivos ya escritos se conservan para diagnostico.

## Evidencia ejecutada

- pruebas dirigidas de runner y reportes: `11/11` PASS;
- fallo sintetico de publicacion: ultimo evento `CASE_FINISHED`, sin
  `RUN_FINISHED`;
- suite Python completa del controlador: `215/215` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra el orden causal entre evidencia terminal y cierre del
stream. No acredita una corrida funcional. La meta permanece activa hasta cero
errores inesperados en la matriz acordada.
