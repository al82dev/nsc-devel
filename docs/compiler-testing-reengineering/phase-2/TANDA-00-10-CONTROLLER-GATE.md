# Tanda 00.10 — Controlador y gate end-to-end de infraestructura

## Resultado

`controller/run_suite.py` conecta el inventario, manifiesto, selector, scheduler, workspaces, ejecutor host,
eventos y reportes. Requiere rutas explícitas y no busca otra copia del ejecutable.

`HostExecutor` invoca un solo caso del binario consolidado, conserva stdout/stderr en el workspace y exige:

- exactamente una línea JSON no vacía;
- identidad de caso, contrato y componente igual al manifiesto seleccionado;
- estado `PASS` o `FAIL` con evidencia coherente;
- código de salida 0 para `PASS` y 1 para `FAIL`;
- timeout derivado de la clase declarada.

Una divergencia del protocolo o un fallo al lanzar el proceso es `INFRASTRUCTURE_ERROR`, nunca fallo del lenguaje.

## Gate sintético

`controller/integration/run_infrastructure_gate.py` ejecuta tres casos sintéticos declarados en
`fixtures/infrastructure`. Comprueba tres resultados `PASS`, ocho eventos progresivos y los cuatro artefactos de
corrida. El temporal vive bajo el build de `A:` y se elimina al finalizar.

Este gate valida el framework, no contratos de NSC. No ejecuta el compilador, NSLink ni pruebas históricas.
