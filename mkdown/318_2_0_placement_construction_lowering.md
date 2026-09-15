# 318 — Lowering de placement construction / Placement construction lowering

## Resultado / Outcome

La forma introducida en el punto 317 ya se ejecuta en el pipeline de proyecto:

```nativesharp
Cell* storage = stackalloc Cell[1];
Cell* placed = new (storage) Cell(20, 22);
```

El compilador reutiliza la selección ordinaria del constructor pero sustituye la reserva de heap
por la dirección aportada. El resultado señala exactamente el storage original.

Placement construction now reaches HIR, MIR, verifier-clean LLVM, linking, and real O0 execution.
It reuses ordinary constructor binding while replacing heap allocation with the supplied address.

## Contrato físico / Physical contract

- `T` es una clase o struct concreto sin modificadores.
- El destino tiene tipo exacto `T*`; no se acepta una conversión implícita ni un temporal.
- El destino se evalúa una vez antes que los argumentos.
- Los argumentos se evalúan una vez de izquierda a derecha y conservan orden nominal, defaults,
  conversiones, accesibilidad y ownership del constructor normal.
- LLVM comprueba null, pone a cero el layout canónico y llama al constructor con el destino como
  `this`.
- Una clase recibe además su descriptor antes de ejecutar el constructor.
- No se llama a `__nativesharp_alloc` ni se programa liberación.

Un struct produce `T*`. Una clase produce la referencia no propietaria `T` respaldada físicamente
por esos bytes, lo que permite usar miembros y dispatch ordinarios sin reinterpretar un `T*` como
un slot que contenga otra referencia.

El caller garantiza capacidad, writability y alineación. El punto 319 agrega destrucción explícita
sin liberar y el 320 modela por separado ownership del objeto y del storage.

## Representación / Representation

`PlacementConstructionExpression` entra al modelo común de constructores. HIR usa
`placement-construction`: su primer hijo es el storage, seguido por los argumentos y los
inicializadores sintetizados. MIR usa `placement-construct`, congela bytes/alineación y marca
`storage=supplied`; el modelo de layout omite deliberadamente un allocation plan.

## Cobertura / Coverage

`tests/unit/Item318PlacementConstructionLoweringTests.cpp` valida:

- constructor explícito de struct e implicit default con zero-init;
- clase colocada con descriptor, field y llamada de método funcionales;
- identidad de dirección y orden de evaluación `destino, argumento 1, argumento 2`;
- HIR/MIR auditables y LLVM sin llamada al asignador;
- enlace y ejecución real O0;
- rechazo de tipo de storage incorrecto, tipo no construible y aridad inválida.

Las suites de sintaxis placement, constructores, construcción de structs y VLAs permanecen verdes.

## Mensaje de commit / Commit message

`Baja placement construction sin asignar / Lower allocation-free placement construction`

`Construye clases y structs una sola vez en storage T* validado, conserva orden y contrato del
constructor y evita toda reserva implícita. / Construct classes and structs exactly once in
validated T* storage, preserve constructor order and contract, and avoid every implicit allocation.`
