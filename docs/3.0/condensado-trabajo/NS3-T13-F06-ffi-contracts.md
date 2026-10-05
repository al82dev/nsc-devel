# NS3-T13-F06 — Contratos FFI / FFI contracts

## Español

NativeSharp 3.0 permite describir en la firma `extern C` las obligaciones que antes quedaban
solamente en documentación externa:

- `T* buffer length(count)` enlaza un puntero de datos con un parámetro entero distinto de la
  misma firma. Un nombre inexistente, un enlace consigo mismo, un contador no entero o un valor
  que no sea puntero se rechazan durante el análisis semántico.
- `fn* C(...) -> R callback(scoped)` declara que el código foráneo no conserva el callback tras
  regresar. `callback(escaping)` exige una transferencia `owned`; un préstamo no puede escapar.
- `unwind(forbidden)` documenta que el callee no desenrolla a través de C. `unwind(abort)` permite
  un adaptador que termine el proceso ante una violación. Un contrato unwind en una función que
  no sea `extern C` es inválido.
- La nullabilidad (`*?`/`*!`) y ownership (`borrowed`/`owned`) usan los contratos de puntero y
  movimiento ya normativos en 3.0; una llamada no obtiene una excepción especial por ser FFI.
- Las funciones `extern C` siguen sin poder declarar `throws`: ningún error tipado o panic cruza
  implícitamente la frontera C.

NSMETA 1.38 publica el índice y nombre del parámetro de longitud, la vida del callback y la
política unwind. El consumidor reconstruye la misma declaración y vuelve a ejecutar las mismas
validaciones, de modo que LibraryReference no borra seguridad.

```ns
extern C from "native.dll"
{
    public int Read(byte*! data length(bytes), nuint bytes) unwind(forbidden);
    public int Subscribe(owned fn* C(int) -> void cb callback(escaping));
}
```

## English

NativeSharp 3.0 can state C-boundary obligations directly in an `extern C` signature:

- `T* buffer length(count)` links a data pointer to a distinct integral parameter in the same
  signature. Unknown/self links, non-integral counts, and non-pointer buffers are rejected.
- `callback(scoped)` promises that foreign code does not retain the callback after return.
  `callback(escaping)` requires an `owned` lifetime transfer.
- `unwind(forbidden)` states that the callee cannot unwind through C; `unwind(abort)` selects a
  terminating boundary strategy. These policies are invalid on non-FFI callables.
- Pointer nullability and ownership keep their ordinary NativeSharp 3.0 rules, and typed errors
  never cross a C boundary implicitly.

NSMETA 1.38 preserves the buffer link, callback lifetime, and unwind policy. A LibraryReference
consumer therefore reconstructs and validates the same contract instead of silently weakening it.

## Verificación / Verification

`nsc_ns3_t13_ffi_contract_tests` covers an O0 executable linked with NSLink, valid buffer and
callback declarations, invalid links/lifetimes, and an NSMETA 1.38 round trip. The directed result
for this change was `PASS: 5 checks`.
