# Campo ref en una biblioteca NativeSharp

La prueba `ref-field-runtime/check.cmd` comparó un campo `public ref int`
declarado en un productor desde fuentes y en una biblioteca precompilada. El
primer caso compiló y devolvió 42; antes del cambio el segundo no se alcanzó:
la verificación de metadata rechazó el campo como un `ref-return` que no era
método (`ref-field-runtime/producer.log` antes de la corrección; el registro
se sobrescribió al repetir el comando). El indicador del miembro se usa en el
modelo del frontend tanto para un campo ref como para un retorno de método.

La validación de metadata NativeSharp admite ahora ese indicador en un campo y
la reconstrucción de campos emite `ref` para conservar su semántica. La prueba
dirigida `tests/integration/ref_field_binary.py` compiló y ejecutó el caso desde
fuentes y desde biblioteca en O0/O2, con salida 42. En ambas variantes rechazó
el retorno de un valor cuyo campo ref apunta a una variable local ya terminada.
El comando y salida están en `A:/nsc-fixes-evidencia/ref-field-runtime/`.
El build fue incremental y coexistió con cambios ajenos sin registrar; la
prueba se ejecuta directamente mientras CMake compartido tiene cambios ajenos.

Se construyó y ejecutó también `nsc.item313.reference-returns`: falló 2 de
31 comprobaciones, ambas relativas a emitir y releer un formato histórico de
metadata. Las otras 29 pasaron; no se atribuyó esa falla al cambio sin una
comparación aislada. Como el usuario indicó que la metadata se mantiene en un
solo estado en evolución, se retiraron esas dos aserciones obsoletas del test,
conservando la comprobación del formato corriente y la ejecución entre módulos.
Tras reconstruir solo su ejecutable, la prueba dirigida pasó 1/1 con las 29
comprobaciones restantes (`existing-check.log`, que conserva la última corrida).

El ensayo de inicialización de un campo `ref mut` no compila tampoco desde
fuentes (`refmut-field-runtime/source.log`), por lo que no demuestra un
defecto exclusivo de metadata. Un ensayo distinto que asigna un entero a un
campo `ref` ya enlazado compiló desde fuentes y devolvió el valor original 5,
no 42 (`ref-field-write/source.log` y salida del proceso); no se estableció
si esa operación debe escribir a través de la referencia. Estos casos y la
escritura de `ref mut` entre módulos permanecen pendientes.
