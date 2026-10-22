# Compatibilidad del ejecutor de comandos de proyecto

## SCOPE LOCK

- Objetivo: restaurar el contrato histórico de tres argumentos de `DriverExecutor` sin perder el manifiesto
  preparado usado por la ruta real del compilador.
- Naturaleza: corrección no funcional de compatibilidad de fuente introducida por la reutilización de digests.
- Revisión fallida: `6bcfca063`.
- Productor responsable: alias público `DriverExecutor`.
- Archivos de producto autorizados: `ProjectCommands.h` y `ProjectCommands.cpp`.
- Protegidos: pruebas, fixtures, CMake, NSC Builder, sintaxis, semántica, diagnósticos, CLI, ABI del lenguaje,
  NSMETA, HIR, MIR, LLVM IR y artefactos emitidos.
- Validación autorizada: limpieza, compilación y ejecución de la batería unitaria completa.
- Prohibido: pruebas externas, minibaseline, baseline, instalación y deploy.
- Terminación: los consumidores históricos y la ruta preparada compilan, y la batería unitaria completa pasa.

## Evidencia

La construcción limpia de `nsc_component_tests` se detuvo en dos consumidores. Las lambdas de tres argumentos de
`DriverProject497To616Module.cpp:24` y `DriverRuntime097To192Module.cpp:195` no podían convertirse al alias
`DriverExecutor`, que exigía un cuarto `CompilationInputManifest*`.

El commit `c12060aa1` sustituyó la firma histórica del alias al introducir la reutilización del manifiesto. La ruta
real de `Driver::runWithManifest` sí proporciona los cuatro argumentos, mientras que los consumidores públicos
anteriores sólo requieren delegar comandos y no participan en la reutilización interna.

## Corrección

Se restaura `DriverExecutor` con sus tres argumentos originales y se declara `PreparedDriverExecutor` para la ruta
interna que transporta el manifiesto. `runProjectCommand` ofrece ambas fronteras. La variante histórica delega en
la preparada mediante un adaptador que ignora únicamente el puntero que ese contrato nunca recibió.

## Contratos conservados

- Los consumidores históricos conservan nombre, firma y comportamiento.
- `Driver::runWithManifest` conserva la entrega del manifiesto preparado al compilador anidado.
- No cambian argumentos CLI, salidas, diagnósticos, códigos de retorno ni acciones de proyecto.
- No cambian sintaxis, semántica, ABI del lenguaje, NSMETA, IR ni artefactos emitidos.
- No se modifica ninguna prueba.

## Validación

Estado actual: `CORRECCIÓN COMPILADA, EJECUCIÓN UNITARIA COMPLETA PENDIENTE`.

Se limpió `build-compiler-suite-v3/debug`: Ninja eliminó 302 archivos y terminó con código 0. En la reconstrucción
desde cero, `DriverProject497To616Module.cpp` y `DriverRuntime097To192Module.cpp` compilaron en los pasos 295 y
296. La incompatibilidad de sus lambdas de tres argumentos no volvió a aparecer. `ProjectCommands.cpp`, que usa
la frontera preparada, también compiló en el paso 684.

La construcción avanzó hasta 758 de 784 objetos y se detuvo por una causa independiente: una macro de directorio
de import libraries no definida al compilar `WindowsRuntimeImports.cpp`. Por ello la ejecución unitaria completa
continúa pendiente; esa causa no pertenece a esta hipótesis ni a este commit.
