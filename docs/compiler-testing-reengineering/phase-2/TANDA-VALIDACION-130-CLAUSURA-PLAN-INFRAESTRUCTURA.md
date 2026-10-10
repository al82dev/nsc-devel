# Tanda Validacion 130: clausura del plan de infraestructura

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente la omision de comandos no iniciados despues
de un fallo de infraestructura. No cambia procesos, comandos, orden, CMake,
compilador, NSLink, casos, manifiestos, oraculos ni bateria historica. No
configura, compila o ejecuta producto, no ejecuta baseline y no toca la unidad
`C:`.

## Defecto demostrado

`metadata.commands` declara el plan completo, pero `CommandWorker` abandonaba
el ciclo inmediatamente despues del primer codigo de salida fallido. El comando
fallido tenia outcome; todos los posteriores desaparecian de `results.json` y
`summary.md` sin estado ni causa.

La ruta de cancelacion ya cerraba el sufijo no iniciado como `CANCELLED`. La
ruta de fallo no mantenia la misma clausura terminal y tampoco declaraba
`variant_count`, por lo que `EvidenceStore` no podia rechazar el recibo
incompleto.

## Correccion causal

`blocked_infrastructure_outcomes` proyecta iterativamente cada comando posterior
como `BLOCKED` e identifica la precondicion que fallo. No ejecuta esos comandos
ni cambia la politica de fail-fast.

La metadata de toda operacion de infraestructura incluye ahora
`variant_count = len(commands)`. `EvidenceStore.finish` exige por tanto una
fila terminal unica por cada entrada del plan antes de publicar evidencia.

El resultado distingue sin ambiguedad:

- comando exitoso: `PASS`;
- comando que fallo: `FAIL` con diagnostico causal;
- comando activo cancelado o no iniciado por cancelacion: `CANCELLED`;
- comando no iniciado por una precondicion fallida: `BLOCKED`.

## Validacion ejecutada

Prueba dirigida de resultados de infraestructura:

```text
python -m unittest tests.compiler-suite.controller.tests.test_nscbuilder3_command_cancellation
Ran 9 tests in 0.001s
OK
```

La matriz cubre el sufijo bloqueado, fallo en el ultimo comando, diagnosticos,
reporte condensado, excepcion del controlador y cancelacion.

Suite Python completa de infraestructura:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 279 tests in 27.840s
OK
```

Auditoria no mutante del catalogo:

```text
python nscbuilder3.py --audit-catalog
cases: 9587
contracts: 1208
components: 44
manifest_rows: 5695
executable: 5695
non_executable: 3892
unclassified: 0
exit: 0
```

Smoke de la GUI:

```text
python nscbuilder3.py --gui-smoke
exit: 0
```

## Limite de la afirmacion

La clausura estatica del plan queda implementada y probada. No se provoco un
fallo real de configuracion o construccion para generar un recibo nuevo y no se
afirma que el HEAD actual compile o ejecute sin errores. La corrida integral
externa permanece `PENDING_USER_RUN`; la tarea persistente sigue activa hasta
demostrar cero errores inesperados en la matriz ejecutable acordada.
