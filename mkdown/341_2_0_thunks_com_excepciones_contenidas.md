# Ítem 341 — Thunks COM con excepciones contenidas / Exception-safe COM thunks

## Resultado / Result

Una dirección publicada en una vtable COM nunca expone directamente un cuerpo NativeSharp que
pueda lanzar. La conversión a `extern C delegate` publica el thunk estable de frontera C, y ese
thunk contiene cualquier excepción antes de que alcance frames del consumidor. / An address
published in a COM vtable never directly exposes a potentially throwing NativeSharp body. The
conversion to `extern C delegate` publishes the stable C-boundary thunk, which contains any
exception before it can reach consumer frames.

## Política de fallo / Failure policy

Como una firma COM arbitraria no tiene un canal universal para traducir excepciones, el contrato
termina el proceso mediante `CallbackExceptionEscape` (razón 13, código observable 141) en vez de
permitir unwind extranjero. / Because an arbitrary COM signature has no universal exception
translation channel, the contract terminates through `CallbackExceptionEscape` (reason 13,
observable code 141) instead of allowing foreign unwind.

## Pruebas / Tests

La prueba instala en una vtable un método que lanza y lo invoca por el slot publicado. Inspecciona
el IR para exigir `boundary.catchswitch`, `boundary.catchpad` y el trap 13, y ejecuta O0 y O2
comprobando código 141. /
The test installs a throwing method in a vtable and invokes it through the published slot. It
inspects IR for the `cabi.unwind` handler and trap 13, then executes O0 and O2 expecting code 141.
