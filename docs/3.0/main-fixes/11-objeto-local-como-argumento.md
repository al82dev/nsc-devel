# Objeto de clase local pasado como argumento (M1-02 y M1-04)

Rama `development-main-fixes`. Continúa el documento 10. Evidencia: `A:\nsc-main-fixes-evidencia\claude\m1-run`
(sección `Reader`) y `logs\m1-02-*`.

## Decisión 1 (delegada por el usuario en Claude): contrato de escape de parámetros de clase

El resumen publicado de escape de parámetros (`parameterEscapeSummaries_` en NSMETA) **no cambia**.
`parameterMayCarryEscape` sigue excluyendo los parámetros de referencia de clase por valor, y su clase
sigue siendo NoEscape por omisión para los demás consumidores.

En su lugar se agregó un resumen nuevo, interno y conservador, que solo usan los objetos de clase locales:
`classParameterMayEscape(callable, parámetro)`. Así ninguna biblioteca ni ningún metadato publicado cambia de
significado, y el objeto local solo se acepta con una prueba propia.

## Contrato de `classParameterMayEscape`

El parámetro (por valor, sin ownership) puede aparecer solo como:

1. **Base de un acceso a campo.** Si el campo no es escalar, su valor debe quedar contenido: base de un
   miembro, destino de una asignación, operando de una comparación, o elemento indexado usado igual.
2. **Receptor de una llamada.** La llamada está resuelta y no es virtual, de interfaz ni con retorno `ref`, y
   su receptor no escapa (`classReceiverMayEscape`, documento 10).
3. **Argumento de otra llamada.** Con conversión identidad, a una llamada resuelta no virtual cuyo parámetro
   tampoco escapa (composición memoizada; la recursión cuenta como escape).
4. **Operando de `==` o `!=`.**

Además, el método debe cumplir:

- es de fuente, concreto, síncrono y no `unsafe`;
- no tiene lambdas, ensamblador ni bloques `unsafe`.

Cualquier otra aparición del parámetro es escape: retornarlo, guardarlo, crear un alias, reasignarlo,
capturarlo o tomarlo por `ref` o `&`.

## Uso desde el llamador (`classLocalUsesAreProvenSafe`)

El objeto local puede ser argumento por valor, con conversión identidad, de un callable con estas
condiciones:

- resuelto, no virtual, no de interfaz, no abstracto y no precompilado;
- sin error declarado;
- su parámetro no escapa.

El llamador tampoco puede declarar error.

## Pruebas

- `nsc.class-value-origins`, pasa.
  - Positivos: `LocalArgumentReadOnly`, `LocalArgumentForwarded`.
  - Negativos, que siguen rechazados:
    - `LocalArgumentStored`, `LocalArgumentReturned`;
    - `LocalArgumentPublishedByMethod`, `LocalArgumentOwnedFieldLeaked`;
    - `LocalArgumentVirtualCallee`.
- `m1-run` (O0 y O2, PASS):
  - `Reader.ReadTwice(passed)` compone una llamada estática, un método del objeto y un argumento reenviado;
  - el objeto queda en el marco (`%local*.object = alloca`);
  - se destruye una vez (`Counter.boxDtors == 3`).
- `nsc.ns3.t06.escape-summaries`, `nsc.stage50_7.borrow-escape-analysis` y
  `nsc.stage56.c7-definite-assignment` pasan.

## Límites

- **Argumentos con conversión a base o interfaz**, y callees virtuales: siguen rechazados. Una llamada
  virtual sobre el parámetro dentro del callee también.
- **Alias locales.** Fuera de la prueba de préstamo existente, siguen rechazados.
- **Otros usos del local.** Retorno, captura y conversión siguen rechazados.
