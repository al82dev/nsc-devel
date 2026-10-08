# Tanda 3 — Proceso, sandbox y reportes crudos

## Objetivo

Crear las fronteras de ejecución de procesos, aislamiento en disco y evidencia sin construir comandos de NSC ni emitir todavía un veredicto de prueba.

## Archivos creados

- `tests/harness/nsc_test/process.py`
- `tests/harness/nsc_test/workspace.py`
- `tests/harness/nsc_test/report.py`
- `tests/harness/tests/test_execution_infrastructure.py`

## Clases

- `ProcessRequest`: comando, cwd, timeout, stdin y ambiente.
- `ProcessResult`: salida completa, código, duración y timeout.
- `ProcessExecutor`: ejecución sin shell y terminación del árbol propio al expirar.
- `CaseWorkspace`: staging aislado en `A:` y mapeo origen/destino.
- `TestRunRecord`: asociación entre caso y proceso.
- `ReportWriter`: `summary.json` y `report.log` escritos atómicamente.

## Decisiones

- Un proceso se inicia sin shell.
- Cada proceso obtiene un grupo independiente.
- Un timeout termina solamente el árbol iniciado por el caso.
- El borrado de workspaces usa una worklist postorden explícita, no recursión.
- Workspaces y reportes rechazan raíces fuera de `A:`.
- `ProcessResult` no finge ser un veredicto; la expectativa se comparará en la Tanda 4.

## Validación ejecutada

- Captura simultánea de stdout, stderr y código distinto de cero.
- Timeout y terminación acotada.
- Copia y limpieza del sandbox.
- Escritura y lectura del reporte JSON y log completo.

```text
python -m unittest discover -s tests\harness\tests -p test_*.py -v
Ran 10 tests in 0.573s
OK
```

La inspección posterior confirmó cero directorios `_test-harness-*` y cero `__pycache__` bajo el harness.

## Comportamiento preservado

No se ejecuta NSC, no se modifica el CLI de selección, no se modifica CMake/CTest y no se escribe en `C:`.

## Pendiente deliberado

La construcción de comandos de NSC, la comparación de expectativas y la integración con el CLI pertenecen a la Tanda 4.
