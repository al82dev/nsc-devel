# Campo ref mut en una biblioteca NativeSharp

Una biblioteca con `public ref mut int value` fallaba al generarse: la
verificación de metadata informaba que no podía materializar la clave
`refmut:builtin:int`. La reproducción anterior está en
`A:/nsc-fixes-evidencia/thread-inherited-binary/mutable-producer-failure.json`
y `thread-inherited-fields/refmut-before.log`.

La clave de campo ya conserva el calificador `refmut:`. Al reconstruir la
declaración NativeSharp, `PublicSurfaceBuilder::emitNominalField` intentaba
tratar esa clave completa como un tipo nominal o builtin. Ahora separa el
calificador solo para esa emisión, reconstruye el tipo referido mediante la
ruta existente y escribe `ref mut` en el campo. No cambia el formato ni la
numeración de metadata.

La prueba dirigida `tests/integration/thread_inherited_safety.py` genera
bibliotecas independientes con campos públicos y privados `ref mut int`,
`ref mut RawReferent` del mismo productor y `ref mut Exception` de Core.
En el consumidor, la base y la derivada se rechazan para Sync; el referente
con puntero crudo se rechaza también para Send. Pasó con Core desde fuentes
y con el SDK binario aislado. Los comandos están en
`A:/nsc-fixes-evidencia/thread-inherited-fields/regression.cmd` y
`regression-binary.cmd`, con sus logs correspondientes. La prueba también
mantiene controles con campos no `ref` y de captura. No ejecuta lecturas o
escrituras del campo a través de módulos ni acredita todos los tipos de
referente o layouts. El build incremental coexistió con cambios ajenos sin
registrar; la prueba se ejecuta directamente mientras CMake está compartido.
La selección dirigida de consumo binario de los punteros compartidos también
pasó (`A:/nsc-fixes-evidencia/shared-binary-sdk-current-check.log`).
