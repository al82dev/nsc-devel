# NS3-T10-F04 — Generadores asíncronos / Async generators

## ES

El compilador modela `await` y `yield` dentro de una sola máquina de estados y un solo frame nativo. El plan conserva los tipos de item, error y cancelación, la política explícita de almacenamiento, el tamaño/alineación del frame y si los items son move-only. Los items prestados sólo cruzan una suspensión cuando existen pruebas de lifetime y dirección estable.

El protocolo exige demanda explícita del consumidor. Sólo puede existir una petición pendiente y un item sin consumir; `Pending` conserva la demanda, `ItemReady` la satisface y bloquea otra petición hasta consumir el item. Error, finalización y cancelación temprana son estados terminales distintos. No hay cola ni asignación heap implícitas.

Prueba dirigida: `nsc_ns3_t10_async_generator_tests` cubre ready/pending alternado, backpressure, items prestados, parada temprana, errores tipados, move-only y tamaño del frame.

## EN

The compiler models `await` and `yield` inside one state machine and one native frame. The plan preserves item, error, and cancellation types, the explicit storage policy, frame size/alignment, and move-only item identity. Borrowed items may cross a suspension only with lifetime and stable-address proofs.

The protocol requires explicit consumer demand. Only one request and one unconsumed item may be outstanding; `Pending` retains demand, `ItemReady` satisfies it and blocks another request until the item is consumed. Error, completion, and early cancellation are distinct terminal states. No queue or heap allocation is implicit.

Focused test: `nsc_ns3_t10_async_generator_tests` covers alternating ready/pending, backpressure, borrowed items, early stop, typed errors, move-only items, and frame size.
