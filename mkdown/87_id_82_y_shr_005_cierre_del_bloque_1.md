# 87 — ID 82 y SHR-005: el cierre del Bloque 1

**Fecha:** 2026-09-02
**Cierra:** ID 82 (asignación compuesta sobre propiedad e indexador) y SHR-005 (residuo de
*owned destination*). Con esto el Bloque 1 queda entero: 121, 122, 123, 89, 127, 82, SHR-005.
**Estado:** **CERRADO Y MEDIDO.** `242/242` en ctest, `Passed: 80  Failed: 0` en el checklist, y
`cc1_c20_property_compound` **ejecutada** a `-O0` y `-O2` devolviendo 42. La primera corrida dio
241/242 por una prueba que comprobaba el TEXTO de un diagnóstico que este cambio amplía — ver §8.

---

## 1. ID 82 — qué se rechazaba, y por qué el rechazo no era una pereza

El diagnóstico que estaba en el árbol:

```
compound assignment to a Stage 38 property/indexer is deferred;
use an explicit read-modify-write assignment
```

`p.X += 3` significa `set_X(p, get_X(p) + 3)`. Tres piezas —una llamada al getter, una operación,
una llamada al setter— y ninguna de ellas era nueva. Lo que faltaba era ligarlas.

Pero el motivo del aplazamiento estaba **escrito en el propio árbol**, en el comentario del nodo
`CompoundStore` (`ProjectOopHirModel.h`), que es el que resuelve `x += e` sobre un destino
corriente:

> *«POR QUE UN NODO PROPIO Y NO UN DESAZUCARADO. La forma obvia seria reescribir `x += e` como
> `x = x + e` aqui mismo. Es incorrecta: el destino se evaluaria DOS VECES, asi que `a[f()] += 1`
> llamaria a `f` dos veces y `*p++ += 1` avanzaria dos veces.»*

`CompoundStore` resuelve eso bajando **la dirección del destino una sola vez** (`children[0]`).
**Una propiedad no tiene dirección.** No hay nada que bajar una vez. Ese es el problema entero del
ID 82, y es la razón por la que el ID 81 lo dejó fuera en vez de arrastrarlo.

### 1.1 La medición que cambió el análisis

En la entrega anterior (doc 84 §7) escribí que cerrar el 82 requería *«añadir al HIR un nodo que
ligue un valor a un temporal y lo reuse — tres capas y un tipo de nodo nuevo»*. **Eso era correcto
sólo si la doble bajada fuera observable.** No lo comprobé; lo asumí.

Lo que la lectura de `lowerResolvedCall` (`ProjectOopHirModel.cpp:1439`) enseña es que **cada
llamada baja su receptor y sus argumentos desde los NODOS DE SINTAXIS**, construyendo un árbol
`unique_ptr` fresco cada vez. Bajar el mismo nodo de sintaxis dos veces no es un error de tipos ni
un `unique_ptr` compartido: son dos subárboles independientes que **calculan lo mismo dos veces**.

Y entonces la pregunta deja de ser «¿cómo comparto el nodo?» y pasa a ser **«¿para qué expresiones
calcular dos veces da lo mismo?»**. Esa pregunta sí tiene una respuesta demostrable.

### 1.2 La regla, y su demostración

`isAccessorDestinationReevaluable` (`ProjectMethodBodyModel.cpp`) acepta exactamente esto:

| Forma | Por qué la segunda bajada da lo mismo |
|---|---|
| `nullptr` (receptor implícito) | Es `this`. |
| `this` / `base` | Un puntero del marco. Ninguna llamada puede reasignarlo. |
| literales, `sizeof(T)`, `default(T)` | No se evalúan: son constantes del módulo. |
| identificador que resuelve a **local** o **parámetro** | Al getter que corre en medio **no se le pasa la dirección de ningún local ni parámetro del llamador**, así que no tiene vía para cambiar lo que la segunda lectura verá. |
| unario puro sobre un operando aceptado | `++`/`--` quedan fuera: escriben. |
| binario sobre dos operandos aceptados | Composición de lo anterior. |

**No es una aproximación conservadora sobre «efectos secundarios».** Es la clase de expresiones
para las que la doble bajada es un teorema: el valor leído la segunda vez no puede haber cambiado
porque no existe el camino por el que cambiaría.

Todo lo demás se rechaza con el **rango de fuente del subnodo culpable** y este mensaje:

