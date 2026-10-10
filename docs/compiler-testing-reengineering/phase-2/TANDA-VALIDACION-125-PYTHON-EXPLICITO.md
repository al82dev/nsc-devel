# Tanda Validacion 125: interprete Python explicito

Fecha: 2026-10-10

## Scope lock

Esta tanda fija exclusivamente la autoridad Python de la infraestructura
nueva. No hardcodea una instalacion, no cambia controladores, CMake, producto,
casos, manifiestos, oraculos ni la bateria historica. No configura, compila o
ejecuta producto, no ejecuta baseline y no toca la unidad `C:`.

## Defecto demostrado

Los controladores CTest se ejecutan mediante `Python3_EXECUTABLE`, pero el
comando de Builder 3 no establecia esa variable. Un arbol CMake reutilizado
podia conservar un interprete descubierto por una sesion anterior, distinto
del Python que estaba ejecutando Builder 3.

El problema afecta las 118 invocaciones de controladores que comparten esa
autoridad; no es necesario modificar cada consumidor.

## Correccion causal

`discover_build_tools` incorpora el `sys.executable` real como herramienta
descubierta y `configure_commands` transmite su ruta normalizada mediante:

```text
-DPython3_EXECUTABLE=<interprete que ejecuta Builder 3>
```

No existe una ruta fija para Python. Una prueba aislada verifica el
descubrimiento con dobles de las demas herramientas y la prueba de comandos
verifica la propagacion tanto para Debug como para Release.

## Validacion ejecutada

```text
python -m unittest tests.compiler-suite.controller.tests.test_nscbuilder3_commands tests.compiler-suite.controller.tests.test_nscbuilder3_paths
Ran 17 tests in 0.116s
OK
```

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 273 tests in 28.120s
OK
```

No se configuro ni compilo la suite.

## Limite de la afirmacion

El interprete de los controladores queda ligado estaticamente a la sesion de
Builder 3. La corrida funcional integral permanece `PENDING_USER_RUN`; la
tarea persistente sigue activa hasta obtener cero errores inesperados en la
matriz ejecutable acordada.
