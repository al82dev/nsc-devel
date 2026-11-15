# Reasignación de dueños, segundo move, préstamos y punteros crudos

Fichas LKF-018, LKF-102, LKF-103 y LKF-104.

## Restricción levantada

- **LKF-018 / LKF-103.** Un local `raii`, y desde el elemento 141 también un local ligado a `new`,
  solo aceptaba `target = move(source)`. Por eso `a = new R(2)` se rechazaba ("RAII locals are
  move-only"), y un local movido no podía reactivarse con `new` ni con una llamada que retorna
  `owned`. `validateRaiiAssignmentTarget` acepta ahora un dueño nuevo: `new T(...)`, `new T[n]`,
  `null` o una llamada `owned`. El HIR ya trataba toda asignación directa a un local RAII como
  almacenamiento propietario, así que el objeto anterior se destruye antes del store, y el local
  queda vivo otra vez. Se comprobó con contadores de destrucción: reemplazo simple, en bucle, con
  `null`, con llamada `owned`, reactivación tras `move` y arreglo `raii` con elementos.
- Siguen rechazados: el resultado de una llamada sin `owned` (el diagnóstico enumera ahora las formas
  aceptadas), reemplazar un dueño con un préstamo vivo y el auto-move.
- **Segundo move de campo.** `move(h.f)` deja el campo en `null`; leerlo después da `null` y solo se
  destruye un objeto.

## Fronteras con diagnóstico explícito (LKF-104)

- El resultado `borrowed` de una llamada se usa en línea, y el préstamo pasa por llamadas anidadas.
- Ligar un local `borrowed` a un retorno `borrowed` se rechaza, también con lifetime nombrado
  (`borrowed[a]`): "borrowed local requires a direct live owner or borrowed identifier source". Un
  local `borrowed` desde un dueño vivo se acepta.
- Retornar un campo `owned` como `borrowed` sin lifetime declarado se rechaza ("borrowed return
  requires a borrowed value whose lifetime outlives the call"); la forma con `borrowed[a]` coincidente
  es la de `ab410f95e`.
- Esto es una capacidad no implementada y no un defecto: no hay relaciones de lifetime entre retorno
  y local. No se propone eludirla con punteros.

## Punteros crudos (LKF-102)

Un puntero sin calificar es el nivel crudo, igual que los arreglos fijos: no se comprueban nulidad,
alineación ni límites. Desreferenciar `null` es comportamiento indefinido (0xC0000005 en O0; en O2
el optimizador puede eliminar la carga). Un acceso desalineado funciona en x64. Los calificadores se
comprueban en compilación: `int*!` no admite `null` y `align(16)` no se deriva de un origen que no lo
prueba ("pointer initializer does not satisfy").

## Fixture actualizado

`nsc_stage50_8_ownership_ir_lowering_tests` esperaba un único `owned-temporary-borrow`, el de la cadena
`"a" + "b"`. Desde el elemento 118 (`1a4cb8062`) cada valor entregado a un parámetro `owned`
(`Consume(move(second))`, `Forward(move(third))`) también tiene un dueño oculto hasta la llamada. La
prueba exige ahora un temporal de cadena entre tres temporales, cada uno con su limpieza.

## Prueba

`nsc.compiler.owner-reassignment-borrow-boundaries`: 24 casos, cada rechazo con su control, O0/O2.