```
compound assignment to a property/indexer evaluates its receiver and index arguments
once for the getter and once for the setter, so each has to be a local, a parameter,
`this` or a constant expression; assign this one to a local first and use that local
```

### 1.3 El campo, que parece inofensivo y no lo es

`h.inner.Prop += 1` leería `h.inner` dos veces. El getter de `Prop` corre entre las dos lecturas
**y sí alcanza los campos del objeto**, así que podría reescribir `inner`. C# evalúa el receptor
una vez y usa el valor viejo; nosotros usaríamos el nuevo.

Es exótico y es real. Rechazarlo con un diagnóstico que dice dónde está el problema es preferible
a divergir en silencio, que es la política que este proyecto ha seguido en todas partes. La
transformación que el mensaje pide —subirlo a un local— es una línea, y vuelve el caso aceptable
por la regla de arriba.

### 1.4 La regla aritmética es la que ya existía, no una segunda

El modelo semántico aplica **literalmente** las tres comprobaciones que la misma función aplica
unas líneas más abajo para `x += e` sobre un destino corriente:

1. `commonNumericType(destino, derecho)` tiene que existir.
2. El operando derecho tiene que convertir **implícitamente al tipo del destino**. Ésta es la
   divergencia deliberada respecto de C# que escribió el ID 81: C# mete un cast implícito de vuelta
   y **trunca** (`byte b; int n; b += n;` compila), y aquí se pide el cast escrito. Una propiedad no
   puede ser la excepción a esa regla.
3. La operación ocurre **entera en el tipo del destino**, que es exactamente lo que hace
   `CompoundStore`.

De (3) sale un detalle que importa: el argumento del setter se anota con el tipo del **getter**, no
con el del operando derecho. Así la conversión que registra `resolveSyntheticCall` es la identidad
y **no aparece una segunda tabla de promociones**.

### 1.5 La bajada

En el caso `AssignmentExpression` del HIR, cuando el token no es `=`:

```
InstanceCall(set_X)                   <- lowerResolvedCall(setterCall, args, preloweredLast)
  receptor                            <- de setterCall->receiverSyntax
  índices...                          <- de los nodos de índice
  Binary(op)   : tipo del destino     <- el `preloweredLast`
    InstanceCall(get_X)               <- lowerExpression(expression->first())
      receptor                        <- bajado OTRA VEZ
      índices...                      <- bajados OTRA VEZ
    Conversion(derecho -> destino)
```

El operando izquierdo de la operación **se obtiene bajando `expression->first()` en contexto de
valor**: los casos `IdentifierExpression`/`MemberExpression` e `IndexExpression` de esa misma
función ya saben convertir ese nodo en la llamada al getter, con su receptor y sus índices. **No
hay una segunda ruta de resolución de accesores**; se reusa la única que hay.

`lowerResolvedCall` recibe un tercer parámetro nuevo, `preloweredLast`, que **sustituye la bajada
del último argumento**. Tiene un solo llamador. Entrar por ahí en vez de armar el nodo de llamada a
mano es lo que mantiene el receptor, las conversiones seleccionadas y los borrows en un solo sitio.

**El orden de evaluación coincide con C#:** receptor, índices, getter, lado derecho, operación,
setter. El lado derecho —que **sí** puede tener efectos— se baja **una sola vez**, dentro del
`Binary`, después de la llamada al getter.

---

## 2. SHR-005 — el residuo era un sufijo de cadena

El doc 68 dejó SHR-005 como *«residuo acotado: arreglo fijo como destino, y verificar el agregado
por valor»*. Las dos mitades resultaron ser cosas distintas.

### 2.1 El agregado por valor: **medido, y no es un hueco**

La condición del modelo semántico y la del backend son **la misma**:

```cpp
// ProjectOopLlvmLowerer.cpp
requiresOwnedDestruction(t)  ==  (t == "builtin:string" || isClassReference(t))

// ProjectMethodBodyModel.cpp
ownedFieldOwnsStorage        ==  (isReferenceTypeKey(t)  || t == "builtin:string")
```

Las dos mitades de la regla —**exigir** una expresión de transferencia al escribir, y **liberar** el
valor desplazado al bajar— disparan sobre exactamente el mismo conjunto de tipos. Para un struct
por valor no dispara ninguna de las dos, que es lo coherente: no hay reserva separada que
transferir ni que liberar, y `owned` sobre un tipo por valor es el no-op documentado que siempre
fue.

**Esto no requería código.** Requería mirar las dos capas y comprobar que coinciden. Queda escrito
como comentario en el sitio, para que no se vuelva a abrir.

