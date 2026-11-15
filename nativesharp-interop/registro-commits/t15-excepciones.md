# T15-001 a T15-007 y T15-013: excepciones C++ que atraviesan NativeSharp

## Comprobación

Con el [runtime de C++](t15-runtime-cpp.md) arrancado, este cambio comprueba el modelo de excepciones; no
cambia código de producción.

## Diseño

- **Modelo** (T15-001, T15-002): NSC usa SEH (`__C_specific_handler`) en sus regiones `try`, `finally` y
  limpieza; Clang usa `__CxxFrameHandler3`, también sobre SEH. Una excepción C++ desenrolla los frames de NSC
  con `RtlUnwindEx` y ejecuta su limpieza.
- **Propagación** (T15-003): una excepción C++ atraviesa NativeSharp sin convertirse. Solo un frame C++ la
  atrapa; un `catch` NativeSharp, con tipo o general, atrapa solo excepciones NativeSharp. Sin un `catch` C++,
  el runtime de C++ llama a `std::terminate`, igual que en un programa C++.
- **Limpieza** (T15-004, T15-006): durante el desenrollado corren el `finally` NativeSharp y el destructor de
  un local C++ que NativeSharp construyó (`T15.Tracker t = new T15.Tracker()`).
- **`noexcept`** (T15-005): una excepción que escapa de una función `noexcept` termina el proceso con
  `std::terminate` dentro de la biblioteca; NativeSharp llama la función igual.
- **DLL** (T15-007): una biblioteca DLL con su propio runtime estático lanza, atrapa y destruye su objeto de
  excepción; el ejecutable arranca por su propio runtime.
- **Límite de un callback** (T15-013): una excepción C++ puede atravesar un callback NativeSharp hacia el
  `catch` de la biblioteca. Un `panic` en un callback termina el proceso con 142 sin desenrollar los frames
  C++. Una excepción NativeSharp no debe salir de un callback hacia frames C++: un proyecto sin la biblioteca
  núcleo no puede lanzarlas, y con ella queda como pendiente.

## Evidencia

Fixture nuevo `pruebas-tanda-15-excepciones/validar.py`, con y sin `--dll`. Cada caso se compara con un
control C++ de Clang 23.1.0 enlazado con el mismo runtime, O0–O3:

| Caso | Resultado |
|---|---|
| excepción `int` que atraviesa un callback NativeSharp hasta el `catch` de la biblioteca | 41 |
| `finally` NativeSharp y destructor de un local C++ durante el desenrollado | 241 |
| excepción de un tipo con destructor, atrapada por referencia | 107 |
| `noexcept` que deja escapar una excepción | `std::terminate` (0xC0000409), igual que Clang |
| excepción sin `catch` | `std::terminate`, igual que Clang |
| `catch` NativeSharp alrededor de una excepción C++ | no la atrapa: `std::terminate` |
| `panic` en un callback | 142 |

Control negativo: `--crt-startup` con `NSC_LINKER=nslink.exe` se rechaza con el motivo.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t15-excepciones-ef09d2ke` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t15-excepciones-nn7_dhoq` |

## Límites

- Una excepción C++ no se convierte en una excepción NativeSharp ni se puede atrapar en NativeSharp.
- Una excepción NativeSharp lanzada dentro de un callback, con la biblioteca núcleo, no se probó.
