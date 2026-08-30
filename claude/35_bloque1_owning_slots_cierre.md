# Bloque 1 -- cierre de owning slots: la auditoria completa, resuelta

Respuesta a `NativeSharp_Auditoria_Bloque1_Item9_Ownership_Completa.md` (2026-08-29). La auditoria
confirmo el item 9 como implementado pero nego la certificacion de Bloque 1 por ocho huecos. Esta
ronda cierra los P0 y P1 de implementacion (§45 A-F) y regenera el packaging (§38-40). Queda
abierto, por decision de diseño con impacto de API, solo el alias plano hacia arrays (§45G).

Todo lo de abajo se midio ANTES de arreglarse y se re-midio despues, PE real bajo Wine, `-O0` y
`-O2`.

## El defecto de fondo (§13, §22, §25 de la auditoria)

La auditoria lo nombro exactamente: el release del valor desplazado colgaba de

```text
RHS is MoveExpression
```

o sea, de la **sintaxis** del lado derecho, no del **storage**. Medido:

| escritura a campo owned vivo | antes | ahora |
|---|---|---|
| `h.slot = move(x)` | libera (212) | libera (212) |
| `h.slot = null` | **fuga (0)** | libera (1) |
| `h.slot = new Res(2)` | **fuga del desplazado (1)** | libera (2) |
| `h.slot = ownedCall()` | **fuga del desplazado (1)** | libera (2) |
| `h.slot = aliasPlano` | **aceptado** (ownership silencioso) | **rechazado** |
| `h.slot = "literal"` | aceptado (puntero a global en slot que libera) | **rechazado** |

Y el dato que delato la forma del bug: la ruta de **propiedad** nunca tuvo el hueco (`h.Slot = null`
y `h.Slot = new` ya daban 1 y 2 correctos), porque el cuerpo del setter siempre deletrea el store
como `backing = move(value)`. El store directo era el unico que dejaba pasar la forma accidental
del RHS a la decision de lowering.

### La regla unificada

**Todo** write a un campo `owned` que posee storage (referencia o string nativo):

1. **libera lo desplazado**, sea cual sea la forma del RHS -- el flag del HIR ya no mira el RHS;
2. exige que el RHS sea una **expresion de transferencia**: `move(...)`, `new T(...)`, `null`, o
   una llamada owned-returning -- el mismo conjunto que los returns `owned` y la inicializacion de
   locals `owned` aceptan desde siempre (§15 opcion A, con precedente, no inventada);
3. `= null` es el **Clear** del lenguaje (§24): libera y deja vacio.

Los locals `raii` conservan su regla mas estricta ya testeada (move-only en asignacion); un campo
no distingue inicializacion de asignacion, asi que toma el conjunto completo.

Genericos: `owned T` cerrado sobre value type sigue siendo no-op -- el HIR flaggea sin mirar tipos
y el gate es `requiresOwnedDestruction` en el backend, la misma division de trabajo de todos los
cleanups.

### Literales de string: rechazo correcto, no regresion

`h.text = "literal"` compilaba. Un literal es un **global constante deduplicado**
(`lowerStringLiteral`): guardarlo en un slot que libera termina pasandole memoria estatica al
deallocator. La grafia honesta es `String.Copy(...)` (owned-returning call), `move(...)` o `null`.
Barrido de los 121 proyectos: nadie lo usaba.

## `delete h.slot` prohibido (§16-17)

Medido antes: `delete h.slot` destruia el objeto y **dejaba el puntero en el slot**; la entry de
destruccion del contenedor lo liberaba otra vez. 2 destrucciones de 1 objeto, garantizadas por el
compilador.

Prohibido en compilacion (la opcion que la auditoria recomendo):

```
error: explicit delete of owned field 'slot' is forbidden; the container releases it on
       destruction -- clear it with '= null', replace it, or take it with move(...)
```

No auto-null: acoplar `delete` a la identidad del lvalue no cubre el delete via alias y convierte
un operador de destruccion en un mutador silencioso de su operando. Los arrays `owned T[]` quedan
intactos: `delete items[i]` / `delete[] items` es el modelo manual del que `~List()` depende
(barrido: la libreria compila limpia).

## Overwrite de `owned string` (§18-19)

Medido antes: exit **5** (corrupcion) en `-O0` y `-O2` -- el release del desplazado leia el
*descriptor de clase* del valor anterior, y un string nativo no tiene descriptor: interpretaba sus
primeros 8 bytes como puntero. La travesia de destruccion final ya bifurcaba por
`isNativeString`; el overwrite ahora toma la misma bifurcacion (deallocator directo).

**Un tipo de slot, una semantica de destruccion** (§19): ya no existe "destruye distinto al morir
el contenedor que al sobreescribir". Medido despues: exit 0 en ambos niveles, con `String.Copy` +
`move` + lectura del contenido final.

## Move-out desde campo owned (§20-21)

Antes: `move(h.slot)` rechazado -- el campo era un **ownership sink** irreversible. Ahora, con la
semantica exacta que la auditoria pidio (tmp = slot; slot = null; owner(tmp), atomica para
ownership):

| sonda | resultado |
|---|---|
| `raii Res taken = move(h.slot)` | 1 destruccion, taken legible (21) |
| `return move(slot)` con this implicito (el idiom `Take()`) | 21 |
| slot queda limpio: holder muere despues del move-out | 1 |
| segundo move-out de slot vacio | null, sin destruccion extra (1) |
| ciclo move-in -> move-out -> move-in | 2 objetos, 2 destrucciones |
| move-out como argumento owned | 1 |
| generico value-type | no-op, lee el valor (7) |

