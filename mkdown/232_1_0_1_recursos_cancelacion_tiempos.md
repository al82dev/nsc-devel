# Entrega 232 — Recursos, cancelación y tiempos / Resources, cancellation, and timing

## Español

Se añadieron dos opciones ortogonales a toda invocación: `--time` informa milisegundos totales con
reloj monotónico incluso en salidas de error; `--timeout-ms=N` (1..86,400,000) fija un deadline de
cancelación cooperativa.

`ProjectCompilation` recibe una función de cancelación y la consulta entre todas sus fases. Si el
deadline vence, se detiene antes de construir el siguiente modelo y el diagnóstico lo nombra. No se
intenta terminar un hilo dentro de LLVM o del linker: eso podría dejar estado y artefactos corruptos.
La cancelación se observa en límites donde los modelos anteriores están completos y el siguiente no
ha comenzado.

Se modificaron `Driver.cpp`, `ProjectCompilation.h/.cpp`, la ayuda, el contrato `NSC-EXEC-001`, el
mapa de conformidad y `DriverTests`. Los tests cubren medición y rechazo de presupuestos inválidos;
la cadena de fases queda cubierta por el callback central. Se ejecutarán en el baseline tras 234.

## English

Two orthogonal invocation options were added. `--time` reports total milliseconds from a monotonic
clock on every exit path. `--timeout-ms=N` (1..86,400,000) establishes a cooperative cancellation
deadline.

`ProjectCompilation` receives a cancellation function and checks it between every model-building
phase. An expired deadline stops before the next phase and names that phase. LLVM and linker calls
are not forcibly interrupted because doing so could corrupt state or partial artifacts; the next
safe boundary observes cancellation.

Driver, ProjectCompilation, help, contract clause `NSC-EXEC-001`, conformance mapping, and Driver
tests were updated. The queued tests run in the single baseline after delivery 234.
