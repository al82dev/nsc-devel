# 319 — Destrucción explícita sin liberar almacenamiento / Explicit destruction without deallocation

## Resultado / Outcome

`destroy objeto` termina el lifetime construido sin entregar el storage al allocador:

```nativesharp
long* bytes = stackalloc long[16];
Widget* storage = (Widget*)bytes;
Widget widget = new (storage) Widget(42);
destroy widget;
Widget next = new (storage) Widget(43);
```

La operación evalúa su destino una vez, produce `void` y admite referencias de clase/interfaz,
punteros a clase y punteros a struct. `null` no hace nada. Un struct trivial sólo termina su
lifetime lógico; no genera liberación ni llamada de runtime.

Explicit destruction now runs the complete dynamic object teardown while leaving the caller-owned
bytes intact. The same address can begin a fresh lifetime through placement construction.

## Separación dinámica / Dynamic split

La entrada histórica `__nsc_destroy_*` hacía dos trabajos inseparables: destruía el objeto y
liberaba su bloque. Cada descriptor incorpora ahora una entrada `__nsc_destroy_object_*` adicional:

- la entrada object-only ejecuta destructor derivado, campos `owned`, bases y delegación a bases
  precompiladas, sin liberar el objeto exterior;
- la entrada histórica llama primero a la object-only y después ejecuta un único
  `__nativesharp_free`;
- `destroy` despacha por el descriptor real hacia la primera; `delete` conserva la segunda.

Los offsets anteriores del descriptor permanecen inmóviles y la nueva entrada se agrega como campo
7. Como un productor antiguo no contiene ese campo, el contrato físico se identifica explícitamente
como Object ABI 2.

## Seguridad / Safety boundary

El frontend rechaza escalares, punteros primitivos, locales `raii`, valores `owned` y `destroy this`
dentro del destructor activo. Estas prohibiciones evitan que una obligación existente vuelva a
destruir el mismo objeto. El punto 320 completa el modelo distinguiendo de forma persistente la
propiedad del objeto y la del storage entre aliases.

## Cobertura / Coverage

`tests/unit/Item319ExplicitObjectDestructionTests.cpp` comprueba HIR/MIR, el campo nuevo del
descriptor, LLVM verificable, orden derivado-base, operands de referencia y puntero, storage de
pila reutilizable, struct trivial y diagnósticos inválidos. También permanecen verdes lexer,
parser, placement 318, Stage 35, ownership Stage 50.11 y el contrato C8 del descriptor.

## Mensaje de commit / Commit message

`Destruye objetos sin liberar storage / Destroy objects without releasing storage`
