# 372 — Jobs tipados / Typed jobs

## Español

`NativeSharp.Core.Threading.Job<T>` representa un trabajo de una sola ejecución cuyo resultado
también es `T`. `Schedule` lo entrega al `ThreadPool`, `Wait` ofrece espera con timeout,
`IsScheduled`/`IsCompleted` exponen el estado y `Result` espera la finalización antes de devolver el
valor. El resultado se publica mediante un evento de Win32 y estado atómico, sin almacenamiento no
tipado ni conversiones.

El productor se expresa como `JobFunction<T>`, una clase abstracta con `T Invoke()`. NativeSharp no
puede declarar todavía un delegado genérico cuyo tipo de retorno sea su propio parámetro genérico:
ese parámetro se introduce después de que el parser lee el retorno. La clase abstracta conserva el
contrato tipado y permite despacho polimórfico sin ocultar esa limitación del lenguaje.

Los objetos `JobFunction<T>` y `Job<T>` se prestan hasta finalizar el trabajo. El job conserva el
`ThreadStart` ligado como campo para que el callback encolado siga vivo. Una transición atómica
impide programar dos veces el mismo job; si la cola acotada lo rechaza, vuelve al estado listo para
permitir reintento.

La prueba ejecuta resultados `int` y `long`, valida estado, espera, resultado y rechazo de una
segunda programación, y repite el escenario en O0 y O2.

## English

`NativeSharp.Core.Threading.Job<T>` is a single-shot work item whose result is also `T`. `Schedule`
submits it to `ThreadPool`, `Wait` supports timed waiting, `IsScheduled`/`IsCompleted` expose state,
and `Result` waits for completion before returning the value. A Win32 event and atomic state publish
the result without untyped storage or casts.

The producer is `JobFunction<T>`, an abstract class with `T Invoke()`. NativeSharp cannot yet declare
a generic delegate whose return type is its own generic parameter because that parameter is
introduced after the parser reads the return type. The abstract class preserves a typed contract
and polymorphic dispatch while documenting that language limitation honestly.

`JobFunction<T>` and `Job<T>` objects are borrowed until completion. The job retains its bound
`ThreadStart` in a field so the queued callback stays alive. An atomic transition prevents scheduling
the same job twice; if the bounded queue rejects it, the job returns to ready state for retry.

The item test executes `int` and `long` results, checks state, waiting, result retrieval, and rejection
of a second schedule, then repeats the scenario at O0 and O2.
