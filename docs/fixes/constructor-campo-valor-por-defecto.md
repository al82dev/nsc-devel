# Campo de valor por defecto con destructor en un constructor que falla (NSC-NEW-012)

## Problema

`nsc.sdk.shared-public-pointers`, caso `Rc normal_factory_base_value_default_failure`, salía 3 en
lugar de 42. El caso reproduce sin punteros compartidos (`A:/nsc-fixes-evidencia/new012`):

```
struct Part { public static int Drops; ~Part() { Drops = Drops + 1; } }
class Base { public Part field; public int Number; public Base() { Number = Gate.Read(); } }
```

Si el constructor de `Base` lanza, el destructor de `field` no se ejecuta (`Drops == 0`), en O0 y
O2. Pasa con `new Base()` directo, bajo un `Derived`, y con un constructor que lanza sin más. En
cambio, la destrucción normal sí deja ese valor (`Drops == 1`), y un campo con inicializador
(`= new Part(1)`) también se libera en el fallo.

## Causa

Al preparar las limpiezas de campos de un constructor (`OopLlvmStatements.cpp`), se registran dos
grupos:

- los campos con una escritura (`Store`), que se marcan vivos en esa escritura;
- entre los campos sin escritura, solo los punteros `owned` anulables y los vacíos compartidos.

Un campo de valor con destructor que conserva su valor por defecto no tenía limpieza. Sin embargo,
está vivo desde el principio: el destructor del objeto lo libera.

## Corrección

Un campo de instancia cuyo tipo requiere liberación de valor, que el constructor no escribe ni
recibe por delegación y que está en una disposición sin solapamiento, se registra como vivo desde
el inicio. El constructor que falla lo libera una vez. Los campos escritos conservan la regla
anterior. Los campos del tipo derivado no están vivos mientras falla el constructor base, porque
sus limpiezas se preparan después de la llamada base, igual que antes.

## Contradicción resuelta en la suite

`unrelated_weak_default_preserved` esperaba que un `struct` del usuario llamado como el débil de Core
no se liberara en el fallo (`Drops == 0`). Eso contradice
`normal_factory_base_value_default_failure`, de la misma suite, que exige liberar el valor por defecto.

Prevalece la regla de RAII: cada valor vivo se destruye exactamente una vez, y el éxito ya lo
destruye. El caso ahora espera `Drops == 1`, y sigue detectando la confusión por nombre: si se
tratara como el débil de Core, se omitiría su destructor.

## Prueba

`nsc.compiler.constructor-default-value-field-failure` comprueba cinco programas en O0 y O2:

- el éxito libera cada campo una vez;
- falla el constructor que declara el campo;
- falla la base bajo un derivado;
- falla el derivado tras completar la base;
- el constructor lanza directamente.

Antes del cambio, las sondas equivalentes salían 3.

En `nsc.sdk.shared-public-pointers` pasan todos los casos Rc, incluido el de NEW-012, salvo tres
fallos previos: `overwrite_explicit_disjoint_view`, `union_without_view` y
`overwrite_local_array_after_view`. Para Arc falla `static_nested_tuple_weak`. Los cuatro son
independientes del cambio: con la corrección desactivada fallan igual. Quedan registrados en
NSC-NEW-013.
