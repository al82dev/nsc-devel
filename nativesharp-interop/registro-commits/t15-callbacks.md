# T15-008 a T15-012: funciones NativeSharp como callbacks de C++

## Comprobación

Este cambio comprueba los callbacks hacia C++; no cambia código de producción.

## Diseño

- **Sin capturas** (T15-008, T15-009): una lambda NativeSharp sin capturas asignada a un local de un tipo
  puntero a función C++ (`T15.Cb f = (int v) => v * 2`) se convierte con el adaptador que NSC emite con la
  convención y los tipos de Clang, el mismo de los callbacks C (tanda 8).
- **Con estado** (T15-010): una lambda con capturas no tiene representación como puntero a función y se
  rechaza; el estado viaja por un `void*` de contexto que la biblioteca devuelve al callback, con la vida que
  le da NativeSharp.
- **Durante la llamada y conservado** (T15-011, T15-012): la biblioteca llama al callback dentro de la
  llamada o lo guarda y lo llama en una llamada posterior; una función NativeSharp no tiene captura que
  pueda quedar colgada.

## Evidencia

Fixture nuevo `pruebas-tanda-15-callbacks/validar.py`, con y sin `--dll`: callback inmediato, guardado y
llamado después dos veces, con contexto (struct NativeSharp por `void*`) y compuesto (`f(f(x))`).
Resultado 125 frente al control C++ de Clang 23.1.0, O0–O3, con las 5 llamadas C++ de NSC iguales a las de
Clang. La biblioteca no necesita el runtime de C y el ejecutable se enlaza con NSLink.

Control negativo: una lambda que captura un local se rechaza (`must not capture state`); la contraparte pasa
el estado por el contexto.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t15-callbacks-ljbz_wme` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t15-callbacks-q7r60_99` |

## Límites

- Una lambda escrita directamente como argumento no resuelve la sobrecarga; se asigna antes a un local del
  tipo puntero a función.
- `std::function` y otros objetos invocables con estado no se construyen desde NativeSharp.
