# Validación 245 — Restauración de la construcción de NSC en Builder 3

## SCOPE LOCK

### Objetivo autorizado

Restaurar exclusivamente la forma en que `nscbuilder3.py` configura y construye el compilador NSC,
usando `nscbuilder.py` y `nscbuilder2.py` como autoridad funcional y el historial Git para atribuir
el diferencial.

### Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Código | `nscbuilder3.py` |
| Símbolos | `compiler_configure_commands`, `compiler_build_commands` |
| Documentación | Este documento y `phase-2/TRACKING.md` |
| Naturaleza | Restauración de infraestructura |

### Elementos protegidos

- `nscbuilder.py` y `nscbuilder2.py`.
- Código del compilador y archivos CMake.
- Rutas, versiones, dependencias y selección de toolchain.
- Modos `Debug` y `Release`, enlace host, optimización host y demás selecciones de Builder 3.
- Toda superficie ajena a la configuración y construcción del target `nsc`.

### Acciones prohibidas

- Configurar o compilar.
- Ejecutar binarios.
- Cambiar rutas, versiones, dependencias o herramientas externas.
- Revertir commits completos.
- Incorporar cambios ajenos presentes en el árbol de trabajo.

## Auditoría causal

El bloque histórico de `nscbuilder.py` configura el generador `Ninja` sin escribir
`CMAKE_MAKE_PROGRAM` y construye el árbol de configuración única sin pasar `--config`.
`nscbuilder2.py` no añade un segundo contrato para construir NSC.

El commit `c9a1c52cef3ab404d8d8ad63c33fe63769ef263e` creó Builder 3 e introdujo dos argumentos
adicionales en esa frontera:

1. `-DCMAKE_MAKE_PROGRAM=<ruta>` durante la configuración;
2. `--config <modo>` durante `cmake --build`.

Ambos son redundantes para el generador Ninja de configuración única y no pertenecen a la forma
preexistente usada para construir NSC. Ningún otro argumento del bloque difiere causalmente del
contrato anterior: `CMAKE_BUILD_TYPE` conserva la selección `Debug`/`Release` propia de Builder 3,
y las rutas y opciones del compilador permanecen sin cambios.

## Corrección

- Se retiró únicamente `CMAKE_MAKE_PROGRAM` de `compiler_configure_commands`.
- Se retiró únicamente `--config <modo>` de `compiler_build_commands`.
- No se modificó descubrimiento de herramientas, rutas, LLVM, NSLink, CMake del producto ni código
  del compilador.

## Validación

`CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE BUILD PENDIENTE DEL USUARIO`.

Por instrucción expresa no se configuró, compiló ni ejecutó ningún artefacto. La validación se
limitó a comparar los argumentos de Builder 3 contra la autoridad histórica y revisar el diferencial
exacto antes del commit.
