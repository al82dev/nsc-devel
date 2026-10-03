# NativeSharp 3.0 — Reborrow y préstamos divididos / Reborrowing and split borrows

## Contrato normativo / Normative contract

Este contrato aplica únicamente a unidades `Language=3`. Un local `ref T` es un préstamo mutable
exclusivo del lugar que inicializa. El préstamo dura hasta el final de su scope léxico. Crear un
segundo préstamo independiente requiere demostrar que ambas rutas son disjuntas; si la prueba no
es posible, la compilación falla cerrada.

This contract applies only to `Language=3` units. A `ref T` local is an exclusive mutable borrow of
its initializer storage place. The borrow lasts through its lexical scope. A second independent
borrow is accepted only when both places are provably disjoint; otherwise compilation fails closed.

## Identidad de lugares / Place identity

Una ruta canónica empieza en un local `L<n>` o parámetro `P<n>` y agrega proyecciones:

- `F:Name`: field nominal;
- `I:constant`: elemento de array/fixed array/puntero con índice constante;
- `I:*`: índice dinámico o indexer cuyo mapping no es estructuralmente conocido;
- `S:start:end`: slice half-open con endpoints constantes (`_` representa extremo abierto);
- `S:*`: slice cuyos límites no pueden demostrarse.

A canonical path starts at a local `L<n>` or parameter `P<n>` and appends nominal field, constant
index, wildcard index, or half-open slice projections. Paths are a compile-time identity only; they
do not affect layout, mangling, calling convention or generated storage.

Dos rutas se solapan cuando comparten root y son iguales, una contiene a la otra, o alguna
proyección relevante es wildcard. Fields con nombres distintos y elementos con índices constantes
distintos son disjuntos. Slices constantes son disjuntos cuando sus intervalos half-open no se
intersectan. Unions, layouts con offsets explícitos e indexers definidos por el usuario nunca
obtienen disjunción solo por nombres o índices escritos.

Two paths overlap when they share a root and are equal, one contains the other, or a relevant
projection is a wildcard. Distinct nominal fields and distinct constant array indices are disjoint.
Constant half-open slices are disjoint when their intervals do not intersect. Raw unions, explicit
overlapping layouts and user indexers never gain disjointness merely from written names or indices.

## Reborrow mutable / Mutable reborrow

`ref T child = ref parent` no crea permiso independiente: hereda la ruta y el límite de vida de
`parent`. Durante el scope de `child`, acceder mediante `parent` o mediante el storage original es
error. Acceder mediante `child` es válido; al terminar su scope, `parent` vuelve a estar disponible.
Una cadena de reborrows aplica la misma regla al alias más derivado vivo.

`ref T child = ref parent` does not create independent permission: it inherits the parent's place
and lifetime bound. While `child` is live, access through `parent` or the original storage is an
error. Access through `child` is valid; once its scope ends, `parent` becomes available again. The
same rule applies recursively to a reborrow chain.

## Operaciones y exclusividad / Operations and exclusivity

- Leer, escribir, pasar por `ref` o mover una ruta solapada requiere el permiso activo correcto.
- Un move del owner completo se solapa con toda proyección descendiente.
- Un move parcial de un hermano demostrado disjunto es válido.
- Una llamada mediante el alias activo conserva el permiso; pasar otra ruta solapada no crea un
  escape implícito.
- Un borrow dentro de loop se libera al salir de su scope léxico en cada iteración. El análisis no
  usa non-lexical lifetimes ni adivina la última lectura.
- Índices dinámicos, llamadas que retornan storage sin procedencia comparable, slices de protocolo
  no estructural y aritmética de punteros no demostrada se consideran solapados conservadoramente.

- Reading, writing, passing by `ref`, or moving an overlapping path requires the active permission.
- A whole-owner move overlaps every descendant projection.
- A partial move of a provably disjoint sibling is valid.
- Calls through the active alias preserve permission; another overlapping argument is rejected.
- A loop borrow ends at its lexical scope on every iteration. No non-lexical lifetime inference is
  claimed.
- Dynamic indices and storage without comparable provenance use the conservative overlap fallback.

## Compatibilidad y costo / Compatibility and cost

Edición 2 conserva las reglas anteriores. F03 no cambia Win64 ABI, NSMETA, layout ni runtime; solo
añade estado semántico y texto de dump. La precisión futura de lifetimes nombrados y procedencia
cross-module pertenece a F04/F07 y puede ampliar pruebas, pero no relajar exclusividad.

Edition 2 preserves its prior rules. F03 changes neither the Win64 ABI, NSMETA, layout nor runtime;
it adds semantic state and dump text only. Named lifetimes and cross-module provenance belong to
F04/F07 and may extend proofs without weakening exclusivity.

## Diagnósticos mínimos / Required diagnostics

- dos préstamos mutables independientes de rutas solapadas;
- acceso directo al storage mientras vive un ref solapado;
- uso del alias padre mientras vive un reborrow hijo;
- move completo del owner prestado;
- índice dinámico/indexer/layout no demostrable tratado como solapamiento;
- `ref var` escrito en fuente, porque un local ref exige tipo de elemento explícito.
