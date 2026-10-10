# Tanda Validacion 126: runtime del compilador seleccionado

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente la autoridad con la que Builder 3 selecciona
`NativeSharp.Runtime.Win32.lib` al configurar `tests/compiler-suite`. No cambia
CMake, el compilador, NSLink, deploy, manifiestos, casos, oraculos ni la bateria
historica. No configura, compila o ejecuta producto, no ejecuta baseline y no
toca la unidad `C:`.

## Defecto demostrado

Validacion 122 elimino la caida a `Program Files`, pero eligio el runtime desde
`BuilderConfig.install_prefix`. Esa fuente no demuestra identidad con el
`nsc.exe` seleccionado. En el arbol auditado existian dos productores distintos:

```text
A:/nsc-devel/build-clang/generated/NativeSharp.Runtime.Win32.lib
  LastWriteTimeUtc: 2026-10-10 05:44:01
A:/native-sharp-3.0-baseline/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib
  LastWriteTimeUtc: 2026-10-07 18:47:05
```

Ambos artefactos tenian 31,988 bytes, pero fecha y productor distintos. El
tamanio no acredita identidad. El comando podia combinar el NSC vivo con un
runtime instalado anterior.

## Causa

`configure_commands` construia la ruta desde `install_prefix`, aunque la
identidad del compilador se establece mediante `nsc_executable` y, cuando se
proporciona, `build_id_header`. La invariante violada era:

```text
nsc.exe + BuildId.h + NativeSharp.Runtime.Win32.lib
pertenecen al mismo directorio generated del mismo productor
```

## Correccion causal

La funcion pura `compiler_runtime_library` aplica una sola autoridad:

- con `build_id_header` explicito, asciende desde
  `generated/nativesharp/Basic/BuildId.h` hasta ese mismo `generated`;
- sin encabezado explicito, deriva `<build>/generated` desde
  `<build>/bin/nsc.exe`;
- no consulta `install_prefix` y no tiene fallback a una instalacion anterior.

`configure_commands` transmite la ruta resultante a
`NSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY` tanto en Debug como en
Release.

## Validacion ejecutada

Prueba dirigida del controlador:

```text
python -m unittest tests.compiler-suite.controller.tests.test_nscbuilder3_commands
Ran 6 tests in 0.008s
OK
```

Suite Python completa de infraestructura:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 273 tests in 28.240s
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

Las pruebas de comandos demuestran dos modos ligados al `generated` del NSC y
una ruta explicita ligada al `generated` del `BuildId.h`. No queda fallback al
prefijo instalado en esta seleccion.

## Limite de la afirmacion

La correccion queda verificada de forma estatica y por pruebas puras del
controlador. No se configuro, compilo o ejecuto la suite; la validacion
funcional integral permanece `PENDING_USER_RUN`. La tarea persistente sigue
activa hasta demostrar cero errores inesperados en la matriz ejecutable
acordada.