### 2.2 El arreglo fijo: sí era un hueco, y de la clase silenciosa

`arrayElementMoveTarget` y la comprobación de fuente de `move(...)` decidían las dos con
`ProjectTypeConversion::isNativeArray`, que es literalmente:

```cpp
return typeKey.size() >= 2 && typeKey.ends_with("[]");
```

Un arreglo fijo (`Resource[2]`) tiene el tipo `type:...Resource[2]`, que termina en `2]`. **No era
ninguna de las dos cosas:**

* `slots[i] = move(x)` — rechazado, porque el destino no admitía un move.
* `move(slots[i])` — rechazado con *«must be a direct native array element access»*.
* `slots[i] = alias` — **aceptado sin comprobar nada**. Éste es el lado silencioso.

No era una exclusión escrita en ningún sitio, ni justificada en ningún documento. Era el sufijo de
una cadena. Y el doc 68 nombra el arreglo fijo explícitamente como lo que quedaba de SHR-005.

### 2.3 Por qué es el mismo caso y no uno nuevo

Lo único que la bajada necesita de este ligado es **la dirección de la ranura**, y el HIR ya sabe
darla para un elemento de arreglo fijo (caso `RawElement`, «C2.1 fixed arrays»).
`MoveArrayElement` recibe esa dirección y hace el load-and-null de siempre, gobernado por
`elementIsReferenceLike`, que sale del **tipo del elemento**. Un arreglo fijo de un tipo por valor
sigue siendo el no-op que ya era.

**El contrato es el del arreglo nativo, no el del campo `owned`:** nada destruye los elementos
automáticamente. Un arreglo fijo es almacenamiento en línea sin longitud —el nivel crudo del
lenguaje, el mismo donde viven `*p` y `&x`— y su contenido lo gobierna quien lo escribió, igual que
`move(items[i])` sobre un `T[]` desde Stage 52. Lo que el move **sí** garantiza es que la ranura
queda en null, para que una lectura posterior no vea un valor ya transferido.

**La admisión es simétrica a propósito.** Si sólo se abriera un lado, `dst[i] = move(src[j])`
compilaría en una dirección y no en la otra.

---

## 3. Qué se tocó

| Archivo | Qué |
|---|---|
| `include/nativesharp/Frontend/ProjectMethodBodyModel.h` | Declaración de `isAccessorDestinationReevaluable`, con el argumento completo de por qué la doble bajada es sana sobre esa clase |
| `src/Frontend/ProjectMethodBodyModel.cpp` | **ID 82:** la función anterior; el rechazo sustituido por la resolución del setter con el valor anotado al tipo del destino; las tres comprobaciones aritméticas. **SHR-005:** las dos condiciones `isNativeArray` ampliadas con `fixedArrayElementTypeKey`, y el comentario que cierra la mitad «agregado por valor» |
| `src/Frontend/ProjectOopHirModel.cpp` | `lowerResolvedCall` gana `preloweredLast`; el caso `AssignmentExpression` arma `Binary(getter, derecho)` y lo pasa como último argumento del setter |
| `tests/cc1/c20_property_compound/` | Fixture de ejecución, salida 42 |
| `tests/unit/Cc1CorrectnessTests.cpp` | Registro de c20; cuatro `checkRejected` del ID 82; un `checkAccepted` de SHR-005 |
| `tests/unit/Stage50_11OwnershipExecutionTests.cpp` | Tres pruebas de SHR-005 **contando destrucciones**, que es como este proyecto verifica ownership |
| `tests/unit/Stage52GenericCollectionsCompilerFixesTests.cpp` | El fragmento esperado del diagnóstico de `move(...)`, que SHR-005 amplía — ver §8 |
| `run_windows_checklist.bat` | c20 en la lista de ejecución |
| `verify_manifest.py` | `mkdown` en `DIRECTORIES` |

Ningún script nuevo. `all.bat` y `run_windows_checklist.bat` siguen siendo los de siempre.

---

## 4. Cómo está medido

### ID 82 — la fixture cuenta, no sólo compara

`c20_property_compound` tiene una clase que **cuenta sus propias llamadas**. Ésa es la diferencia
entre «da el número correcto» y «el getter corrió una vez y el setter una vez»: un desazucarado que
evaluara el destino dos veces daría el mismo valor final en casi todos los casos y **sólo se
delataría en el conteo**.

