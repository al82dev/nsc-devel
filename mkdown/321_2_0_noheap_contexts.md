# 321 — Contextos `noheap` / No-heap contexts

## Resultado / Outcome

Un método puede declarar `noheap` para convertir la ausencia de asignaciones directas de heap en
un contrato comprobado:

```nativesharp
public static noheap int Fill(Cell* storage)
{
    Cell* value = new (storage) Cell();
    value->Data = 42;
    destroy value;
    return 42;
}
```

`stackalloc`, placement construction, valores y `destroy` permanecen disponibles. `new` de clase,
raw `new`, arreglos nativos, operaciones de string que reservan, delegates, lambdas y suscripciones
de evento producen un diagnóstico en la expresión exacta.

The modifier establishes a compiler-checked direct-allocation boundary while preserving the stack
and caller-supplied-storage programming model.

## Frontera del contrato / Contract boundary

El lexer reserva la palabra, el parser la admite una sola vez en métodos ordinarios y contratos de
interfaz, y el modelo de miembros la conserva como un bit independiente de inlining, ownership,
ABI y accesibilidad. El volcado de miembros publica `noheap=true`.

La validación sucede al construir HIR, donde ya se conoce la operación física. Esto evita deducir
heap desde la ortografía: construcción de struct y placement no se confunden con construcción de
clase, y una lambda se reconoce por su nodo de creación aunque su sintaxis aparezca dentro de una
conversión.

## Separación respecto al 322 / Separation from item 322

El 321 cierra operaciones directas del cuerpo. Todavía puede aparecer una llamada ordinaria, pues
decidir si su destino respeta `noheap` requiere el grafo, overrides, interfaces y metadata. Esa
propagación completa es el punto 322 y queda deliberadamente fuera de este commit.

## Cobertura / Coverage

`tests/unit/Item321NoHeapContextTests.cpp` verifica el contrato visible en el modelo, aceptación de
stack/placement, diagnósticos de clase, raw/array `new`, concatenación, lambda y modificador
duplicado. Las regresiones de lexer, parser, placement, destrucción y ownership se ejecutan antes
del commit.

## Mensaje de commit / Commit message

`Añade contextos noheap / Add no-heap contexts`
