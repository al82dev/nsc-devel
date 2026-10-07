# Tanda correctiva 15: resolución de NSLink en CMake y NSC Builder

## SCOPE LOCK

- Objetivo: resolver NSLink tanto en CMake por consola como desde la GUI de NSC Builder.
- Naturaleza: corrección de infraestructura.
- Archivos funcionales autorizados: `CMakeLists.txt` y `nscbuilder.py`.
- Superficies protegidas: clases C++, compilador, lenguaje, pruebas históricas, instalación en `C:` y baseline.
- Comportamiento inmutable: `nsc.exe` se construye con clang-cl y lld-link; NSLink sólo se entrega al paso
  generado que construye productos NativeSharp.
- Acciones prohibidas: modificar el backend, desplegar, instalar, ejecutar baseline u otras matrices.
- Criterio de terminación: ambas entradas resuelven un `nslink.exe` existente sin depender de una ruta
  guardada obsoleta.

## Causa raíz

La GUI conservaba una ruta NSLink persistida aunque el archivo hubiera desaparecido. Además, al seleccionar
Debug sustituía las salidas históricas todavía válidas por `bin/debug/nslink.exe`, incluso cuando esa salida
modular aún no existía. CMake sólo buscaba NSLink en `PATH` si no recibía el ejecutable explícito.

## Cambios

- CMake conserva `NSC_NSLINK_EXECUTABLE` y agrega `NSC_NSLINK_ROOT`.
- También reconoce `NSC_LINKER`, `NSLINK_EXE` y `NSLINK_ROOT`.
- La búsqueda incluye las salidas modulares y las salidas históricas del repositorio hermano `nslink-devel`.
- La GUI prefiere `bin/<config>/nslink.exe`, recurre a `build-<config>/bin/nslink.exe` y reemplaza una ruta
  persistida inexistente por una salida válida de la configuración seleccionada.

## Archivos

- `CMakeLists.txt`: contrato de resolución para ejecución directa de CMake.
- `nscbuilder.py`: selección y recuperación de rutas desde la GUI.

## Verificación

El commit se crea antes de ejecutar cualquier comprobación. No se ejecuta baseline ni se modifica `C:`.
