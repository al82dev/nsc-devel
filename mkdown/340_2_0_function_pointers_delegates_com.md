# Ítem 340 — Function pointers y delegates para COM / COM function pointers and delegates

## Resultado / Result

Los métodos COM pueden describirse con tipos `extern C delegate` distintos y conservar su firma al
publicarse como direcciones nativas. El programa borra deliberadamente el tipo al almacenar cada
dirección en la vtable y exige una reconstrucción explícita al leerla. / COM methods can be
described with distinct `extern C delegate` types and retain their signatures when published as
native addresses. The program deliberately erases the type while storing each address in the
vtable and requires explicit reconstruction when reading it.

## Superficie certificada / Certified surface

La matriz dirigida cubre un método tipo `QueryInterface` con `void**` de salida, un método de
referencia con retorno `uint` y un método con dos parámetros de salida `int*`. Todos reciben el
puntero opaco `self` como primer argumento. / The focused matrix covers a `QueryInterface`-like
method with a `void**` output, a reference method returning `uint`, and a method with two `int*`
outputs. All receive the opaque `self` pointer as their first argument.

## Pruebas / Tests

La prueba ejecuta el round trip delegate-dirección-slot-delegate en O0 y O2. También verifica
identidad del objeto devuelto, mutación a través de `self` y escritura de parámetros de salida; el
resultado observable es 42. / The test executes the delegate-address-slot-delegate round trip at
O0 and O2. It also verifies returned object identity, mutation through `self`, and output-parameter
writes; the observable result is 42.
