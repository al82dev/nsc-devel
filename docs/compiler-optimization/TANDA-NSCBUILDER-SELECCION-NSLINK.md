# Tanda — selección explícita de NSLink en NSCBuilder

Fecha: 2026-10-04

Rama: `development`

Estado: terminada

## Cambios

- La ruta predeterminada dejó de apuntar al árbol interno `build-debug`.
- Release usa `A:\nslink-devel\bin\release\nslink.exe`.
- Debug usa `A:\nslink-devel\bin\debug\nslink.exe`.
- La interfaz permite elegir Release o Debug y actualiza la ruta correspondiente.
- El botón `Examinar…` permite seleccionar cualquier otro `nslink.exe` sin modificar el script.
- La configuración y la ruta elegida se conservan en las preferencias de NSCBuilder.
- Las rutas antiguas de `build-debug`, `build-release` y del staging instalado se migran al nuevo
  destino publicado.
- El resumen de ejecución muestra tanto la configuración como la ruta exacta que se utilizará.

## Archivos modificados

- `nscbuilder.py`

## Archivos nuevos

- `docs/compiler-optimization/TANDA-NSCBUILDER-SELECCION-NSLINK.md`: expediente de esta tanda.

## Mejora

NSCBuilder ya no depende de la estructura privada del árbol CMake de NSLink. La selección es
visible, persistente y reemplazable desde la interfaz antes de iniciar una construcción o prueba.

## Validación

No se ejecutaron pruebas, compilaciones ni baseline por instrucción expresa del usuario.