* los diez operadores sobre una propiedad
* `GetCalls == 1` y `SetCalls == 1` por operación compuesta; y una asignación **simple** sigue
  siendo un setter y **cero** getters — el cambio no debe haber metido una lectura donde no la había
* `s.A += Probe()` llama a `Probe` **una** vez: el equivalente exacto de `buffer[Probe()] += 1` de
  la `c14`, movido al lado que aquí sí puede tener efectos
* el indexador con índice literal, local, parámetro y expresión sobre ellos
* `byte`: `200 + 55 = 255`, y **`255 + 1 = 0`** — escrito a propósito para fijar que la aritmética
  ocurre en `byte` y no en `int` con un guardado posterior
* receptor implícito (`Total += 10` dentro de la clase), que es el caso que un contenedor escribe
  de verdad

La mitad negativa vive en la suite, porque una fixture tiene que compilar entera: receptor que es
una llamada, índice que es una llamada, receptor que es un campo, y el estrechamiento del lado
derecho.

### SHR-005 — destrucciones contadas

Tres pruebas en `Stage50_11OwnershipExecutionTests`:

* `MoveIntoAndOutOfAFixedArraySlot` → **71**: el objeto transferido sigue vivo y legible (tag 7) y
  se destruye exactamente una vez. Si el move de entrada no transfiriera, sería 72.
* `SecondMoveOutOfAFixedArraySlotYieldsNull` → **1**: la ranura vaciada queda en null, un objeto una
  destrucción por muchas veces que se drene.
* `RelocationBetweenTwoFixedArraySlots` → **2**: la operación que el arreglo nativo ya tenía desde
  Stage 52 (`grown[i] = move(items[i])`), escrita sobre arreglos fijos.

---

## 5. Lo que este cambio NO hace, dicho en voz alta

**El valor de una asignación a accesor no es usable como expresión.** `int r = p.X = 5;` y
`int r = p.X += 5;` compilan en el modelo semántico (el resultado se anota con el tipo del getter)
pero la bajada devuelve el nodo de la llamada al **setter**, cuyo tipo es `void`. Es una
**limitación preexistente e idéntica para `=`**, no algo que el ID 82 introduzca: la fixture no la
usa, y cerrarla es su propio ítem, no un apéndice de éste.

**El receptor y los índices que no cumplen la regla se rechazan, no se aceptan con una advertencia.**
`h[f()] += 1` no compila. La transformación que el diagnóstico pide es una línea.

**El arreglo fijo no gana destrucción automática de sus elementos.** Gana exactamente lo que el
arreglo nativo ya tenía: `move` explícito y auditado, con null-store. Si algún día se quiere
destrucción automática de elementos, es otro ítem y hay que decidirlo — la entrada de destrucción
recorre campos `owned`, uno por campo, no N elementos por campo.

---

## 6. Estado del Bloque 1

| ID / SHR | Qué | Estado |
|---|---|---|
| 121 | Promoción entera en `commonNumericType` | cerrado y medido (doc 82) |
| 122 | Una sola representación de constante entera | cerrado y medido (doc 82) |
| 123 | Endurecimiento de `stackalloc` | cerrado y medido (doc 84) |
| 89 | `sbyte` | cerrado y medido (doc 84) |
| 127 | Normalización sin extensión de signo (P1, encontrado de paso) | cerrado y medido (doc 86) |
| **82** | **Compuesta sobre propiedad e indexador** | **cerrado y medido — c20 ejecuta y da 42** |
| **SHR-005** | **Residuo de owned destination** | **cerrado y medido — 71, 1 y 2 destrucciones** |

**El Bloque 1 está cerrado.**

**Sigue abierto y NO es trabajo de ingeniería, es decisión:** ID 126 (`byte + byte` → `byte`, con
desbordamiento silencioso; cerrarlo cambia el lenguaje), ID 87 (`volatile`: el doc 60 §16 y el
handover se contradicen), SHR-063 (condicionales de compilación: falta elegir el mecanismo),
ID 125 (UTF-8) e ID 124 (probablemente falso).

---

## 7. La corrida, y su resultado

```
all.bat
```

```
Passed: 80   Failed: 0
ctest:  100% tests passed out of 242
[ OK ] cc1_c20_property_compound -O0 -- exit 42
[ OK ] cc1_c20_property_compound -O2 -- exit 42
[ OK ] bug_triage -- 16/16, cero vivos
MANIFEST: 1222/1222 PASS
```

