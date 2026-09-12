# Limpieza segura de artefactos en NSC Builder / Safe artifact cleanup in NSC Builder

## Resumen / Summary

NSC Builder incorpora una acción manual **Limpiar** para retirar los árboles de construcción que
quedan después de compilar el compilador, ejecutar pruebas o certificar un baseline. La acción no se
ejecuta automáticamente: el usuario decide cuándo recuperar el espacio y siempre recibe una lista
exacta de los directorios antes de confirmar.

NSC Builder now provides a manual **Limpiar** action to remove build trees left after building the
compiler, running tests, or certifying a baseline. Cleanup never runs automatically: the user decides
when to reclaim the space and always sees the exact directory list before confirming.

## Archivos modificados / Modified files

- `nscbuilder.py`
  - Define el inventario de árboles de construcción conocidos.
  - Valida rutas configuradas sin permitir que la limpieza salga del repositorio.
  - Ejecuta la eliminación en un worker separado para no congelar la interfaz.
  - Añade el botón **Limpiar**, confirmación previa, progreso, cancelación y reporte de errores.
  - Defines the known build-tree inventory.
  - Validates configured paths without allowing cleanup to escape the repository.
  - Runs deletion in a separate worker so the interface remains responsive.
  - Adds the **Limpiar** button, prior confirmation, progress, cancellation, and error reporting.

## Alcance de la limpieza / Cleanup scope

La acción reúne tres rutas configurables de la interfaz —build del compilador y los dos builds de
CTest—, los nombres históricos conocidos `build-*` y el área temporal
`_nscbuilder/work/tests`. Sólo se acepta un árbol `build-*` que sea hijo directo de la raíz del
repositorio, o exactamente el área temporal del runner. Las rutas duplicadas se eliminan del plan y
las rutas inexistentes simplemente no se presentan.

The action combines the three configurable UI paths—the compiler build and both CTest builds—the
known historical `build-*` names, and the `_nscbuilder/work/tests` temporary area. A target is
accepted only when it is a direct `build-*` child of the repository root or exactly the runner's
temporary area. Duplicate paths are removed from the plan, and paths that do not exist are simply
not shown.

La validación resuelve primero cada ruta. Por ello, una ruta absoluta externa o un enlace/junction
que termine fuera del repositorio se rechaza antes de mostrar la confirmación. Si una de las tres
rutas configurables es inválida, no se ejecuta una limpieza parcial: la interfaz informa qué ruta
debe corregirse.

Validation resolves every path first. Therefore, an external absolute path or a link/junction that
lands outside the repository is rejected before confirmation is shown. If one of the three
configurable paths is invalid, no partial cleanup runs; the interface reports which path must be
corrected.

## Datos conservados / Preserved data

La acción no incluye ni puede alcanzar:

- `_abi`, incluidos gates, archivos y baselines certificados;
- `_nscbuilder/history`, incluidos `metadata.json`, consolas e informes;
- fuentes, pruebas, documentación o repros bajo `_targeted`;
- la instalación de producto en `C:\native-sharp`;
- productos de release fuera de los árboles de construcción enumerados.

The action does not include and cannot reach:

- `_abi`, including gates, archives, and certified baselines;
- `_nscbuilder/history`, including `metadata.json`, consoles, and reports;
- sources, tests, documentation, or repros under `_targeted`;
- the product installation at `C:\native-sharp`;
- release products outside the enumerated build trees.

## Flujo de interfaz / UI flow

`MainWindow.start_cleanup()` comprueba que no exista otra ejecución ni una lectura CTest activa,
construye el plan seguro y muestra todos los destinos. Tras la confirmación deshabilita las acciones
que podrían iniciar otro trabajo y mueve un `CleanupWorker` a un `QThread`.

`MainWindow.start_cleanup()` verifies that no other run or CTest inventory read is active, builds the
safe plan, and displays every target. After confirmation it disables actions that could start other
work and moves a `CleanupWorker` to a `QThread`.

`CleanupWorker.run()` elimina un árbol a la vez, publica cada ruta en la consola del compilador y
actualiza el progreso. Un error en un directorio queda registrado y no oculta los demás resultados.
La cancelación se observa entre árboles; nunca se abandona deliberadamente una eliminación a mitad
de una operación de sistema de archivos.

`CleanupWorker.run()` removes one tree at a time, publishes every path in the compiler console, and
updates progress. An error in one directory is recorded without hiding the remaining results.
Cancellation is observed between trees; a filesystem deletion is never deliberately abandoned in
the middle of an operation.

## Motivo / Rationale

Una limpieza automática al terminar un baseline eliminaría evidencia útil justo cuando puede ser
necesaria para diagnosticar un fallo. La acción manual conserva ese control, mientras que la lista
previa y la frontera estricta del repositorio hacen explícita una operación destructiva. Ejecutarla
fuera del hilo de la interfaz evita que borrar árboles grandes haga parecer que NSC Builder dejó de
responder.

Automatic cleanup after a baseline would erase useful evidence precisely when it may be needed to
diagnose a failure. A manual action preserves that control, while the preview and strict repository
boundary make a destructive operation explicit. Running it outside the UI thread prevents large
tree deletion from making NSC Builder appear frozen.

## Validación / Validation

Se validó la sintaxis completa de `nscbuilder.py` con `python -m py_compile`. No se ejecutó baseline,
no se inició una compilación y no se pulsó la acción destructiva durante esta entrega.

The complete `nscbuilder.py` syntax was validated with `python -m py_compile`. No baseline or build
was started, and the destructive action was not invoked during this delivery.

## Commit de referencia / Reference commit

### Título / Title

```text
Agrega limpieza segura a NSC Builder / Add safe cleanup to NSC Builder
```

### Descripción / Description

```text
ES:
- agrega una acción manual para borrar árboles de construcción y temporales
- restringe cada destino a ubicaciones seguras dentro del repositorio
- conserva baselines, historial, fuentes e instalación de producto
- mantiene la interfaz responsiva con progreso y errores por directorio

EN:
- add a manual action to remove build trees and temporary outputs
- restrict every target to safe locations within the repository
- preserve baselines, history, sources, and the product installation
- keep the UI responsive with progress and per-directory errors
```
