# Tanda Validacion 122: autoridad runtime de la suite en A

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente el comando con el que Builder 3 configura
`tests/compiler-suite`. No cambia CMake, el compilador, NSLink, deploy,
manifiestos, casos, oraculos ni la bateria historica. No configura, compila o
ejecuta producto, no ejecuta baseline y no toca la unidad `C:`.

## Defecto demostrado

Un reporte externo mostro que el comando generado no establecia
`NSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY`. Por ello, la definicion
de `tests/compiler-suite/CMakeLists.txt` la derivaba de
`CMAKE_INSTALL_PREFIX`, cuyo valor predeterminado producia:

```text
C:/Program Files (x86)/NativeSharpCompilerSuiteInfrastructure/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib
```

Ese resultado viola el aislamiento de Builder 3: la suite debe consumir el
runtime perteneciente al prefijo explicitamente seleccionado en `A:`.

## Correccion causal

`configure_commands` deriva ahora la ruta de runtime de
`BuilderConfig.install_prefix` y entrega a CMake exactamente:

```text
-DNSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY=A:/native-sharp-3.0-baseline/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib
```

La prueba contractual verifica esa ruta para Debug y Release, rechaza una
aparicion de `Program Files` y conserva la normalizacion a diagonales.

## Validacion ejecutada

Prueba dirigida de infraestructura:

```text
python -m unittest tests.compiler-suite.controller.tests.test_nscbuilder3_commands
Ran 4 tests in 0.011s
OK
```

Suite Python completa de infraestructura:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 270 tests in 28.643s
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

Los seis archivos protegidos conservan exactamente los SHA-256 registrados en
`CURSOR.json`.

## Limite de la afirmacion

La orden queda cerrada estaticamente y por su prueba contractual. No se
configuro ni compilo la suite despues del cambio; la validacion funcional
integral permanece `PENDING_USER_RUN`. La tarea persistente sigue activa hasta
obtener cero errores inesperados en la matriz ejecutable acordada.
