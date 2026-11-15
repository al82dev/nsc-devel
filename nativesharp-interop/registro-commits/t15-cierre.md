# Tanda 15: cierre

## Criterio

El mismo de las tandas [10](t10-cierre.md) a [14](t14-cierre.md). Una casilla T15 se cierra cuando:

- la capacidad que pide tiene evidencia propia frente a un control compilado por Clang 23.1.0 y enlazado con
  el mismo runtime, en biblioteca estática y DLL, O0–O3;
- las excepciones, los destructores, los frames de corutina y las reservas los ejecuta el runtime de C++ de
  MSVC, inicializado por su propio arranque, y cada declaración que NSC llama usa el símbolo y los tipos
  físicos de Clang;
- cada forma que NSC no proyecta se rechaza con diagnóstico, sin emitir otra llamada ni otro almacenamiento,
  y queda registrada abajo como pendiente, con la tanda o el ítem que la continúa.

Un límite registrado no es interoperabilidad terminada (T18-007).

## Casillas

| Ítem | Capacidad comprobada | Registro |
|---|---|---|
| T15-001 | NSC usa SEH en `try`, `finally` y limpieza; una excepción C++ desenrolla sus frames | [excepciones](t15-excepciones.md) |
| T15-002 | Modelo de Microsoft de Clang (`_CxxThrowException`, `__CxxFrameHandler3`); runtime inicializado por `mainCRTStartup` | [excepciones](t15-excepciones.md), [runtime](t15-runtime-cpp.md) |
| T15-003 | Excepción C++ que atraviesa NativeSharp sin convertirse; `catch` NativeSharp no la atrapa; sin `catch` C++, `std::terminate` | [excepciones](t15-excepciones.md) |
| T15-004 | `finally` y destructor de un local C++ construido en NativeSharp durante el desenrollado | [excepciones](t15-excepciones.md) |
| T15-005 | Excepción que escapa de `noexcept`: `std::terminate`, igual que Clang | [excepciones](t15-excepciones.md) |
| T15-006 | Excepciones `int` y de un tipo con destructor que atraviesan un callback NativeSharp | [excepciones](t15-excepciones.md) |
| T15-007 | La misma matriz con la biblioteca en una DLL con su propio runtime | [excepciones](t15-excepciones.md) |
| T15-008 | Lambda sin capturas pasada como puntero a función C++ | [callbacks](t15-callbacks.md) |
| T15-009 | Adaptador con la convención y los tipos de Clang | [callbacks](t15-callbacks.md) |
| T15-010 | Lambda con capturas rechazada; estado por un `void*` de contexto | [callbacks](t15-callbacks.md) |
| T15-011 | Callback llamado durante la llamada C++, también compuesto | [callbacks](t15-callbacks.md) |
| T15-012 | Callback guardado y llamado en una llamada posterior | [callbacks](t15-callbacks.md) |
| T15-013 | Excepción C++ a través de un callback hacia el `catch` de la biblioteca; `panic` en un callback termina con 142 | [excepciones](t15-excepciones.md) |
| T15-014 | Puntero inteligente propietario no copiable construido y destruido por NativeSharp; `std::unique_ptr` en la biblioteca | [recursos](t15-recursos.md) |
| T15-015 | Allocator con estado como objeto NativeSharp; `std::allocator` en la biblioteca | [recursos](t15-recursos.md) |
| T15-016 | Copia de un tipo no copiable rechazada (`NS3020`); transferencia hecha por C++ | [recursos](t15-recursos.md) |
| T15-017 | Generador con `co_yield` y el pipeline de corutinas de Clang | [recursos](t15-recursos.md), [runtime](t15-runtime-cpp.md) |
| T15-018 | Suspensión, reanudación, fin y liberación del frame | [recursos](t15-recursos.md) |

## Pendientes que quedan fuera de la tanda 15

Cada uno se rechaza con diagnóstico o deja el nombre sin proyectar.

| Pendiente | Ítem | Continúa en |
|---|---|---|
| Convertir una excepción C++ en una excepción NativeSharp, o atraparla en NativeSharp | T15-003 | pendiente documentado |
| Una excepción NativeSharp lanzada dentro de un callback, con la biblioteca núcleo | T15-013 | pendiente documentado |
| Una lambda escrita directamente como argumento de tipo puntero a función | T15-008 | T17-003 |
| `std::function` y objetos invocables con estado | T15-010 | pendiente documentado |
| Construir desde NativeSharp un tipo cuyo constructor es una plantilla (`std::unique_ptr`) | T15-014 | pendiente documentado |
| Tipos movibles no copiables devueltos por valor y `move` de un objeto C++ en NativeSharp | T15-016 | pendiente documentado |
| Una corutina devuelta por valor | T15-017 | pendiente documentado |
| `std::allocator<int>::deallocate` desde NativeSharp | T15-015 | pendiente documentado |
| Runtime de C++ dinámico (`/MD`) | T15-002 | T16-002 |

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
