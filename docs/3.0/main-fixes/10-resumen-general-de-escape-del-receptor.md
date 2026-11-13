# Resumen general de escape del receptor para objetos de clase locales (M1-01)

Rama `development-main-fixes`. Mejora 1: `MyClass value;` se construye en el marco y se destruye al salir del
ámbito. Evidencia: `A:\nsc-main-fixes-evidencia\claude\m1-run` (ejecución O0/O2) y `logs\m1-01-*`.

## Decisiones (delegadas por el usuario en Claude)

1. **Escape no demostrado.** Si no se demuestra que el objeto local no escapa, el error se mantiene: «local
   class object has an unproven use or receiver effect». Nunca hay un `new` oculto.
2. **Lista blanca.** Se amplía con un resumen general en lugar de quitarla. La lista blanca anterior sigue
   aceptando lo que aceptaba.
3. **Controles.** Dos controles negativos pasan a positivos; se justifican abajo.

## Causa del límite

`safeCall` (`classLocalUsesAreProvenSafe`) solo aceptaba un método sobre el objeto local con estas
condiciones:

- argumentos escalares, y un resultado `void` o escalar;
- un resumen de efectos del receptor que solo conocía escrituras escalares.

Por eso un método tan simple como `box.Put(r)`, con un argumento de clase, rechazaba el objeto local
(sonda R10 de M2-07; `m1-run` antes del cambio: dos errores).

## Contrato del resumen (`classReceiverMayEscape`)

Un método conserva su receptor cuando se cumplen todas estas condiciones:

1. **Uso de `this`.** `this` solo aparece como base de un acceso a miembro.
2. **Campos no escalares** (de referencia, arreglo o struct) de `this`. Solo se usan como:
   - base de un acceso a miembro;
   - destino de una asignación `=`;
   - operando de `==` o `!=`;
   - elemento indexado usado de esas mismas formas.

   Un campo así nunca se retorna, se pasa como argumento, se convierte ni inicializa un local. Así, ningún
   objeto que el local posea (sus hijos `owned`) sale con él.
3. **Construcciones prohibidas.** No hay:
   - lambdas;
   - grupos de métodos del receptor usados como valor;
   - `&` ni `ref` de un campo;
   - ensamblador en línea;
   - bloques `unsafe`.
4. **Llamadas sobre `this` o `base`.** Toda llamada así está resuelta y no es virtual (o es una llamada a
   `base`), y su destino cumple este mismo resumen. La recursión se trata como escape.
5. **El método mismo.** Es de fuente, concreto, no `async`, no generador, no `unsafe` y sin retorno `ref`.

## Vía general en `safeCall`

Se acepta una llamada sobre el objeto local con cualquier argumento y cualquier tipo de resultado si se
cumplen estas condiciones:

- la llamada está resuelta, es de instancia, no es de interfaz ni de `base`, y no tiene paquete de
  parámetros;
- el destino efectivo (resuelto por tipo exacto cuando es virtual) no es precompilado ni abstracto;
- no tiene retorno `ref` ni suspensión, y no declara error;
- su receptor no escapa según el resumen.

Un argumento que nombra al propio objeto local sigue rechazado por el recorrido general, porque su padre no
es un acceso a miembro. Ese caso queda para M1-04.

## Controles que cambian (decisión 3)

- **`LocalOwnedChildReplacedByMethod`** (`Reset() { child = new Child(); }`): pasa a aceptado.
  - El hijo nuevo queda `owned` por el objeto del marco, y el anterior se destruye con la asignación.
  - `m1-run` lo ejecuta dos veces y exige 3 construcciones y 2 destrucciones de `Child` antes de salir del
    bloque, y 3 destrucciones después (PASS O0 y O2).
- **`LocalUserConversionArgument`** (conversión implícita de usuario en el argumento): pasa a aceptado. La
  conversión corre sobre el argumento, no sobre el receptor.

## Pruebas

- `nsc.class-value-origins` (125 comprobaciones), pasa.
  - Positivos nuevos:
    - `LocalReceiverReferenceArgument`, `LocalReceiverThisCall`, `LocalReceiverReturnsFieldMember`;
    - los dos controles movidos.
  - Negativos nuevos, que siguen rechazados:
    - `LocalReceiverReturnsThis`, `LocalReceiverPublishesThis`, `LocalReceiverReturnsReferenceField`;
    - `LocalReceiverStoresFieldInArgument`, `LocalReceiverPassesThis`.
  - Siguen rechazados los controles previos:
    - `LocalClassFieldEscape` (publicar un campo);
    - `LocalArrayFieldEscape` (retornar un arreglo en campo).
- **Ejecución O0/O2 de `m1-run`** (PASS en ambos):
  - `Box` en un bloque y en un método auxiliar;
  - argumentos de clase y llamadas sobre `this`;
  - conteo exacto de constructores y destructores;
  - `Holder` con un hijo `owned` reemplazado.

  El IR O0 muestra los objetos del marco como `%localN.object`; solo `Child` y `R` usan
  `__nativesharp_alloc`.
- Regresiones:
  - `owned-child-run`: PASS O0 y O2;
  - `OwnedNullCtor.nsproj`: «ok»;
  - `nsc.ns3.t06.escape-summaries`, `nsc.stage50_7.borrow-escape-analysis` y
    `nsc.stage56.c7-definite-assignment` pasan.

## Límites (pendientes del plan)

- **M1-02 a M1-04.** Pasar el objeto local como argumento, crear un alias, retornarlo, capturarlo o
  convertirlo a base o interfaz sigue rechazado.
- **Llamadas virtuales sobre `this` dentro del callee.** Siguen siendo escape. La resolución por tipo exacto
  solo se aplica a la llamada del llamador.
- **M1-06/M1-07.** Constructores y destructores siguen con las pruebas del modelo de construcción (documento
  01).
- **Llamadores con `throws`.** Un llamador que declara error sigue rechazando llamadas (regla previa).
