# Tanda 10: identidad técnica del compilador

## Problema

La pantalla humana de nsc --version ahora incluye copyright y puede localizarse. La extensión
seguía esperando el formato técnico histórico y marcaba NSC 2.0 compatible como incompatible.

## Solución

- Se agrega --print-compiler-identity.
- La salida usa el encabezado versionado NSCOMPILER 1 y campos clave=valor.
- Publica versión completa, release, lenguaje, stage y hash de build.
- No contiene banner, copyright ni texto localizado.
- --version permanece orientado a personas y no cambia.

## Archivos

- include/nativesharp/Driver/Driver.h
- src/Driver/Driver.cpp
- tests/unit/CompilerIdentityCommandTests.cpp
- CMakeLists.txt

## Mejora

Las integraciones dejan de analizar texto humano y pueden cruzar una identidad estable con
NSTOOLCHAIN 2.

## Verificación

Construcción Debug/O0 y ejecución exclusiva de nsc_compiler_identity_command_tests, seguida del
target nsc. Sin baseline ni CTest general.

Se intentó inicialmente nsc_driver_tests, pero ese target contiene 233 checks y terminó con 15
fallos antiguos de enlace PE/lld-link no relacionados. La identidad nueva no apareció entre los
fallos. Para no mezclar contratos ni seguir gastando CPU, la verificación se aisló en el target
indicado arriba.
