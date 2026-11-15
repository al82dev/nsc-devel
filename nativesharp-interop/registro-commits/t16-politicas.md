# T16-009 a T16-011: varias DLL, propiedad de memoria y recursos del CRT

## Comprobación

Este cambio define políticas a partir del código y de los fixtures de la tanda; no cambia código de
producción.

## Políticas

- **Runtime NativeSharp con varias DLL** (T16-009): cada imagen NativeSharp, ejecutable o DLL, enlaza su
  propia copia del runtime NativeSharp (`WindowsExecutableBuilder` y `WindowsLibraryBuilder` reciben
  `configuredWindowsRuntimeLibrary()`); no hay estado del runtime compartido entre imágenes. Una DLL C++
  lleva su propio runtime de C y C++ estático. En los fixtures DLL de la tanda, el ejecutable usa la entrada
  de NativeSharp y la DLL C++ su arranque, sin interferir ([arranque](t16-arranque.md), [TLS](t16-tls.md),
  [atómicos](t16-atomicos.md)).
- **Memoria** (T16-010): libera la imagen que reserva. La memoria que reserva una biblioteca C++ (`new`,
  `malloc`) la libera la misma biblioteca, por un destructor o una función de liberación que expone; un
  objeto NativeSharp lo libera el runtime NativeSharp. Con `/MT` cada imagen tiene su copia del CRT, así que
  liberar en otra imagen no está soportado aunque llegue a funcionar.
- **Recursos del CRT** (T16-011): `FILE*`, descriptores, `errno`, `locale`, manejadores de `atexit` y estado
  de `rand` son de la copia del CRT que los crea; no atraviesan el límite de una DLL. Una biblioteca que los
  expone debe exponer también las operaciones sobre ellos.

## Evidencia

El código citado y los fixtures DLL de la tanda. No hay un rechazo automático: NSC no ve qué hace una
biblioteca con la memoria que recibe.

## Límites

- Con el runtime dinámico de MSVC (`/MD`) las imágenes comparten el CRT; no se probó.
