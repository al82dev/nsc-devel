# Tanda Validacion 124: artefactos LLVM C sin cache obsoleta

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente las dos busquedas CMake que derivan
`LLVM-C.lib` y `LLVM-C.dll` de `LLVM_DIR`. No cambia rutas, nombres,
toolchain, comandos, producto, casos, manifiestos, oraculos ni la bateria
historica. No configura, compila o ejecuta producto, no ejecuta baseline y no
toca la unidad `C:`.

## Defecto demostrado

`find_library` y `find_file` crean entradas de cache por omision. El arbol de
la suite es reutilizable y Builder 3 permite seleccionar `LLVM_DIR`; por tanto,
una reconfiguracion podia conservar las rutas resueltas para una toolchain
anterior aunque el comando transmitiera una autoridad LLVM distinta.

La consecuencia era una identidad mixta: headers y configuracion de la nueva
LLVM con biblioteca de importacion o DLL de la anterior.

## Correccion causal

Las dos busquedas usan ahora `NO_CACHE` y conservan `NO_DEFAULT_PATH`:

- solo buscan dentro de los directorios derivados del `LLVM_DIR` vigente;
- repiten la derivacion en cada configuracion;
- no consultan rutas globales ni conservan el resultado entre toolchains.

La prueba contractual exige `NO_CACHE` en ambas fronteras reales.

## Validacion ejecutada

```text
python -m unittest tests.compiler-suite.controller.tests.test_nscbuilder3_commands
Ran 6 tests in 0.007s
OK
```

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 272 tests in 28.180s
OK
```

No se configuro ni compilo la suite.

## Limite de la afirmacion

La seleccion estatica de los dos artefactos LLVM C queda ligada al
`LLVM_DIR` vigente. La corrida funcional integral permanece
`PENDING_USER_RUN`; la tarea persistente sigue activa hasta obtener cero
errores inesperados en la matriz ejecutable acordada.
