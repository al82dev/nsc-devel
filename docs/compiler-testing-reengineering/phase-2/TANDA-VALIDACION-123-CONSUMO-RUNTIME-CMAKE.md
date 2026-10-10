# Tanda Validacion 123: consumo efectivo de la autoridad runtime

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente la declaracion CMake que recibia la ruta
runtime emitida por Builder 3. No modifica ninguna otra variable CMake, el
compilador, NSLink, deploy, manifiestos, casos, oraculos ni la bateria
historica. No configura, compila o ejecuta producto, no ejecuta baseline y no
toca la unidad `C:`.

## Defecto demostrado

Validacion 122 agrego al comando:

```text
-DNSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY=<ruta explicita en A:>
```

Sin embargo, `tests/compiler-suite/CMakeLists.txt` ejecutaba despues una
asignacion normal e incondicional del mismo nombre. Esa variable normal
sombreaba la entrada de cache creada por `-D` y restablecia el valor derivado
de `CMAKE_INSTALL_PREFIX`.

## Correccion causal

La unica asignacion responsable es ahora una entrada `CACHE FILEPATH` sin
`FORCE`. CMake conserva asi un valor proporcionado por el controlador y usa el
prefijo predeterminado solamente cuando el llamador no establecio la variable.

La prueba contractual lee el productor CMake real y exige esta declaracion; no
duplica el comportamiento del compilador ni crea un runner alterno.

## Validacion ejecutada

Prueba dirigida de infraestructura:

```text
python -m unittest tests.compiler-suite.controller.tests.test_nscbuilder3_commands
Ran 5 tests in 0.007s
OK
```

Suite Python completa de infraestructura:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 271 tests in 28.055s
OK
```

No se configuro ni compilo la suite. Los seis archivos protegidos conservan
los SHA-256 registrados en `CURSOR.json`.

## Limite de la afirmacion

La cadena estatica queda cerrada desde el argumento emitido por Builder 3
hasta la declaracion CMake que lo consume. La corrida funcional integral
permanece `PENDING_USER_RUN`; la tarea persistente sigue activa hasta obtener
cero errores inesperados en la matriz ejecutable acordada.
