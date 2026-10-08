# Tanda 20D — Pruebas desacopladas y perfiles O0–O3

## Objetivo

Configurar y ejecutar pruebas sólo después de validar el recibo del compilador,
sin volver a configurar ni construir `nsc.exe`. Implementar O0, O1, O2, O3 y
la matriz completa sin ejecutar los perfiles prohibidos durante esta tarea.

## SCOPE LOCK

- Se modifica únicamente el harness v2 y la infraestructura Builder 2.
- No se modifican manifiestos, casos, entradas, expectativas ni el compilador.
- No se ejecuta la batería, baseline, O1, O2 ni O3.
- La suite especializada conserva su contrato O0; no se falsifica una matriz
  para suites cuyos comandos poseen otra configuración propia.

## Implementación

- `RunSelection` conserva una tupla explícita y sin duplicados de perfiles.
- `create_test_run_plan` valida primero el recibo y genera pasos de casos por
  perfil; el plan no contiene configure ni target `nsc`.
- El registro unitario se construye y ejecuta una sola vez porque prueba C++
  interno, no una optimización de código NativeSharp.
- Las suites especializadas se ejecutan una sola vez cuando O0 está incluido.
- `NscCommandBuilder` acepta el perfil seleccionado y sustituye cualquier flag
  O0–O3 previo por exactamente uno.
- `PipelineRunner` verifica el SHA-256 congelado antes del primer paso y al
  terminar, por lo que una reconstrucción o sustitución incidental invalida la
  corrida.

## Complejidad

La creación del plan cuesta `O(p)` para `p <= 4` perfiles. La sustitución del
flag recorre una vez la lista de argumentos del caso, `O(a)`, sin recursión ni
copias de árboles.

## Validación dirigida

```text
python -m unittest \
  tests.harness.tests.test_nscbuilder2 \
  tests.harness.tests.test_verification
Ran 16 tests in 0.562s
OK
```

Se comprobó estáticamente que el plan completo contiene cuatro ejecuciones de
manifiestos y no contiene configuración, CTest integral ni construcción de
NSC. No se ejecutaron los perfiles sobre el compilador real.
