# Tanda 12: cierre

## Criterio

El mismo de las tandas [10](t10-cierre.md) y [11](t11-cierre.md). Una casilla T12 se cierra cuando:

- la capacidad que pide tiene evidencia propia frente a un control compilado por Clang 23.1.0, en
  biblioteca estática y DLL, O0–O3;
- cada declaración que NSC llama directamente usa el símbolo y los tipos físicos de Clang, y cada
  llamada heredada o virtual va por un adaptador que compila Clang;
- cada forma que NSC no proyecta se rechaza con diagnóstico, sin emitir otra llamada ni otro
  almacenamiento, y queda registrada abajo como pendiente, con la tanda o el ítem que la continúa.

Un límite registrado no es interoperabilidad terminada (T18-007).

## Casillas

| Ítem | Capacidad comprobada | Registro |
|---|---|---|
| T12-001 | Bases y su acceso: campos de bases públicas no virtuales; base privada y virtual rechazadas; records trivialmente copiables con bases como valores | [herencia](t12-herencia-y-despacho.md) |
| T12-002 | Offsets de base de Clang en campos y conversiones; ajuste de `this` en el adaptador | [herencia](t12-herencia-y-despacho.md) |
| T12-003 | Conversión implícita de derivado a base, nulo conservado; cast explícito como `static_cast` | [herencia](t12-herencia-y-despacho.md) |
| T12-004 | Métodos heredados de la primera y la segunda base; operadores miembro | [herencia](t12-herencia-y-despacho.md), [operadores](t12-operadores-miembro.md) |
| T12-005 | Métodos virtuales y overrides | [herencia](t12-herencia-y-despacho.md) |
| T12-006 | Despacho por la vtable del ABI de Microsoft, en objetos de NativeSharp y por punteros a la base | [herencia](t12-herencia-y-despacho.md) |
| T12-007 | Destrucción mediante una base con destructor virtual | [herencia](t12-herencia-y-despacho.md) |
| T12-008 | Herencia múltiple con base polimórfica no primaria | [herencia](t12-herencia-y-despacho.md) |
| T12-009 | Métodos de una base virtual y su override | [herencia](t12-herencia-y-despacho.md) |
| T12-010 | `typeid` en la biblioteca, DLL con su CRT | [herencia](t12-herencia-y-despacho.md) |
| T12-011 | `dynamic_cast` en la biblioteca, DLL con su CRT | [herencia](t12-herencia-y-despacho.md) |
| T12-012 | Comparación con un consumidor C++ equivalente en cada fixture | [herencia](t12-herencia-y-despacho.md), [operadores](t12-operadores-miembro.md) |

## Pendientes que quedan fuera de la tanda 12

Cada uno se rechaza con diagnóstico o deja el nombre sin proyectar.

| Pendiente | Ítem | Continúa en |
|---|---|---|
| RTTI, `new`, `delete` y estáticas con guarda en una biblioteca estática: necesitan el CRT en el ejecutable | T12-007, T12-010, T12-011 | T16-002, T16-003 |
| `typeid` y `dynamic_cast` escritos en NativeSharp sobre un tipo C++ | T12-010, T12-011 | pendiente documentado |
| Campos de una base virtual y conversión de puntero a una base virtual | T12-001, T12-003, T12-009 | pendiente documentado |
| Construcción de un record con base virtual desde NativeSharp | T12-009 | pendiente documentado |
| Referencia a la base (`ref`/`in` de `B`) desde un derivado | T12-003 | pendiente documentado |
| Operadores miembro no const, heredados o de asignación, llamada, subíndice y flecha | T12-004 | pendiente documentado |
| Métodos heredados y virtuales en una plantilla precompilada | T12-004, T12-006 | T13-001 |
| Excepciones que atraviesan un adaptador | T12-004 | T15-003 |
| Herencia de un tipo C++ desde NativeSharp | — | fuera del alcance |

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
