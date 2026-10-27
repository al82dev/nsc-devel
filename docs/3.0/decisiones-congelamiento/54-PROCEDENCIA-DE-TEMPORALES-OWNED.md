# Congelamiento 3.0, elemento 135 — núcleo de procedencia de temporales owned

Fecha: 2026-10-27. Revisión de partida: `0f75aeca0`.

## Nota de trabajo

```text
HEAD: 0f75aeca0
Elemento: 135 del plan de 151 (núcleo de procedencia y las seis formas de temporales)
Contrato vigente: C-11 y elementos 131a/131b (docs 51 y 52); decisión de Alberto (2026-10-27), abajo
Documento histórico que podría contradecirlo: doc 51, SCOPE LOCK («c ? Make().Name : "x" sigue sin destruir el
  temporal»); este elemento lo reemplaza
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/Internal/MethodBody/ExpressionResult.h
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  include/nativesharp/Frontend/ProjectOopHirModel.h
  src/Frontend/Bodies/MemberAccessAnalyzer.cpp
  src/Frontend/Bodies/CallAnalyzer.cpp
  src/Frontend/Bodies/StatementAnalyzer.cpp
  src/Frontend/Bodies/ExpressionCore/OperatorAndConditionalExpressions.cpp
  src/Frontend/HIR/HirExceptionAndCleanup.cpp
  src/LLVM/Internal/OopLlvmImplementation.h
  src/LLVM/Lowering/OopLlvmCleanup.cpp
  src/LLVM/Lowering/OopLlvmExpressionMachine.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

131a sólo seguía el préstamo de un temporal owned a través de lecturas de campo. Las demás formas por las que
pasa el préstamo quedaban fuera, y en cada una el temporal se fugaba o el préstamo se podía guardar. La columna
«Antes» sigue el contrato de 131a (doc 51, punto 3); sólo A10 (d37) se midió en el árbol de partida:

| Forma | Ejemplo | Antes |
|---|---|---|
| `?:` | `string s = c ? Make(3).Name : "x";` | compilaba; `Make(3)` no se destruía (A10) |
| `??` | `string s = Make(1).Name ?? "x";` | compilaba; sin destrucción |
| Resultado no owned de llamada | `string s = Make(6).GetName();`, `Id(Make(7).Name)` | compilaba; sin destrucción |
| Getter | `string s = Make(5).Title;` | compilaba; sin destrucción |
| Cadena de miembros | `Make(3).Next().Title.Length` | `Make(3)` y `Next()` sin destrucción |
| `(new T()).Campo` | `(new Box(5)).Id`, `(new Box(5)).Name` | el `Box` no se destruía |

## Decisión de Alberto (2026-10-27)

- El núcleo de procedencia cubre sólo fuentes cuya vida NSC conoce estáticamente. En este elemento, los
  temporales owned: el resultado de una llamada con retorno owned y `new` de una clase.
- Cierra las seis formas: `?:`, `??`, resultados de llamadas no owned, getters, cadenas de miembros y
  `(new T()).Campo`.
- El temporal nunca se destruye mientras viva una referencia derivada. El escape siempre se rechaza; la vida
  nunca se extiende.
- `?:` y `??` toman la unión de todas las ramas.
- Leer un campo de valor copiable no es un préstamo.

## Contrato

1. **Procedencia (análisis).** `borrowsOwnedTemporary` marca un valor referencia o string que apunta dentro de
   un temporal owned. La marca nace en:
   - una lectura de campo o un getter cuyo receptor es el temporal o un préstamo de él
     (`markOwnedTemporaryFieldBorrow`, `carriesOwnedTemporary`); el receptor puede ser una llamada con retorno
     owned o `new` de una clase;
   - una llamada cuyo resultado no es un owner nuevo, cuando su receptor o algún argumento lleva la marca
     (`markCallTemporaryBorrow`). La regla es conservadora: el análisis no sabe de qué entrada vino el
     resultado;
   - `?:` y `??`, con la unión de sus ramas.

   No la llevan:
   - un resultado que es un owner nuevo (retorno owned, concatenación);
   - un valor copiable (`Make(4).Id`).

   Los sumideros de 131a (local, asignación, `return`, `yield`) rechazan la marca:

   ```text
   the value borrows an owned temporary (the result of an owned-returning call or of 'new'), which is destroyed
   at the end of the full expression, so the local 's' cannot keep it; store the temporary in a raii or owned
   local first
   ```

   El diagnóstico nombra la expresión cuando tiene texto propio (`'Title'`); una llamada, `?:` o `??` dicen «the
   value».
2. **Dueño hasta la expresión completa (HIR).** `ownReceiversUntilFullExpression` da el dueño oculto de 131a
   también:
   - al receptor `new` de una lectura de campo;
   - a un préstamo que sigue por un resultado no owned de llamada o getter (`passesBorrowThrough`), por un `?:`
     o por un `??`;
   - a un préstamo que termina en un owner nuevo hecho a partir de él (`endsInNewOwner`: un string nuevo, o el
     objeto que devuelve un método llamado sobre el préstamo);
   - a un préstamo que termina en una escritura de campo a través de él.

   Si el flujo llega a algo que guarda el valor, el temporal no recibe dueño. El HIR nunca crea una referencia
   colgante por sí mismo, y el análisis rechaza esos flujos.
3. **Ramas.** Un préstamo que cruza un `?:` o `??` viene de un temporal que sólo un camino creó. Para esos casos:
   - su `FullExpressionCleanup` lleva `booleanValue`;
   - al entrar, el LLVM pone en null el slot de cada dueño puntero que contiene (`clearTemporaryOwnerSlots`);
   - la limpieza ya omite un dueño null, así que el temporal que no se creó no se destruye.

   Eso exige un dueño puntero (clase o string) y ninguna suspensión dentro de la expresión completa. Si no se
   cumple, queda la conducta anterior.

## SCOPE LOCK

- **Cambia.**
  - Las seis formas destruyen el temporal una vez, al final de la expresión completa.
  - Guardar, asignar, devolver o entregar con `yield` su préstamo se rechaza.
  - Hay un rechazo nuevo por diseño: `string s = c ? Make(3).Name : "x";` (sonda d37 A10, antes 300).
  - El texto del diagnóstico de 131a se generaliza. La frase que verifican las pruebas no cambia.
- **Idéntico.**
  - Las formas de 131a.
  - `(new T()).Metodo()` (131b).
  - Los argumentos owned (elemento 118).
  - Los locales `raii`/`owned`, las excepciones de `catch`, los objetos `scope` y el almacenamiento de
    corrutinas (elemento 136).
  - Las referencias no owned que vienen de parámetros o campos.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas `A:\temp\nsc-dec\d40` en `-O0` y
`-O2`, con el mismo resultado en los dos. `Log` acumula el `Id` de cada `Box` destruido:

| Sonda | Forma | Resultado |
|---|---|---|
| P1CondPlain | `(c ? Make(3).Name : Make(4).Name).Length` | 303: sólo se crea y destruye `Make(3)` |
| P2CondUntaken | `(c ? Make(3).Name : "xy").Length`, `c` falso | 200: la rama no tomada no destruye nada |
| P3CoalescePlain | `(Make(1).Name ?? "x").Length` | 301 |
| P19CoalesceUntaken | `(t ?? Make(9).Name).Length`, `t` no null | 200 |
| P6GetterPlain | `Make(5).Title.Length` | 305 |
| P8MethodPlain | `Make(6).GetName().Length` | 306 |
| P11IdPlain | `Id(Make(7).Name).Length` | 307 |
| P12NewField | `(new Box(5)).Id` | 55 |
| P14NewNamePlain | `(new Box(8)).Name.Length` | 308 |
| P15ConcatStored | `raii string s = Make(2).Name + "!";` | 402: owner nuevo; el temporal se destruye |
| P16ValueField | `Make(4).Id` | 44: valor copiable |
| P20Loop | `(i == 1 ? Make(i).Name : "xy").Length` en 3 vueltas | 701: una sola destrucción |
| P21Chain | `Make(3).Next().Title.Length` | 343: se destruye 4, luego 3 |
| Q1GetterAndMethod | getter + método + argumento en una expresión | 9765: 7, 6, 5 en orden inverso |
| Q2NewObject | `(new Box(5)).Id + (new Box(8)).Name.Length` | 885 |
| Q3NewOwnerAndValue | concatenación guardada y campo copiable | 4424 |
| P4, P5, P7, P9, P10, P13, P17, P18 | guardar `?:`, `??`, getter, método, argumento y `new` | rechazadas |

Sondas anteriores:

- d37 (131a): igual, salvo A10, rechazada por diseño.
- d38 (131b): igual.
- d36: T7 y T11 rechazadas; T8 42; T10 1; el resto sin cambio.

Resto (`seq.cmd f135` contra `f121`):

- `all.cmd` idéntico;
- `asynccases`, `gencases` y `loopcases` idénticos;
- los 66 programas de Freeze30 en `-O0` y `-O2`;
- los 27 proyectos de `tests/cases`;
- bibliotecas idénticas.

Cambia sólo la copia de MacroA en d28. Es la misma línea de 131b (doc 52), por el receptor `new` de
`Temporary()`.

Prueba nueva: `Freeze30ContractTests::testTemporaryProvenance`.

- Corren:
  - F135CondPlain: 303;
  - F135CondUntaken: 200;
  - F135CoalesceUntaken: 200;
  - F135GetterAndMethod: 9765;
  - F135NewObject: 885;
  - F135NewOwnerAndValue: 4424;
  - F135Loop: 701.
- Rechazadas: F135CondStored, F135CoalesceStored, F135GetterStored, F135MethodStored, F135ArgumentStored,
  F135NewStored y F135CondReturned.

Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el gate final.

## No verificado

- Batería completa y unitarias: gate final.
- Un `?:` o `??` cuya expresión completa suspende (`await`) conserva la conducta anterior: el temporal no recibe
  dueño.
