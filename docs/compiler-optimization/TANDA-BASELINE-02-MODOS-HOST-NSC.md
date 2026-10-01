# Tanda baseline 02 — Modos de construcción del host nsc

## Objetivo

Permitir elegir desde `nscbuilder` cómo se construye el compilador nativo sin confundir esa
decisión con el enlazador que NativeSharp usa para sus programas.

## Controles nuevos

- Enlace LLVM del host:
  - `STATIC`: modo predeterminado, autónomo y certificable.
  - `DYNAMIC`: modo de desarrollo que enlaza `LLVM-C.lib` y coloca `LLVM-C.dll` junto a
    `nsc.exe`.
- Optimización del host: `O0`, `O1`, `O2` u `O3`.

Los perfiles O0–O3 de fixtures continúan siendo una selección independiente. LLD-link/NSLink
continúan indicando el enlazador de los productos NativeSharp, no el enlace interno de LLVM.

## Contratos de seguridad

- El runtime MSVC permanece estático (`MultiThreaded`, equivalente a `/MT`) en ambos modos.
- El modo dinámico verifica que `nsc.exe` importe `LLVM-C.dll` pero no MSVCP, VCRUNTIME ni UCRT
  dinámica.
- El modo estático sigue prohibiendo todos esos imports, incluido LLVM-C.
- Al volver de dinámico a estático, el posbuild elimina `LLVM-C.dll` del directorio de salida.
- Gate 0 sigue construyéndose en `STATIC`; el selector dinámico pertenece a desarrollo.

## Archivos modificados

- `CMakeLists.txt`
- `cmake/VerifyNativeSharpStaticHostRuntime.cmake`
- `nscbuilder.py`
- `tests/selfhost/test_bootstrap_infrastructure.py`

## Archivo nuevo

- Este documento, para mantener en el commit la explicación, los contratos y la validación de la
  tanda.

## Mejora

- Los ciclos de desarrollo pueden evitar el enlace de los componentes estáticos de LLVM cuando
  no se necesita producir un paquete certificable.
- La optimización del compilador deja de estar fijada implícitamente por `Release`.
- El cambio de modo es explícito, visible y no deja DLL residuales al regresar al paquete estático.

## Validación específica

- Dos pruebas unitarias dirigidas del plan CMake: aprobadas.
- Configuración y construcción de `nsc` en modo LLVM dinámico: aprobadas.
- Ejecución `nsc --version` dinámica: aprobada.
- Contrato dinámico LLVM + MSVC estático: aprobado.
- Restauración, construcción y ejecución de `nsc` estático: aprobadas.
- Contrato autónomo estático: aprobado.
- Verificación del comando O0: contiene `/Od`; el árbol quedó restaurado a `STATIC/O2`.
- `LLVM-C.dll` residual después de restaurar estático: inexistente.
- Baseline completo: no ejecutado.
- `C:\native-sharp`: no tocado.

