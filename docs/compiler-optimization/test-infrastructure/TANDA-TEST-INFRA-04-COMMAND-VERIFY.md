# Tanda 4 — Comandos y verificación de expectativas

## Objetivo

Cerrar la primera ruta completa manifiesto → sandbox → proceso → verificación → reporte sin migrar todavía una prueba del compilador.

## Archivos creados

- `tests/harness/nsc_test/command.py`
- `tests/harness/nsc_test/verify.py`
- `tests/harness/tests/test_verification.py`

## Archivos modificados

- `tests/harness/nsc_test/model.py`
- `tests/harness/nsc_test/report.py`
- `tests/harness/nsc_test/__init__.py`
- `tests/harness/run_nsc_tests.py`
- `.gitignore`

## Clases

- `NscCommandBuilder`: fija el ejecutable NSC, sustituye placeholders y aplica O0.
- `TextExpectation`: comparación `exact`, `contains` o `not-contains` elegida por el manifiesto.
- `ResultVerifier`: código, streams, artefactos y resultado de producto.
- `TestResult`: veredicto separado del resultado crudo del proceso.
- `VerificationFailure`: causa estructurada de fallo.

## Contrato de NSLink

La ruta seleccionada se valida como archivo y se transmite al proceso exclusivamente mediante `NSC_LINKER`, que es el contrato consumido por `src/Linker/LldLinker.cpp`. Los casos `link-run` y `emit-library` rechazan una ejecución sin NSLink explícito.

## Placeholders

- `{source:N}`: fuente N ya copiada al sandbox.
- `{project}`: proyecto ya copiado al sandbox.
- `{workspace}`: directorio aislado del caso.

Un placeholder desconocido es error. Los argumentos nunca pueden sustituir el primer elemento del comando, que siempre es el NSC congelado.

## Validación ejecutada

- Ruta fija de NSC y NSLink.
- Sustitución de fuente sin duplicarla.
- Rechazo de enlace sin NSLink.
- Comparación de salida y artefactos.
- Acumulación de fallos independientes.
- Ejecución CLI controlada con reporte verificado.

```text
python -m unittest discover -s tests\harness\tests -p test_*.py -v
Ran 15 tests in 0.785s
OK
```

La inspección posterior confirmó cero directorios `_test-*` y cero `__pycache__` residuales. `/_test-harness/` quedó excluido de Git porque es almacenamiento operativo, no fuente.

## Comportamiento preservado

No se migra ni modifica ninguna prueba actual. La ejecución integrada se valida con un proceso controlado que no es NSC.

## Pendiente deliberado

La Tanda 5 crea el registro C++ consolidado. El primer caso real de NSC se migra hasta la Tanda 7.
