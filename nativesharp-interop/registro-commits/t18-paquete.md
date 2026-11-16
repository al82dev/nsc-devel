# T18-017 a T18-020: paquete de NSC en un entorno limpio

## Comprobación

Este cambio no toca producción. Se añade el fixture `pruebas-tanda-18-paquete` y el documento
[DISTRIBUCION-INTEROP-C-CPP.md](../DISTRIBUCION-INTEROP-C-CPP.md).

## Dependencias (T18-017)

`cmake --install` del build en un prefijo nuevo instala 116 archivos: `nsc.exe`, el runtime NativeSharp, las
bibliotecas de importación, las bibliotecas NativeSharp en fuente y los contratos. `llvm-readobj
--coff-imports` muestra que `nsc.exe` importa solo DLL del sistema (`KERNEL32`, `ADVAPI32`, `ntdll`, `VERSION`,
`OLEAUT32`, `SHELL32`, `ole32`): LLVM y Clang 23.1.0 están enlazados de forma estática. NSLink, `lld-link`,
`llvm-lib` y los encabezados de recursos de Clang no forman parte de la instalación de NSC.

## Entorno limpio (T18-018)

El fixture instala el build en `A:/temp` y copia `nslink.exe` y `lld-link.exe` junto a `nsc.exe`; más tarde
copia también `llvm-lib.exe`. De Clang toma solo `lib/clang/23/include`. Compila y ejecuta con un entorno de
`SystemRoot`, `SystemDrive`, `TEMP`, `TMP` y `PATH=C:\Windows\System32`.

| Caso | O0 | O2 |
|---|---|---|
| NativeSharp solo | 42 | 42 |
| Biblioteca C estática | 42 | 42 |
| Biblioteca C++ con STL y `--crt-startup` | 42 | 42 |
| DLL NativeSharp cargada por un programa C | 42 | 42 |

Los negativos fallan con su motivo:

- sin los encabezados de recursos de Clang;
- sin NSLink;
- sin `llvm-lib`, en una biblioteca estática. Con `llvm-lib.exe` junto a `nsc.exe`, la biblioteca tiene 3
  miembros.

Evidencia: `A:/temp/nsc-t18-paquete-_0auatp4`.

## Requisitos y versiones (T18-019, T18-020)

[DISTRIBUCION-INTEROP-C-CPP.md](../DISTRIBUCION-INTEROP-C-CPP.md) cubre:

- el orden en que NSC busca cada pieza, según el código;
- qué necesita cada tipo de programa;
- las versiones de cada componente;
- los comandos de reproducción de cada prueba integral y de la regresión.

## Límites

- El paquete probado arma NSLink, `lld-link` y `llvm-lib` a mano: la instalación de NSC no los incluye
  (T19-022).
- El toolset de MSVC y el SDK de Windows vienen de `A:/vc2026-runtime`; no se probó otra versión.
