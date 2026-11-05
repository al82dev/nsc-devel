# Tanda 08: no diagnosticar una demanda de global especulativa

- La validación agrupada del 2026-11-05 falló al compilar `Shadow.ns.txt`: `C enum global type conflicts with another type: HiddenState`, aunque las referencias eran a un parámetro y un local NativeSharp.
- La observación de nombres ocurre antes de resolver variables locales. Se deja sin seleccionar un enum externo conflictivo en esta observación en vez de producir un error inmediato; si el global se utiliza realmente, su proyección sigue sin estar disponible y debe rechazarse.
- Pendiente: repetir la batería agrupada y comprobar tanto la sombra como el rechazo de un uso real conflictivo. La corrección no demuestra todas las reglas de búsqueda y colisión.
