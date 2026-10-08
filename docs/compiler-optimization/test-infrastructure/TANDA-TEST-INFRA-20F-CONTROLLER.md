# Tanda 20F — Controlador de estado para la GUI

## Propósito

Separar el estado y las reglas de composición de los widgets PySide6. La GUI
no decide si un artefacto es válido: solicita planes al controlador y sólo
habilita operaciones que éste puede respaldar con recibos vigentes.

## Implementación

`_nscbuilder2/controller.py` centraliza:

- creación y aceptación de planes de NSC;
- creación y aceptación de planes de NSLink, NSASM y NSDBG;
- carga y revalidación de recibos persistentes;
- bloqueo de pruebas, staging y deploy sin NSC vigente;
- bloqueo de herramientas seleccionadas sin recibo vigente;
- staging junto al `nsc.exe` construido en A:;
- creación del plan de deploy explícito.

No contiene PySide6, estado global, recursión ni rutas de herramientas de
build. Esto permite probar las invariantes sin abrir una ventana.

## Validación

```text
python -m py_compile _nscbuilder2\controller.py _nscbuilder2\__init__.py
python -m unittest tests.harness.tests.test_nscbuilder2
Ran 11 tests in 0.397s
OK
```

No se construyeron productos ni se ejecutó deploy.
