# Tanda universal 26: contrato estable del destino de `move`

## Alcance bloqueado

- Se corrige exclusivamente la asercion fragil de `MoveNeedsOwnerDestination`.
- El programa invalido debe seguir siendo rechazado.
- La prueba exige el identificador estable `InvalidOperand`, no una frase exacta.
- No se modifica el compilador, ABI, NSMETA, librerias ni runtime.
- No se ejecuta baseline ni se instala nada en `C:`.

## Causa confirmada

El compilador rechazaba correctamente `Resource b = move(a)` cuando `a` era un propietario RAII.
La prueba fallaba porque comparaba una frase de Stage 49 con el diagnostico vigente:
`owned return value cannot initialize a non-owning local; use a raii owner destination`.
El cambio de redaccion no altero la regla semantica ni el identificador del diagnostico.

## Cambio

El helper de Stage 49 permite omitir la comparacion textual cuando el caso proporciona un
`DiagnosticId`. `MoveNeedsOwnerDestination` conserva sus dos pruebas: rechazo obligatorio y
presencia de `InvalidOperand`.

## Archivos

- `tests/unit/Stage49RaiiMoveSemanticsTests.cpp`
  - Reemplaza la frase exacta por el identificador estable en este caso.
- `docs/compiler-optimization/TANDA-UNIVERSAL-26-MOVE-DIAGNOSTIC-CONTRACT.md`
  - Registra causa, limites y evidencia.

## Evidencia posterior al commit

- La reconstruccion completa del arbol `build-test-nslink` termino correctamente.
- Se ejecutaron las 684 pruebas existentes con O0 y
  `C:\native-sharp\bin\nslink.exe`; no se ejecuto baseline ni instalacion.
- Resultado global: 595 pruebas pasaron y 89 fallaron en 106.16 segundos.
- `nsc.stage49.raii-move` paso sus 87 checks.
- El programa con destino no propietario continuo rechazado mediante `InvalidOperand`.

## Conclusion

La prueba conserva el contrato semantico y deja de depender de una frase mutable. La tanda queda
verificada y cerrada.
