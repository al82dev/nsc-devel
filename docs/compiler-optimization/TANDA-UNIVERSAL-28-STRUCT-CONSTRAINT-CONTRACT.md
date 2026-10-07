# Tanda universal 28: `where T : struct` es una capacidad positiva

## Alcance bloqueado

- Se actualiza exclusivamente el caso CC1 que aun describia `struct` como constraint no
  implementado.
- Se conserva toda la matriz negativa de constraints mal formados o no satisfechos.
- No se modifica el compilador, ABI, NSMETA, librerias ni runtime.
- No se ejecuta baseline ni se instala nada en `C:`.

## Causa confirmada

T04 ya implementa y prueba `where T : struct`, incluida la negativa que rechaza una clase con el
diagnostico `requires a value type`. CC1 conservaba una fixture anterior que esperaba el mensaje
`not supported yet`; el compilador aceptaba correctamente el programa y la prueba lo trataba como
fallo.

## Cambio

La fixture historica pasa a ser positiva y materializa `Caja<Valor>` con un `struct` real. De este
modo comprueba parsing, validacion del constraint y cierre generico, en lugar de limitarse a una
declaracion sin uso.

## Archivos

- `tests/unit/Cc1CorrectnessTests.cpp`
  - Convierte la limitacion obsoleta en contrato positivo materializado.
- `docs/compiler-optimization/TANDA-UNIVERSAL-28-STRUCT-CONSTRAINT-CONTRACT.md`
  - Registra causa, cobertura complementaria y limites.

## Verificacion pendiente del commit

- Reconstruir `nsc_cc1_correctness_tests`.
- Ejecutar las 684 pruebas completas configuradas con O0 y NSLink.
- Confirmar que CC1 y T04 declarative constraints pasan juntos.
