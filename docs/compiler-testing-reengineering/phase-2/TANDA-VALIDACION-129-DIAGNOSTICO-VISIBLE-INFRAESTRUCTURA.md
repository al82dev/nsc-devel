# Tanda Validacion 129: diagnostico visible de infraestructura

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente la perdida de presentacion grafica del
diagnostico causal ya preservado por Validacion 127. No cambia procesos,
comandos, CMake, compilador, NSLink, casos, manifiestos, oraculos ni bateria
historica. No configura, compila o ejecuta producto, no ejecuta baseline y no
toca la unidad `C:`.

## Defecto demostrado

`CommandWorker` entrega resultados terminales con una causa accionable en
`CaseOutcome.detail`. `EvidenceStore.finish` conserva ese campo completo en
`results.json` y lo incorpora a `summary.md`. Sin embargo,
`_infrastructure_finished` solamente mostraba el texto generico `Operacion
terminada con errores` y no actualizaba ni abria la pestaña `Errores
condensados`.

Una excepcion del controlador tenia el mismo problema: llegaba como evidencia
`ERROR: ...`, pero una lista vacia de outcomes hacia que el reporte visual
afirmara que no habia fallos terminales.

## Correccion causal

El formateo del reporte condensado vive ahora en la funcion pura
`condensed_error_report`. Tanto la ejecucion de casos como los cierres de
infraestructura usan exactamente esa autoridad.

Cuando configuracion o construccion termina con un estado terminal fallido:

- la GUI conserva la causa capturada;
- actualiza el reporte condensado;
- abre `Resultados > Errores condensados` automaticamente;
- mantiene `full.log`, `results.json` y `summary.md` como evidencia integra.

Una excepcion del controlador sin outcomes se presenta como una causa terminal
propia, sin declararla falsamente como una ejecucion limpia. Los cierres
correctos siguen mostrando el log completo y no cambian de pestaña.

## Validacion ejecutada

Prueba dirigida del recorrido de infraestructura:

```text
python -m unittest tests.compiler-suite.controller.tests.test_nscbuilder3_command_cancellation
Ran 7 tests in 0.001s
OK
```

La matriz cubre errores de `lld-link`, CMake y MSVC, fallback por codigo de
salida, reporte condensado causal, excepcion del controlador, cancelacion
activa y comandos no iniciados.

Suite Python completa de infraestructura:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 277 tests in 28.119s
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

El recorrido estatico del diagnostico queda cerrado desde la captura hasta la
evidencia persistida y la presentacion grafica. No se realizo una construccion
funcional para provocar un fallo real en la GUI ni se afirma que el HEAD
actual compile o ejecute sin errores. La corrida integral externa permanece
`PENDING_USER_RUN`; la tarea persistente sigue activa hasta demostrar cero
errores inesperados en la matriz ejecutable acordada.