Las dos líneas de c20 son las que cierran el ID 82, y no las de ctest. **La lección del ID 127
sigue en pie:** bajo `ctest` en Windows las fixtures **compilan y enlazan pero no se ejecutan**. Un
`nsc.cc1.correctness` verde demuestra que c20 compila, **no** que dé 42. Eso lo dice el checklist,
y por eso c20 está en su lista de ejecución.

SHR-005 va por el otro camino y también está medido: `nsc.stage50-11.ownership-execution` **sí**
ejecuta sus programas, y sus tres casos nuevos devolvieron 71, 1 y 2 contando destrucciones.

El manifiesto pasa con **1222 entradas** — la corrida anterior no habría cubierto los ~120
documentos de `mkdown\`, que es lo que arregló la línea de `DIRECTORIES`.

---

## 8. La primera corrida: 241 de 242, y el fallo fue mío

```
99% tests passed, 1 tests failed out of 242
   81 - nsc.stage52.generic-collections-compiler-fixes (Failed)

FAIL: the rejection names the specific move()-array-element restriction, not an unrelated error
FAILED: 1 of 88 checks
```

**Lo que sí pasó, y es lo que importa:**

| Prueba | |
|---|---|
| `nsc.cc1.correctness` | **Passed** (45.7 s) — c20 compila y enlaza, las cuatro negativas del ID 82 fallan con el texto correcto, y el `checkAccepted` de SHR-005 compila |
| `nsc.stage50-11.ownership-execution` | **Passed** (30.5 s) — las tres pruebas de SHR-005 **cuentan destrucciones ejecutando**: 71, 1 y 2 |

Esas dos son las que miden lo entregado, y las dos están verdes. La de Stage 52 no medía nada de
esto: comprobaba que la negativa de `move(...)` sobre un indexador **nombra la restricción concreta
y no un error cualquiera de más arriba**, y lo hacía buscando la frase literal:

```cpp
diagnostic.message().find("move(...) array-element source must be a direct native array element access")
```

SHR-005 amplía esa restricción, así que amplía la frase: ahora dice `native array or fixed array
element access`. **La prueba seguía midiendo lo correcto; lo que cambió es la frase.** Se actualiza
el fragmento esperado, con el porqué escrito al lado.

**El error de método, dicho en claro:** cambié el texto de un diagnóstico **sin preguntar quién lo
estaba leyendo**. Un `grep` de la frase por `tests/` antes de tocarla habría costado diez segundos
y habría evitado una corrida entera de cuatro minutos. Un mensaje de diagnóstico no es un
comentario: es una salida observable, y en este árbol hay pruebas que la observan. La regla que
sale de aquí es corta — **antes de reescribir un diagnóstico, buscar su texto en `tests/`.**

---

## 9. Un cambio de herramienta que salió de esta ronda

Dos corridas seguidas se leyeron mal, y las dos veces por la misma causa: **`_logs\` no era de una
sola corrida.** `all.bat` llamaba a `run_windows_checklist.bat` **dos veces**, y la primera acción
del checklist es `rmdir /s /q _logs`. La segunda pasada borraba los logs de la primera, y lo que
quedaba tenía marcas de tiempo separadas por hora y media dentro de un directorio que se borra
entero al arrancar: un estado imposible que costó una lectura equivocada de un error de build que
ya no existía. Llamaba además a `run_bug_triage.bat` por separado, cuando el checklist ya lo
invoca en su línea 334.

`all.bat` ahora hace una sola pasada, en el orden que la limpieza impone —checklist, luego
manifiesto con su salida capturada, luego el reporte— y termina con el código de salida del
reporte.

Y `report.py`, nuevo, convierte `_logs\` entero en **`_logs\REPORTE.txt`**: veredicto primero, y
debajo el extracto del log de cada cosa que falló, ya recortado. Es **autocontenido** a propósito —
un reporte que dice «ver `02_ctest.log`» no ha ahorrado nada. Lleva dos cosas que salieron
directamente de los errores de esta ronda:

* **Aviso de logs de distinta cosecha.** Si las marcas de tiempo de `_logs\` se separan más de
  quince minutos, lo dice con las fechas delante.
* **La lista COMPLETA de fixtures**, no sólo las que fallaron. Bajo `ctest` en Windows las fixtures
  no se ejecutan, así que la única prueba de que una da 42 es su línea en esa lista. Un reporte que
  sólo listara fallos obligaría a abrir `SUMMARY.txt` para contestar «¿y la fixture nueva, corrió?»
  — que es justo la pregunta a hacerse después de añadir una, y justo el descuido que dejó pasar el
  ID 127 durante dos rondas.
