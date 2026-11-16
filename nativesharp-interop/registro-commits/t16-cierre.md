# Tanda 16: cierre

## Criterio

El mismo de las tandas [10](t10-cierre.md) a [15](t15-cierre.md). Una casilla T16 se cierra cuando:

- la capacidad que pide tiene evidencia propia frente a un control compilado por Clang 23.1.0 y enlazado con
  el mismo runtime, en biblioteca estática y DLL, O0–O3, o, si pide una política, la política está definida
  sobre el código y los fixtures de la tanda;
- los constructores, destructores, TLS y atómicos los ejecuta el runtime de C++ de MSVC, inicializado por su
  propio arranque, y una entrada que nadie inicializaría se rechaza con diagnóstico;
- cada forma que NSC no cubre queda registrada abajo como pendiente, con la tanda o el ítem que la continúa.

Un límite registrado no es interoperabilidad terminada (T18-007).

## Casillas

| Ítem | Capacidad comprobada | Registro |
|---|---|---|
| T16-001 | Entrada `__nativesharp_start`; con `--crt-startup`, `mainCRTStartup` y un `main` que entra en NativeSharp | [arranque](t16-arranque.md) |
| T16-002 | Objetos del programa y del código C++ emitido, runtime NativeSharp, bibliotecas, módulos precompilados y el runtime estático de MSVC | [arranque](t16-arranque.md) |
| T16-003 | Constructores globales ejecutados por el CRT antes de `main`; sin `--crt-startup`, el miembro con inicializadores que toma el enlace se rechaza por su nombre | [arranque](t16-arranque.md), [inicializadores](t16-inicializadores.md) |
| T16-004 | Destructores y manejadores de salida ejecutados por `exit` del CRT tras `Main`; en una DLL, al descargarse | [arranque](t16-arranque.md) |
| T16-005 | Constructor, `Main` y destructor observados con Win32, sin el CRT: `cmd` | [arranque](t16-arranque.md) |
| T16-006 | Directorio TLS y `__dyn_tls_init` del CRT; sin el CRT el enlace falla con esos símbolos | [TLS](t16-tls.md) |
| T16-007 | Un `thread_local` con constructor construido por cada hilo, también uno de la biblioteca que llama a NativeSharp | [TLS](t16-tls.md) |
| T16-008 | Orden: runtime de C y C++, constructores C++, runtime NativeSharp, destructores C++ | [arranque](t16-arranque.md) |
| T16-009 | Cada imagen con su propio runtime NativeSharp y su propio CRT estático | [políticas](t16-politicas.md) |
| T16-010 | Libera la imagen que reserva | [políticas](t16-politicas.md) |
| T16-011 | Los recursos del CRT no atraviesan el límite de una DLL | [políticas](t16-politicas.md) |
| T16-012 | DLL C++ con globales construidos al cargarse y destruidos al descargarse | [arranque](t16-arranque.md) |
| T16-013 | Globales de una DLL construidos en `_DllMainCRTStartup` bajo el lock del cargador; restricciones documentadas | [arranque](t16-arranque.md) |
| T16-014 | Tres unidades C++ en dos bibliotecas y un módulo NativeSharp precompilado, con NSLink y lld-link | [enlace](t16-enlace.md) |
| T16-015 | Definición fuerte duplicada rechazada con su nombre por los dos enlazadores; COMDAT plegadas | [enlace](t16-enlace.md) |
| T16-016 | Biblioteca ThinLTO enlazada con lld-link; NSLink la rechaza con su motivo | [enlace](t16-enlace.md) |
| T16-017 | `std::atomic` de la biblioteca operado desde cuatro hilos por callbacks NativeSharp | [atómicos](t16-atomicos.md) |

## Pendientes que quedan fuera de la tanda 16

| Pendiente | Ítem | Continúa en |
|---|---|---|
| Runtime de C++ dinámico (`/MD`) | T16-002, T16-009 | pendiente documentado |
| Ejecutar inicializadores de `.CRT$XCU` desde la entrada de NativeSharp sin el CRT | T16-003 | pendiente documentado |
| Diagnóstico al llamar a NativeSharp desde un constructor global C++ | T16-008 | pendiente documentado |
| `LoadLibrary`/`FreeLibrary` de una DLL C++ desde NativeSharp | T16-012 | pendiente documentado |
| Diagnóstico propio para TLS sin las bibliotecas del CRT (hoy, símbolos sin definir del enlazador) | T16-006 | T17-006 |
| LTO entre el programa NativeSharp y la biblioteca C++; bitcode con NSLink | T16-016 | pendiente documentado |
| Un hilo creado por NativeSharp que usa TLS C++ | T16-007 | pendiente documentado |

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
