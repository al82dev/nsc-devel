# Ítem 338 — Consumo COM mediante vtables crudas / Raw-vtable COM consumption

## Resultado / Result

NativeSharp certifica el patrón binario mínimo de un consumidor COM: el objeto empieza con un
puntero a tabla, cada slot contiene una dirección de función `extern C` y la llamada reenvía el
puntero de interfaz como primer argumento. No se introduce un sistema de tipos COM ni wrappers
ocultos. / NativeSharp now certifies the minimum binary pattern for a COM consumer: the object
starts with a table pointer, every slot contains an `extern C` function address, and the call
forwards the interface pointer as its first argument. No COM type system or hidden wrappers are
introduced.

## Contrato ABI / ABI contract

La tabla se representa como `nint*`; el slot se convierte explícitamente al delegate `extern C`
con la firma correcta. Así, la selección de slot y la reinterpretación quedan visibles en el
código fuente, mientras el backend conserva la convención C Win64 ya certificada. / The table is
represented as `nint*`; a slot is explicitly converted to an `extern C` delegate with the correct
signature. Slot selection and reinterpretation therefore remain visible in source while the
backend preserves the already certified Win64 C convention.

## Pruebas / Tests

La prueba dirigida construye una interfaz de tres slots en memoria, carga el tercero, recupera el
estado a través de `this` y ejecuta la llamada indirecta en O0 y O2 con resultado 42. / The focused
test builds a three-slot interface in memory, loads the third slot, recovers state through `this`,
and executes the indirect call at O0 and O2 with result 42.
