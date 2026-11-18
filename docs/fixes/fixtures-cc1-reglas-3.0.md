# Fixtures cc1 adaptadas a las reglas 3.0 de propiedad y vistas

Con el ejecutable de prueba vigente (ver `binario-de-prueba-vigente.md`), cuatro fixtures cc1
fallaban porque su código es inválido bajo reglas que entraron después de escribirlas. Las reglas
son intencionales y están documentadas; las fixtures se adaptan sin cambiar ninguna comprobación ni
código de salida esperado. Ninguna comprobación se debilita: un valor ausente o distinto sigue
devolviendo un código de fallo.

- **c25, c26 — `Optional<T>.Value()`** declara `throws InvalidOperationException` desde el contrato
  de errores 3.0 (C-13), y una llamada sin manejar se rechaza. `x.Value() != v` pasa a
  `x.IsEmpty() || x.UncheckedValue() != v`: ausente y distinto devuelven el mismo código que antes
  devolvía el valor distinto. Antes, un valor ausente escapaba como excepción.
- **c25 — `delete` de locales inferidos RAII.** `crece`, `w` y `buffer` se ligan directamente a
  `new`; desde el congelamiento 3.0 (elemento 141) son dueños que se liberan al salir del ámbito y
  el `delete` explícito se rechaza. Se retiran esos tres `delete`; `r` no es dueño inferido y
  conserva el suyo.
- **c26 — lectura con vista viva.** `r.ReadUInt32()` muta el lector y se rechaza mientras vive la
  vista `campoNombre` que devolvió `r.ReadSpan`. La vista queda en su propio bloque, que termina
  antes de la lectura siguiente.
- **c29 — `AllocateSpan` con span vivo.** `AllocateSpan` muta la arena y se rechaza mientras vive
  `bloque`. El span queda en su propio bloque. La propiedad de la arena (los punteros viejos siguen
  válidos al crecer) se mide en la sección 4 con punteros crudos y no cambia.
- **c23 — arreglos con vistas.** Un arreglo no se puede liberar con `delete[]` ni pasar a otra
  llamada mientras una vista suya está en el ámbito, y cada retorno temprano hacía `delete[]`. Los
  arreglos pasan a `raii` y cada grupo de vistas vive en su bloque. En `ReadOnlyIsReadOnly` el
  destino de `CopyTo` se toma antes que las vistas y la dirección se guarda en un puntero crudo
  para comparar en el bloque siguiente.

Lo que c23 ya no expresa: dos vistas creadas por llamadas distintas sobre el mismo arreglo a la vez
(`Spans.Of` y luego `Spans.OfRange` con la primera viva). El lenguaje lo rechaza porque la llamada
podría liberar el arreglo; un recorte de una vista existente (`all.Slice`) sí convive con ella y se
sigue comprobando.

La adaptación de `ReadOnlyIsReadOnly` dejó ver una imprecisión que se registra aparte
(NSC-NEW-004): una vista obtenida por método de un span creado por llamada tiene raíz desconocida, y
mientras vive se rechaza pasar a una llamada incluso un arreglo sin relación.

Resultado: las 71 fixtures cc1 y la comprobación `--rest` pasan con el ejecutable reconstruido.
