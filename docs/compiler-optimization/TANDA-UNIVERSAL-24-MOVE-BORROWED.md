# Tanda universal 24: `move` no convierte un prestamo en propietario

## Alcance bloqueado

- Se corrige exclusivamente la admision de `move(...)` sobre locales y parametros `borrowed`.
- Se conserva `move(...)` copy-shaped para valores copiables y genericos cerrados sobre valores.
- No se cambia el lowering, el ABI, NSMETA ni la representacion fisica de ownership.
- No se construyen librerias, no se ejecuta baseline y no se instala nada en `C:`.

## Causa confirmada

La ruta copy-shaped aceptaba cualquier local o parametro que no requiriera movimiento destructivo.
Un parametro `borrowed Resource` caia en esa ruta: el analizador borraba su ownership y su
procedencia, producia un valor sin calificador y dejaba que continuara la resolucion de llamada.

Eso es semanticamente incorrecto. Un prestamo no posee el almacenamiento y `move(view)` no puede
fabricar un propietario. El problema estaba en el productor comun `MoveExpression`, no en las dos
pruebas que lo detectaban.

## Cambio

Antes de clasificar el movimiento copy-shaped, el analizador rechaza un local o parametro
`borrowed` cuyo tipo realmente puede tener propietario. Los tipos valor sin ownership fisico
siguen usando la regla copy-shaped existente.

## Archivos

- `src/Frontend/ProjectMethodBodyModel.cpp`
  - Conserva la procedencia borrowed y rechaza la transferencia imposible.
- `docs/compiler-optimization/TANDA-UNIVERSAL-24-MOVE-BORROWED.md`
  - Registra causa, limites, decision y evidencia.

## Verificacion pendiente del commit

- Reconstruir el arbol de pruebas existente.
- Ejecutar las 684 pruebas completas configuradas con O0 y NSLink.
- Confirmar que Stage 50.4 y Stage 50.10 cierran sin regresiones de move/copy generico.
