# Integración de la laptop con correcciones posteriores

Este documento conserva el estado histórico de la copia de integración
`A:/nsc-fixes-integracion-actual` en las revisiones indicadas abajo. Las
referencias a trabajo pendiente o no aplicado corresponden a ese momento;
no describen necesariamente el estado actual de `development-fixes-limitaciones`.
Las pruebas registradas no certifican el HEAD de esa rama.

La integración parte de `2e303db8229a2fb834343a896ad15cbfaf3c1beb` y
`20d9ffad5b7dec62ed859ad71feaf17e129f2d85`. Los cinco conflictos de
restricciones genéricas se resolvieron conservando la resolución de identidad
abierta de la rama principal, la identificación de parámetros directos por
posición de la laptop y el recorrido posterior de seguridad `Send`/`Sync`.

## Validación dirigida

En `A:/nsc-fixes-evidencia/integration-incremental` se enlazó un compilador
aislado. El inventario `integration-dependencies.txt` compara los cambios de
ambas copias y usa las dependencias registradas por Ninja: se recompilaron 302
objetos afectados del objetivo `nsc`, se conservaron los restantes y se
reconstruyeron las bibliotecas afectadas. `results.json` y `links.json` guardan
comandos y resultados. Las rutas de SDK y temporales se configuraron en A.

Pasaron las pruebas dirigidas registradas en `test-results.json` y
`extra-test-results.json`: restricciones genéricas de ambas ramas, conversiones,
índices y selección con máscaras SIMD, fuentes externas de proyecto, seguridad
heredada `Send`/`Sync`, punteros compartidos con SDK binario, campos `ref` entre
bibliotecas y ruta de runtime de AddressSanitizer. Los logs individuales están
junto a esos archivos. No se ejecutó la batería completa.

## Límites

Los objetos no afectados se reutilizaron del build de
`A:/nsc-devel-fixes`. La selección depende de su base de dependencias y de su
estado previo; no equivale a una compilación limpia ni certifica todos los
comportamientos. Se recompilaron las unidades afectadas por las diferencias de
archivos rastreados de la copia principal, incluidos sus cambios sin registrar;
esos cambios no se incorporaron a esta rama de integración. El SDK binario de
prueba procede del build aislado anterior y no demuestra por sí solo un SDK
reconstruido desde esta revisión. Los archivos sin registrar de la copia
principal permanecen fuera de este commit y deben preservarse al aplicar la
integración allí, en particular `src/Frontend/MIR/Internal/MirNodeMachine.cpp`.

La integración no demuestra seguridad concurrente completa de las familias de
punteros ni resuelve sus pendientes. Daikon e interoperabilidad C/C++ permanecen
fuera del alcance; no se modificaron sus builds ni se realizó despliegue.

## Actualización con correcciones posteriores de punteros

La integración se actualizó con los commits de la rama de punteros desde
`8dc2a1720` hasta `3e3d83d30`. El merge en la copia aislada no produjo conflictos.
Para validarlo se reutilizaron los objetos de la integración anterior y se
recompilaron incrementalmente 191 unidades afectadas por los cambios. Comandos
y resultados están en `A:/nsc-fixes-evidencia/integration-incremental-update/`
(`plan.json`, `results.json` y `links.json`). La dependencia del encabezado
compartido incluyó la compilación de `src/Interop/CHeaderGenerator.cpp` en esa
carpeta aislada; no se modificó esa fuente ni se usaron builds de interop.

Pasaron las pruebas dirigidas de restricciones de ambas ramas, tres operaciones
SIMD, fuentes externas, `Send`/`Sync` heredado, punteros con SDK binario,
observación de StrongCount y una selección de vistas desde fuentes. Sus comandos
y logs están en `test-results.json`. También pasó el control dirigido de ruta
de runtime de AddressSanitizer (`asan-runtime-path.log`); ese control no prueba
toda la instrumentación ni todos los errores de memoria. No se ejecutó la
batería completa.

Los objetos no afectados y el SDK binario se reutilizaron de builds anteriores.
El SDK incluye un archivo con cambios concurrentes sin registrar y la copia
principal conserva sus archivos pendientes, incluido `MirNodeMachine.cpp`.
La actualización no certifica un árbol limpio ni todos los contratos de las
familias de punteros. No se ha aplicado a la rama principal ni enviado al remoto.

## Integración del caso 73 y observador débil

El commit `82f56e06c69fb59e4f2db4a7cbf7983ca174d0bd`, publicado en
`origin/development-fixes-73-run`, se integró en `4d7fde426`. Después se
integraron los commits de escritorio `46a58d434` y `daab4eb89` en
`a148d75e5`. Los merges en esta copia aislada no presentaron conflictos. El
`CMakeLists.txt` combinado incluye el evaluador y sus pruebas; los cambios
sin registrar del archivo homónimo de la copia principal no se incorporaron.

Se preparó `A:/nsc-fixes-evidencia/integration-incremental-73/bin/nsc.exe`
recompilando 72 unidades afectadas sobre los objetos de integración previos.
El nuevo `ComptimeFunctionEvaluator.cpp` se compiló desde esta copia con la
configuración del objetivo frontend. El plan, comandos y resultados están en
`integration-incremental-73/plan.json`, `results.json` y `links.json`. Una
dependencia del objetivo `nsc` volvió a compilar `src/Interop/CHeaderGenerator.cpp`
en A; no se modificó esa fuente ni se usaron builds de interop.

Pasaron las pruebas dirigidas `comptime_calls.py`, `shared_weakcount_during_view.py`
desde fuentes y con SDK binario, y `shared_field_method_borrow.py` con SDK
binario. El registro de comandos, códigos y logs está en
`integration-incremental-73/directed-results.json`. La prueba de #run registró
116 invocaciones entre compilaciones y ejecuciones. No se ejecutaron los
objetivos unitarios de #run en este build ni la batería completa.

El enlace reutiliza objetos de compilaciones anteriores, que incorporaron
cambios concurrentes sin registrar de la copia principal. El SDK binario
`shared-weakcount-sdk` también incluye uno de esos archivos. Estas pruebas no
certifican el árbol limpio ni el SDK de la revisión de integración. Los límites
funcionales de #run figuran en `73-run-laptop.md`; las pruebas no acreditan una
evaluación general del lenguaje ni los casos pendientes de punteros. La
reproducción del campo predeterminado y las de finally suspendido siguen abiertas.
La integración continúa separada de la rama principal y no se ha enviado ni
desplegado.
