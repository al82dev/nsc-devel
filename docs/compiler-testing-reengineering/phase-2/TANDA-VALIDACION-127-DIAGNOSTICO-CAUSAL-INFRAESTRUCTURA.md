# Tanda Validacion 127: diagnostico causal de infraestructura

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente la perdida del diagnostico capturado al
proyectar comandos de configuracion y construccion a `CaseOutcome`. No cambia
procesos, cancelacion, comandos, CMake, compilador, NSLink, casos, manifiestos,
oraculos ni bateria historica. No configura, compila o ejecuta producto, no
ejecuta baseline y no toca la unidad `C:`.

## Defecto demostrado

La evidencia persistida de `20261009-234910-fcda0e5c` contiene dos resultados:

```text
Configurar Debug               PASS
Construir infraestructura Debug FAIL — exit code 4294967295
```

Sin embargo, su `full.log` contiene diagnosticos concretos de `lld-link`, por
ejemplo simbolos sin definir de `LlvmModule`, `LldLinker`,
`PeExecutableVerifier` y `WindowsRuntimeBuilder`. `results.json` y `summary.md`
perdian esa causa y obligaban a abrir y buscar manualmente el log completo.

## Causa

`ProcessController.run` devuelve `(exit_code, captured_output)`, pero
`CommandWorker.run` desempaquetaba el segundo valor como `_`.
`infrastructure_command_outcome` recibia solamente el codigo y no podia
conservar ningun diagnostico en `CaseOutcome.detail`.

## Correccion causal

`CommandWorker` entrega ahora la salida ya capturada al proyector terminal.
La funcion pura e iterativa `infrastructure_failure_detail` recorre las lineas
en orden y conserva el primer diagnostico causal reconocido de:

- CMake;
- clang, clang-cl o MSVC mediante sus formas `: error:` y `: error `;
- `lld-link` y otros errores fatales;
- Ninja mediante `ninja: error:`.

Para CMake conserva tambien la primera linea explicativa. El detalle completo,
incluido el codigo de salida, se limita a 800 caracteres; `full.log` continua
siendo la evidencia integra. Cuando no existe un diagnostico reconocible se
preserva exactamente el comportamiento anterior: `exit code N`.

Cancelacion, comandos no iniciados y comandos exitosos no cambian.

## Validacion ejecutada

Prueba dirigida de resultados de infraestructura:

```text
python -m unittest tests.compiler-suite.controller.tests.test_nscbuilder3_command_cancellation
Ran 5 tests in 0.000s
OK
```

La matriz dirigida cubre `lld-link`, CMake, MSVC, fallback sin diagnostico,
cancelacion activa y cierre terminal de comandos no iniciados.

Suite Python completa de infraestructura:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 275 tests in 27.876s
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

## Limite de la afirmacion

La perdida estatica del diagnostico queda corregida y probada. No se repitio
la compilacion historica ni se afirma que sus errores de enlace sigan presentes
en la revision actual. La corrida funcional integral permanece
`PENDING_USER_RUN`; la tarea persistente sigue activa hasta demostrar cero
errores inesperados en la matriz ejecutable acordada.
