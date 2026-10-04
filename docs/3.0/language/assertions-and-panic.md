# Assertions and panic / Aserciones y panic

## ES — Contrato normativo

NativeSharp 3 ofrece cuatro intrínsecos contextuales:

- `assert(condicion)` comprueba un `bool` y termina con la razón estable de assertion si es falsa.
- `unreachable()` declara una ruta imposible, tiene tipo `never` y termina si se ejecuta.
- `unsafe { assume(condicion); }` comunica una premisa al optimizador. No comprueba la condición;
  si es falsa, el programa tiene comportamiento indefinido.
- `panic()` o `panic(mensaje)` divergen. El mensaje, cuando existe, debe ser `string`.

El proyecto elige `<PanicProfile>abort</PanicProfile>` —valor predeterminado— o
`<PanicProfile>unwind-native</PanicProfile>`. `abort` no ejecuta cleanup. `unwind-native` ejecuta
los cleanups nativos activos, pero no puede ser capturado por `catch` tipado ni cruzar de forma
implícita una frontera FFI.

`<PanicHandler>Namespace.Funcion</PanicHandler>` registra un hook opcional con firma exacta
`void(string message, string file, int line, int column)`. Se llama antes de abortar o desenrollar;
si retorna, continúa el perfil seleccionado. No se asigna un objeto de excepción administrado.

## EN — Normative contract

NativeSharp 3 provides four contextual intrinsics. `assert(bool)` is checked and traps on failure;
`unreachable()` is a diverging promise that traps if reached; `assume(bool)` is allowed only in an
explicit unsafe context and a false premise is undefined behavior; `panic()` and `panic(string)`
diverge.

The project selects the default `abort` profile or `unwind-native`. Abort skips lexical cleanup.
Native unwind executes active cleanup but is not a typed language exception and may not cross an
FFI boundary implicitly.

An optional project `PanicHandler` must name a non-throwing namespace function with signature
`void(string message, string file, int line, int column)`. It runs before the selected termination
profile and receives the payload plus source location. The implementation uses native Win64
control flow and introduces no CLR dependency or mandatory heap allocation.
