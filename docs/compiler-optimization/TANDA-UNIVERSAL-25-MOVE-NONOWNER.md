# Tanda universal 25: `move` no fabrica ownership

## Alcance bloqueado

- Se corrige exclusivamente `move(...)` sobre referencias ordinarias sin ownership.
- Se cubren locales y parametros.
- Se conserva el movimiento copy-shaped para tipos valor copiables y genericos valor.
- No se cambia ABI, NSMETA, lowering, librerias ni runtime.
- No se ejecuta baseline ni se instala nada en `C:`.

## Causa confirmada

El productor comun de `MoveExpression` clasificaba como copy-shaped cualquier fuente que no fuera
RAII, `owned` o move-only. Esa clasificacion tambien incluia referencias de clase ordinarias. Como
resultado, `raii Resource target = move(nonOwner)` podia fabricar ownership desde una fuente que
nunca lo tuvo.

## Cambio

Antes de admitir la ruta copy-shaped, el analizador comprueba si el tipo de la fuente puede tener
propietario. Si puede y la fuente no es RAII, `owned` o move-only, se emite el diagnostico estable
que exige una fuente propietaria. La regla se aplica de igual forma a locales y parametros.

## Archivos

- `src/Frontend/ProjectMethodBodyModel.cpp`
  - Impide fabricar ownership desde locales y parametros ordinarios.
- `tests/unit/Stage49RaiiMoveSemanticsTests.cpp`
  - Conserva el caso historico de local y agrega cobertura simetrica para parametro.
- `docs/compiler-optimization/TANDA-UNIVERSAL-25-MOVE-NONOWNER.md`
  - Registra causa, limites, decision y verificacion.

## Verificacion pendiente del commit

- Reconstruir el arbol de pruebas existente.
- Ejecutar las 684 pruebas completas configuradas con O0 y NSLink.
- Confirmar que los contratos `MoveNonOwner` pasan y que los movimientos de valores siguen
  aceptados.