Implementado como espejo del move desde elemento de array (Fix 4 de Stage 52): mismo contrato de
address + load + null-store, nodo propio (`MoveOwnedField`) para que los dumps digan la verdad.
`move(h.Slot)` sobre una PROPIEDAD se rechaza -- una lectura de propiedad es una llamada, no
storage que se pueda vaciar; el contenedor expone su `Take()`.

Como en arrays, el tracking estatico de moved-from NO se extiende a campos (a que objeto pertenece
un campo es un hecho de runtime); el slot vaciado queda en null y una lectura posterior ve un slot
vacio.

## Orden de destruccion: contrato fijado y ejecutado (§26-28)

Medido antes (nunca documentado ni testeado): todos los cuerpos primero (derivada->base), despues
todos los campos **base-first en orden de declaracion**. Eso invertia las dos propiedades utiles:
los campos de la base morian antes que los de la derivada que podian referenciarlos, y todos los
cuerpos corrian sin que ningun campo se hubiera liberado.

**Contrato adoptado** (inverso exacto de construccion -- la regla que los owners con scope ya
siguen, y la de C++ para subobjetos):

```text
cuerpo del destructor de la mas derivada
campos owned de la mas derivada, en orden INVERSO de declaracion
cuerpo del destructor de la base
campos owned de la base, en orden inverso
... hacia arriba
```

Dos propiedades load-bearing: un cuerpo de destructor corre siempre con los campos de SU clase
vivos (puede leerlos; el release null-guarded que sigue salta lo que ya descargo a mano), y los
campos de la derivada se liberan antes de que muera cualquier estado de la base -- nunca al reves.

Implementacion: `ProjectOopMirDispatchTable::destructionSteps` -- **una** secuencia ordenada que el
frontend congela y el backend ejecuta paso a paso, sin decisiones de orden propias, para que modelo
y codigo emitido no puedan divergir en silencio. El contrato completo vive en ese struct.

Ejecutado, codificado posicionalmente en base 4 para que un solo exit code capture presencia Y
orden: entrelazado derivada/base = **114** (el orden viejo daba 75); inverso de declaracion dentro
de una clase = **30**. `-O0` y `-O2`.

## Terminologia del getter corregida (§4, §41, §31-32)

La documentacion decia "el getter presta". Corregido en docs, comentario canonico
(`ownsAccessorStorage`) y tests: el getter devuelve una **referencia plana no-owner sin
provenance** -- la tercera categoria de §32, no un `borrowed` formal. Un alias que sobrevive a su
contenedor **queda colgante por diseño**, como una referencia cruda de C++, y no debe presentarse
como borrow safety.

## Suite (§33) y conteos (§10)

Correccion de conteo primero: la ronda 34 decia 42 escenarios; eran **43** (los checks, 128, si
cuadraban). Tras esta ronda:

**`Stage50_11OwnershipExecutionTests.cpp`: 57 escenarios / 176 checks.**

Nuevos (todos `-O0` y `-O2`): overwrite-null (1), overwrite-new (212), overwrite-owned-call (2),
move-out (21), `Take()` por this implicito (21), segundo move-out de slot vacio (1), ciclo
in-out-in (2), overwrite de owned string via `String.Copy` (0, contra NativeSharp.Core real),
orden entrelazado (114), orden inverso de declaracion (30); negativos con assert sobre el
mensaje: alias plano -> campo owned, `delete` de campo owned, literal -> campo owned string,
`move()` de propiedad.

## Packaging (§36-40)

- `verify_manifest.py` daba 56 fallos en la extraccion auditada. El manifest se regenero sobre el
  arbol actual (los `.exe` de `windows_execution_checklist` que ya no existen salen del set; los
  archivos nuevos entran) y la entrega se verifica con **0 failures dentro del zip extraido**,
  ademas de configure+build+ctest desde la extraccion aislada.
- Version sigue `0.1.0-dev55` / stage 55 (§42: sin cambio de release hasta indicacion explicita).

## Lo que queda abierto (unico item: §45G)

**Alias plano hacia un slot de ARRAY que descarga.** Para campos quedo cerrado en esta ronda (el
RHS de un campo owned exige transferencia). Para elementos de array sigue abierto y es la decision
que la auditoria misma clasifica como "decision de diseño + compiler fix + library migration":
endurecerlo rompe `AddRange`/`InsertRange` tal como estan escritos. Los datos para decidir estan en
`BUGS_ENCONTRADOS/bug_ownership_en_slots_de_contenedor.md`; la decision de API es de Alberto.

**Y una capacidad lista sin usar**: `List<T>` podria exponer ahora
`public owned T this[int i] { get; set; }` y `list[i] = move(item)` -- cambio de superficie
publica, tambien decision de Alberto (§6 de la auditoria coincide en no mezclarlo con el cierre
del compilador).

## Checklist §46 de la auditoria

```text
[x] overwrite-null runtime probe PASS O0/O2
[x] overwrite-new runtime probe PASS O0/O2
[x] delete-owned-field semantics fijada y testeada (prohibido)
[x] owned-string overwrite PASS O0/O2
[x] destruction order fijado y ejecutado
[~] plain alias / discharging slot: cerrado para campos; arrays = decision de API pendiente
[x] todos los negativos relevantes presentes
[x] full build warnings-as-errors PASS
[x] full CTest PASS (230/230)
[x] focused ownership suite PASS (176 checks)
[x] manifest regenerado
[x] verify_manifest.py PASS (0 failures, dentro del zip)
[x] fresh extraction + fresh configure + fresh rebuild
[x] documentacion reconciliada (43 escenarios corregido; "presta" -> referencia plana)
```
