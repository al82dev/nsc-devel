# Validación 164: oráculo de puntuación del lexer autoalojado

## SCOPE LOCK

- Objetivo: corregir exclusivamente el desfase de índice del oráculo
  `PunctuationMatrix()` para `TSH-00445`.
- Productor ejercitado: `SelfHostLexer.nsproj` en `Debug/O0`.
- Caso autorizado: `TSH-00445|Debug|O0`.
- Archivos autorizados:
  `tests/compiler-suite/selfhost/modules/SelfHostLexerFullModule.ns` y este
  documento.
- Fuera de alcance: modificar el lexer autoalojado, el manifiesto, Builder 3,
  `TRACKING.md`, los seis defectos conocidos observados en la tanda anterior o
  cualquier otro caso.
- Validación autorizada después del commit: una reejecución exacta de
  `TSH-00445` mediante la ruta canónica de Builder 3.

## Evidencia de entrada

La corrida dirigida del componente `SelfHost.Lexer` conservada en
`_nscbuilder3/history/20261010-164521-b0b52d06` compiló correctamente su
fixture y registró un único fallo no conocido:

```text
TSH-00445|Debug|O0
FAIL
exit code 47; expected 0
```

La confirmación exacta
`_nscbuilder3/history/20261010-164628-416afbf4` reprodujo el mismo resultado.
El código `47` pertenece a la rama falsa de `PunctuationMatrix()`; no es un
diagnóstico del compilador ni un fallo de construcción del fixture.

## Causa raíz confirmada

La entrada del oráculo comienza así:

```text
( ) { } [ ] ; , ? : ...
0 1 2 3 4 5 6 7 8 9 10
```

`Lexer.LexPunctuation()` publica `TokenKind.Question` para `?` y
`TokenKind.Colon` para `:`. Por tanto, `Question` ocupa el índice `8`. El
oráculo consultaba el índice `9`, que corresponde a `Colon`, y rechazaba una
secuencia correcta. Los índices posteriores comprobados por la misma función
ya coinciden con la secuencia real.

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`. No existe evidencia de defecto del
compilador para este caso.

## Corrección mínima

La única operación funcional de esta tanda cambia la consulta de
`t.KindAt(9)` a `t.KindAt(8)` en `PunctuationMatrix()`. No se modifica la
entrada, el contrato, el productor, el lexer, la cantidad de tokens, los demás
índices ni el resultado esperado.

## Contrato de validación

Los dos archivos de la tanda se commitean antes de ejecutar. Después del
commit se reejecuta únicamente `TSH-00445|Debug|O0` con NSC y NSLink ubicados
en `A:`. El resultado y sus rutas quedan en el historial inmutable de Builder
3; no se actualizará este documento después de la ejecución.

## Estado previo a validación

- Causa raíz: confirmada.
- Corrección: implementada.
- Commit funcional previo a pruebas: pendiente de crear.
- Reejecución exacta: pendiente.
- Cambios en producto o lexer: ninguno.
- Unidad `C:` modificada: no.
