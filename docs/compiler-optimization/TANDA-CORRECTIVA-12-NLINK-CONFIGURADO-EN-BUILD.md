# Tanda correctiva 12: NSLink configurado en el build

## Objetivo

Eliminar la dependencia accidental de la variable de entorno `NSC_LINKER` durante el post-build
de `nsc.exe`.

## Cambios

- CMake recibe y conserva `NSC_NSLINK_EXECUTABLE` como ruta de herramienta configurada.
- Si se configura manualmente, CMake puede adoptar `NSC_LINKER` o localizar `nslink.exe` en
  `PATH`, pero falla temprano si no existe una seleccion valida.
- El comando que genera `NativeSharp.Runtime.Win32.lib` recibe la ruta configurada de forma
  explicita.
- `nscbuilder.py` transmite a CMake el NSLink elegido en la interfaz, por lo que el build ya no
  depende de que el shell conserve una variable temporal.

## Archivos

- `CMakeLists.txt`
- `nscbuilder.py`

## Mejora

Reconstruir `nsc.exe` directamente o mediante NSC Builder usa la misma herramienta seleccionada.
El post-build deja de fallar con `NSLink was not found` cuando cambia el proceso o el entorno.
