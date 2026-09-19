# Dependencias permitidas por el bootstrap de NativeSharp 2.0

## Semilla

- `nsc0`: compilador C++ congelado. Sólo produce `nsc1` o recupera la cadena.
- Toolchain C++: sólo reconstruye `nsc0`; no participa al construir `nsc2` o `nsc3`.

## Backend y plataforma

- LLVM 23.1 mediante el subconjunto congelado de su API C.
- `lld-link` como enlazador externo certificado.
- NSLink actual como producto hermano certificado.
- Windows SDK, UCRT y bibliotecas del sistema para Windows x64.

## Orquestación transitoria

- CMake y Ninja para construir la semilla C++ y pruebas existentes.
- Python 3 para orquestación, locks, reportes y gates mientras se completa el bootstrap.

## Reglas

- Toda herramienta se selecciona por ruta absoluta después de resolver la configuración.
- La versión y el SHA-256 de cada binario crítico se registran en `bootstrap.lock`.
- LLVM puede estar implementado internamente en C++, pero `nsc-next` sólo consume su API C.
- Una reescritura NativeSharp de NSLink, NSAsm u otra herramienta vive en un repositorio nuevo.
- No se permite ocultar un frontend, parser, semántica o backend alterno dentro de una dependencia.
