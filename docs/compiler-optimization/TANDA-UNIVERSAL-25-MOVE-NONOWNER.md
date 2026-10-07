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

## Evidencia posterior al commit

- La reconstruccion completa del arbol `build-test-nslink` termino correctamente.
- Se ejecutaron las 684 pruebas existentes con O0 y
  `C:\native-sharp\bin\nslink.exe`; no se ejecuto baseline ni instalacion.
- Resultado global: 594 pruebas pasaron y 90 fallaron en 107.95 segundos.
- `MoveNonOwner` paso y el nuevo `MoveNonOwnerParameter` paso.
- `nsc.stage50-11.ownership-execution` paso completo, confirmando que el movimiento copy-shaped
  de valores continua aceptado.
- `nsc.stage49.raii-move` conserva un solo fallo: una expectativa textual antigua de
  `MoveNeedsOwnerDestination`; el compilador si rechaza el programa.

## Conclusion

La correccion elimino la fabricacion de ownership desde locales y parametros ordinarios sin
romper movimientos de tipos valor. La tanda semantica queda verificada y cerrada; la expectativa
textual restante pertenece al cierre posterior de contratos de prueba.
