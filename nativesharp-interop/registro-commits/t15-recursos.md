# T15-014 a T15-018: punteros inteligentes, allocators, tipos no copiables y corutinas

## Comprobación

Con el [runtime de C++](t15-runtime-cpp.md) y la STL de MSVC en la sesión, este cambio comprueba los recursos
C++; no cambia código de producción.

## Diseño

- **Punteros inteligentes** (T15-014): un puntero inteligente propietario de la biblioteca (`Owner<int>`, no
  copiable, con destructor) se construye en un local NativeSharp, que lo destruye al salir del bloque con el
  destructor de Clang; la biblioteca lo llena y transfiere la propiedad. `std::unique_ptr` se usa dentro de la
  biblioteca; NativeSharp no lo construye porque su constructor por defecto es una plantilla.
- **Allocators** (T15-015): un allocator con estado (`Arena`) es un objeto NativeSharp que la biblioteca usa
  por referencia; `std::allocator` reserva y libera en la biblioteca con el runtime de C++.
- **Movibles no copiables** (T15-016): copiar `Owner<int>` o `std::unique_ptr<int>` en NativeSharp se rechaza
  (`NS3020`); la transferencia la hace una función C++ que mueve el puntero, y el origen queda vacío.
- **Corutinas** (T15-017, T15-018): la biblioteca crea un generador con `co_yield` en un objeto NativeSharp
  (`T15.Gen`); NativeSharp lo reanuda, lee los valores, comprueba su fin y lo libera. El frame lo reserva el
  runtime de C++ y su promesa se destruye al liberarlo.

## Evidencia

Fixture nuevo `pruebas-tanda-15-recursos/validar.py`, con y sin `--dll`. Resultado 1509 frente al control C++
de Clang 23.1.0, O0–O3: valor transferido y origen vacío (142), una liberación (1000), `Arena` usado (38),
`unique_ptr` y `std::allocator` (12), dos valores y un frame vivo (17), fin (300) y frame liberado (0).

Controles negativos: copiar `Owner<int>` y `std::unique_ptr<int>` se rechaza (`NS3020`).

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t15-recursos-gkgy_1bo` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t15-recursos-ox522dhi` |

## Límites

- `std::unique_ptr` y otros tipos cuyo constructor es una plantilla no se construyen desde NativeSharp.
- Un tipo movible no copiable devuelto por valor, y `move` de un objeto C++ en NativeSharp, no se importan.
- Una corutina devuelta por valor no se importa; se crea en un objeto NativeSharp por referencia.
- `std::allocator<int>::deallocate` no es un miembro compatible desde NativeSharp (`allocate` sí); no se investigó la causa.
