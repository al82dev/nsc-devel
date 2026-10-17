# NSC 3.0.5 — R04C3: clasificación de `loop` y `comprehend`

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `9a93fb585`.
- **Objetivo exacto:** determinar si la profundidad de `loop` expression y comprehension pertenece
  a R04 (statements) o R03 (expresiones Pratt), y persistir cada arista pendiente sin implementar
  una solución parcial en el componente equivocado.
- **Naturaleza:** auditoría y clasificación; no hay cambio funcional ni refactor de producto.
- **Entrada exacta:** una expresión primaria `loop { ... }` o `comprehend(builder) { ... }` cuyo
  bloque contiene otra expresión de la misma familia, directa o dentro de otro statement.
- **Operación exacta:** `parsePrimaryExpression` entra a `parseLoopExpression` o
  `parseComprehensionExpression`; esas funciones llaman `parseBlock`, cuyo statement vuelve a
  `parseExpression` y a `parsePrimaryExpression`.
- **Invariante:** la eliminación de esta profundidad debe cubrir cualquier posición de expresión,
  no sólo expression statements dentro de un bloque.
- **Archivos autorizados:** este expediente y
  `A:\handover-deepseek\restantes-optimizacion-deepseek.md`.
- **Archivos protegidos:** todo código de producto, headers, AST, lexer, parser, semántica, HIR,
  MIR, LLVM, runtime, Driver, metadata, Builder, fixtures, manifiesto y expectativas.
- **Contratos congelados:** TP-PAR-385…394; forma de `LoopExpression`; label; recovery; builder
  evaluado una vez; nombres sintéticos monotónicos; stack LIFO de comprehension; lowering de
  `yield return`/`yield break`; orden, ranges, diagnostics, sintaxis, API, ABI y NSMETA 3.0.4.
- **Pruebas inmutables:** `ParserLoopComprehensionModule.cpp` y las entradas CP-PAR-033 del
  manifiesto. No se modifica ninguna prueba ni expectativa.
- **Prohibido:** introducir un caso especial sólo para expression statements, modificar el parser
  Pratt dentro de R04, alterar la sintaxis, agregar límite de profundidad, baseline, suite completa,
  deploy, instalación, push, pull o fetch.
- **Criterio exacto de terminación:** todas las aristas de estas dos familias quedan atribuidas a
  R03 mediante subítems explícitos; R04C queda sin código pendiente propio; el tracking conserva
  los contratos que deberán probarse al implementar R03.

## Presupuesto obligatorio de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Producto | ninguno |
| Documentación | este expediente |
| Tracking | archivo externo de pendientes |
| Pruebas | sólo inventariar cobertura existente |
| Configuración | ninguna |
| Perfil | auditoría estática |

## Inventario de aristas

### `loop` expression

1. `parseExpression`
2. `parseAssignmentExpression` y la cadena Pratt
3. `parsePrimaryExpression`
4. `parseLoopExpression`
5. `parseBlock`
6. `parseStatement`
7. cualquier consumidor de expresión
8. regreso a `parseExpression`

`loop` no posee token de statement ni dispatcher propio en `parseStatement`. Su única entrada es
`parsePrimaryExpression`. Puede aparecer como inicializador, operando, argumento, assignment,
return u otra posición de expresión. Interceptarlo únicamente cuando ocupa un expression statement
no elimina la misma arista en los demás consumidores.

### `comprehend` expression

1. `parseExpression`
2. cadena Pratt hasta `parsePrimaryExpression`
3. lookahead contextual `comprehend (...) {`
4. `parseComprehensionExpression`
5. `parseExpression` para el builder
6. push de `ComprehensionContext`
7. `parseBlock`
8. statements del cuerpo, incluido otro `comprehend`
9. pop del contexto y materialización de builder/loop/result

Además de la profundidad del bloque, esta familia conserva estado de expresión vivo mientras se
parsea el builder y el cuerpo. La migración correcta necesita continuaciones Pratt capaces de
mantener el contexto LIFO y ejecutar el pop tanto en éxito como en recovery. Una máquina exclusiva
de statements no posee ese contrato.

## Prueba contrafactual de pertenencia

- Si R04 reconociera `loop` o `comprehend` sólo desde `parseStatement`, una entrada como
  `value = loop { ... };` seguiría entrando por la cadena Pratt recursiva.
- Si R04 reemplazara solamente el `parseBlock` interno, el frame de `parseLoopExpression` o
  `parseComprehensionExpression` seguiría vivo mientras el bloque alcanza otra expresión.
- Por tanto, no existe una modificación local de R04 que elimine la profundidad completa sin
  duplicar o invadir la máquina de expresiones de R03.

El símbolo responsable no es el dispatcher statement ya migrado, sino la continuidad suspendida
de una expresión primaria a través de un bloque. R03 es la frontera causal mínima.

## Contratos de validación transferidos

R03 incorpora de forma explícita:

- `R03A`: `loop` expression en toda posición de expresión, body heap-backed, label y recovery;
- `R03B`: `comprehend`, builder único, contextos LIFO, nombres monotónicos, `yield`, recovery y
  controles statement anidados.

La cobertura existente que no puede alterarse es:

- TP-PAR-385/386: cuerpo y recovery de `loop`;
- TP-PAR-387/388/389: forma mínima y semántica parser de `yield`;
- TP-PAR-390/391: contextos anidados y restauración tras error;
- TP-PAR-392: nombres sintéticos monotónicos;
- TP-PAR-393: composición con `for`/`if`;
- TP-PAR-394: evaluación única del builder.

Estas pruebas cubren contratos superficiales, no profundidad extrema. R03 deberá agregar su
contrafactual profundo antes de modificar producto y demostrar que falla en `9a93fb585` por la
arista aquí inventariada.

## Resultado

R04C3 queda clasificado sin cambios de producto. `loop` y `comprehend` no se omiten ni se declaran
ya iterativos: permanecen pendientes como R03A y R03B. R04C queda cerrado porque sus dos familias
statement propias (`switch` y statement `match`) ya fueron migradas en R04C1/R04C2.

No se ejecutaron baseline, suite completa, deploy, instalación, push, pull ni fetch.
