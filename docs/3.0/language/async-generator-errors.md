# Errores y reanudación de generadores async / Async generator errors and resumption

## ES — Contrato normativo

Un generador async (`AsyncIterator<T, E>`) es una corrutina con frame residente: cada
`MoveNextAsync` reanuda justo después del `yield return` en el que se detuvo. Ninguna sentencia
anterior se vuelve a ejecutar y cada local vive en el frame del iterador hasta su destrucción.

Un error que sale del cuerpo del generador —un `throw` sin `catch` local o un `await` cuyo error
no se captura dentro— llega completo al future de ese `MoveNextAsync`:

- antes de entregarlo, el generador corre una vez los `finally` y las destrucciones activas;
- el iterador queda terminado: un `MoveNextAsync` posterior responde `false` sin ejecutar nada;
- un `catch` alrededor del `await` recibe el mismo objeto, sea el llamador síncrono o async;
- sin `catch`, el error se propaga como el de una llamada (C-13 exige declararlo) y se destruyen
  los locales vivos del consumidor; desde `Main` termina el proceso con la excepción nativa.

`Cancel()` deja un iterador detenido en el estado de cancelación de su `yield`: el siguiente
`MoveNextAsync` corre la limpieza activa ahí y responde `false`. Destruir un iterador detenido
corre esa misma limpieza una vez. Un iterador que nunca empezó no ejecuta nada en ninguno de los
dos casos.

## EN — Normative contract

An async generator (`AsyncIterator<T, E>`) is a frame-resident coroutine: each `MoveNextAsync`
resumes right after the `yield return` it stopped at. No earlier statement runs again and every
local lives in the iterator frame until it is destroyed.

An error that leaves the generator body —a `throw` without a local `catch`, or an `await` whose
error is not caught inside— reaches the future of that `MoveNextAsync` whole: the generator first
runs its active `finally` blocks and destructions once, then stays finished (a later
`MoveNextAsync` answers `false` and runs nothing). A `catch` around the `await` receives the same
object from a synchronous or an async caller; without one the error propagates like a call's
(C-13 requires it to be declared) and the consumer's live locals are destroyed.

`Cancel()` parks a suspended iterator in the cancel state of its `yield`: the next `MoveNextAsync`
runs the cleanup active there and answers `false`. Dropping a suspended iterator runs that cleanup
once. An iterator that never started runs nothing in either case.

Pruebas / Tests: `tests/unit/Freeze30ContractTests.cpp` (`testAsyncGeneratorErrors`,
`testAsyncGeneratorResume`) y `TL-LANG-520`.
