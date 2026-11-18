# Identidad de argumentos anidados en punteros compartidos binarios

En `b6b2c3108`, una comprobación de `Arc<Arc<Safe>>` contra Core binario
fallaba al resolver `SharedAtomicControl.DropStrong<T>`. El control de la misma
prueba con fuentes era correcto; un Core binario anterior a la corrección de
receptores de solo lectura fallaba igual. Evidencia anterior en
`A:\nsc-fixes-evidencia\shared-readonly-clone-constraints-control-20261113.log`.

El volcado anterior conservado durante la investigación mostró que el
argumento `Safe` originalmente resuelto como `type:Probe::Safe` reaparecía
como nombre sin resolver con procedencias distintas en la especialización
anidada. La clonación iterativa omitía la copia de identidad y superficie para
un hijo ordinario. `cloneTypeWorkItem` aplica ahora la misma copia que la ruta
normal cuando el mapa de sustituciones está vacío; por lo tanto el subárbol
es una copia sin reemplazos. No cambia el formato de metadata.

`tests/integration/shared_nested_binary.py` compiló Core mínimo desde esta
copia y ejecutó consumidores con `Rc<Rc<T>>` y `Arc<Arc<T>>`, promoción de
débiles y valores homónimos en los módulos productor y consumidor. Las ocho
combinaciones de productor y consumidor O0/O2 devolvieron 42. Comandos y
resultados: `A:\nsc-fixes-evidencia\nested-shared-binary-modules-20261113.log`.

La prueba de restricciones de hilos pasó contra Core binario y fuente,
incluidas las cuatro variantes anidadas antes fallidas y los rechazos de
payloads inseguros. La prueba de alias anidados contra bibliotecas estática y
dinámica y la prueba de visibilidad entre proyectos pasaron con sus controles.
Los registros están en `A:\nsc-fixes-evidencia\nested-arc-constraints-after-20261113.log`,
`nested-generic-source-constraints-20261113.log`,
`nested-generic-alias-control-20261113.log` y
`nested-generic-visibility-control-20261113.log`.

La evidencia usa un Core mínimo y no certifica el SDK completo, todos los
árboles de tipos ni las rutas con sustituciones activas. En el volcado aún
aparecen otras especializaciones con nombres sin resolver; no se afirma su
eliminación. Las pruebas no cubren descarga de módulos ni todas las
intercalaciones concurrentes.
