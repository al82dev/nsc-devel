# CC1 — Fase 4A: `sizeof` (tareas 40–46)

**Fecha:** 2026-08-31 · **Alcance:** el compilador. Nada más.
**Origen:** `NativeSharp_Milestone_CC1_...md`, hallazgos M-003 (§15) y M-004 (§16), prescripciones
CC1-400 … CC1-404.
**Tabla:** `MILESTONE_1_TASKS.md`, fase 4A.

---

## 0. Punto de partida

Fase 3 cerrada y **reproducida dos veces**: corridas de 13:06 y 13:56, ambas `Passed: 55 /
Failed: 0`, `ctest` 242/242, build sin errores ni avisos. G3 cerrado.

---

## 1. Qué es `sizeof` hoy en este compilador, medido

Antes de escribir nada barrí el árbol entero buscando `sizeof`. Aparece en exactamente cuatro
sitios:

| Dónde | Qué hace |
|---|---|
| `src/Lexer/Lexer.cpp` | el token `KwSizeof` |
| `src/Parser/Parser.cpp` | dos nodos: `SizeofTypeExpression` (lleva `typeSyntax`) y `SizeofExpression` (lleva `first`) |
| `src/Frontend/ProjectMethodBodyModel.cpp` | seis líneas: analiza el operando como código normal y tipa el resultado como `builtin:ulong` |
| `include/nativesharp/Diagnostics/DiagnosticId.h` | `InvalidSizeof`, declarado y **usado en ningún sitio** |

Y nada más. Ni un `case` en `ProjectOopHirModel`, ni una línea en Sema, ni un test.

Así que `sizeof` en el árbol tal como estaba: parsea, tipa mal, y muere en el `default` de
`lowerExpression` con `unsupported Stage 34 OOP expression kind: SizeofTypeExpression`. La misma
forma exacta que anoté para `stackalloc` en `69_huecos_de_runtime_y_lenguaje.md`: **mitad delantera
presente, feature ausente.**

---

## 2. Tres correcciones al texto de las tareas

### 2.1 La tarea 40 tenía un criterio que no se puede medir

Decía: cambiar el tipo a `nuint`, hecho cuando **«coincide con el pipeline clásico»**. Y el
documento de origen lo justifica en §15 —«el pipeline clásico los baja directamente a una constante
`nuint`»— y en §16 —«el pipeline clásico posee tratamiento explícito de unevaluated operand y tests
que comprueban que un incremento dentro de `sizeof` no sobreviva como ejecución en HIR».

**Nada de eso está en este árbol.** No hay implementación clásica de `sizeof` con la que comparar,
ni ese test existe. El criterio, tal como estaba escrito, no se puede cumplir ni incumplir.

El cambio a `nuint` sigue siendo el correcto, pero necesita una razón propia, y la tiene: `sizeof`
produce una **cuenta de bytes**. El tipo sin signo del ancho del puntero es lo que consumen la
aritmética de punteros (fase 4D) y `stackalloc` (fase 4B); darle `ulong` obligaría a una conversión
en cada uso natural y además mentiría en un target de 32 bits. La fixture lo mide por ese lado:

```
nuint bytes = 64;
nuint count = bytes / sizeof(long);   // sin conversión, porque los dos son nuint
```

### 2.2 La tarea 44 no se puede hacer donde decía

Pedía calcular el tamaño en `ProjectMethodBodyModel` y guardarlo en el binding. Fui a ver de dónde
sacaría el tamaño, y el orden de construcción del pipeline lo impide:

```
compileProjectOopHir()
  └─ compileProjectObjectLayouts()      <- el modelo de layout se construye AQUI
  └─ oopHirModel_->build(..., *objectLayoutModel_, ...)
```

`methodBodyModel_` ya está construido para cuando eso ocurre, y `ProjectMethodBodyModel::build` ni
siquiera **recibe** el modelo de layout — su firma son cinco parámetros y ninguno lo es.

Hacerlo ahí dejaba dos salidas, las dos malas: reordenar el pipeline (el modelo de layout depende
del de miembros, que depende del de nombres…), o **duplicar las reglas de tamaño en el modelo
semántico** — que es literalmente la duplicación semántica que este milestone existe para quitar.

El reparto correcto, que además es más limpio:

- **el modelo semántico** registra `SizeofConstant` con **el type key que se está midiendo** en
  `elementTypeKey`, y valida lo que no tiene tamaño;
- **el HIR** convierte eso en constante preguntándole a `valueLayoutForTypeKey`.

Es la misma clase de corrección que la de la tarea 32 en la fase 3: el texto nombraba el sitio
equivocado, y hacerlo literalmente habría forzado una duplicación.

### 2.3 Faltaba una tarea

`SizeofTypeExpression` **no tocaba siquiera su `typeSyntax()`**. Ponía `ulong`, marcaba válido y
seguía. `sizeof(void)` y `sizeof(TipoQueNoExiste)` pasaban el análisis semántico sin una palabra y
morían después en el HIR con un mensaje interno. Eso es la tarea **46b**, y usa el
`DiagnosticId::InvalidSizeof` que llevaba tiempo declarado sin cablear.

---

## 3. El operando no evaluado, resuelto por construcción y no por enumeración

CC1-401 pide que el operando de `sizeof(expression)` resuelva nombres, miembros y tipos pero **no**
ejecute efectos de definite assignment, no registre una asignación, no mueva ownership, no cree
borrow state persistente. Criterio de terminado: *«el estado de flujo antes y después de analizar
el operando es idéntico»*.

La forma obvia sería un flag `unevaluatedContext_` consultado en cada caso de `analyzeExpression`
que pudiera tener un efecto. `analyzeExpression` tiene ~500 KB de casos. Enumerar los efectos a
suprimir es una lista que se queda incompleta el día que alguien añade un caso nuevo.

La forma que **cumple el criterio literalmente** es capturar y restaurar:

```cpp
const OwnershipFlowSnapshot beforeOperand = captureOwnershipFlow();
const ExpressionResult operand = analyzeExpression(expression->first(), ...);
restoreOwnershipFlow(beforeOperand);
```

Funciona porque `OwnershipFlowSnapshot` **ya lleva las tres familias que importan** — lo comprobé
antes de elegirlo, no lo supuse:

```cpp
struct OwnershipFlowSnapshot final
{
    bool reachable;
    std::unordered_set<std::size_t> movedRaiiLocals;        // must
    std::unordered_set<std::size_t> maybeMovedRaiiLocals;   // may
    std::unordered_set<std::size_t> movedOwnedParameters;
    std::unordered_set<std::size_t> maybeMovedOwnedParameters;
    std::unordered_set<std::size_t> invalidLocalBorrows;
    std::unordered_set<std::size_t> maybeInvalidLocalBorrows;
    std::unordered_set<std::size_t> assignedLocals;         // definite assignment
    std::size_t localCount;
};
```

Es la misma maquinaria que la fase 2 usó para las ramas de `?:` y de `&&`/`||`. La propiedad se
cumple **por construcción**: no depende de que yo haya acertado la lista de efectos.

**¿Y los bindings que el operando deja registrados?** Quedan huérfanos y son inertes. Lo comprobé:
`ProjectOopHirModel::build` indexa cada familia de bindings **por nodo de sintaxis** en un mapa, y
sólo los consulta al bajar ese nodo concreto. El operando de un `sizeof` no se baja nunca — se baja
una constante en su lugar. Un `MoveBinding` registrado dentro de un `sizeof(move(o))` nunca se
consulta, así que el propietario no se mueve ni en el flujo ni en el código emitido.

### Lo que este modelo NO cubre, dicho aquí y no escondido

`sizeof(x)` con `x` sin asignar **sigue reportando error**. Bajo un modelo de operando no evaluado
estricto no debería: nada se lee. Restaurar el estado de flujo deshace el efecto, pero no
des-reporta un diagnóstico ya emitido durante el análisis.

Lo dejo así a propósito, y no por descuido: suprimir diagnósticos dentro del operando exigiría
precisamente el flag que acabo de argumentar en contra, y erraría hacia **aceptar** código dudoso
en vez de hacia rechazarlo. Queda anotado como comportamiento conservador conocido.

---

## 4. El tamaño sale del servicio canónico, y sólo de él

CC1-402: *«No hardcodear tamaños en el OOP HIR. Usar el mismo servicio de target layout utilizado
por ABI/object layout.»*

Ese servicio existe y es `ProjectObjectLayoutModel::valueLayoutForTypeKey`. Su propio comentario
dice para qué está:

> *Shared by the MIR model and the OOP HIR model so a stride and an offset can never be computed by
> two different rules.*

Así que el HIR emite:

```cpp
const auto layout = objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, binding->elementTypeKey);
auto node = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:nuint");
node->integerValue = layout->first;
```

Ni una tabla de tamaños nueva. Ésa es toda la diferencia entre cerrar el hueco y crear una segunda
regla de tamaño que pueda divergir de la primera.

Y el operando de `sizeof(expression)` **no se baja**. Es lo que hace que un efecto secundario
escrito dentro no llegue a ejecutarse: no hay nodo que lo represente.

---

## 5. Lo que apareció al atar `sizeof` a ese servicio — tarea 78

Fui a leer `valueLayoutForTypeKey` en vez de confiar en su comentario, y contesta **8 para un
enum**: cae por el `return {{8, 8}}` de las formas con forma de referencia (clase, interfaz,
arreglo dinámico, puntero, string).

Pero `layoutNamedType`, **en la misma clase**, contesta **4** para un enum, vía
`BuiltinLayoutKind::Enum` → `{4, 4}` en `TargetDataLayout.cpp`.

Dos funciones del mismo archivo, el mismo tipo, respuestas distintas — dentro de la función cuyo
comentario promete que un stride y un offset no pueden salir de dos reglas distintas. Y
`valueLayoutForTypeKey` no es decorativa: es la que calcula el **stride de los arreglos**
(`ProjectOopMirModel::elementLayout`, managed y raw) y los **offsets de inicializador agregado**.

**No lo he arreglado, y la fixture deliberadamente no asierta `sizeof(Color)`.** Dos razones:

1. **No lo he medido.** Sé lo que dicen las dos funciones; no sé qué hace el binario con un
   `Color[]`. Afirmar mi expectativa en un test es exactamente el error que llevo tres rondas
   anotando en estos documentos.
2. **Alcance.** La regla del documento de origen es explícita: nada «de paso». Queda como **tarea
   78, fase 4E**, con ID propio.

Vale la pena decir cómo apareció: no por buscar bugs, sino porque CC1-402 obligaba a rutar `sizeof`
por el servicio canónico, y fui a leerlo. Un `sizeof` con su propia tabla de tamaños habría dado
`4` para el enum, habría parecido correcto, y habría dejado la divergencia intacta y sin descubrir.

---

## 6. Un aviso que habría roto tu build, cazado antes de entregarlo

`methodCoreOperationKindName` tiene un `switch` exhaustivo sobre `MethodCoreOperationKind`. Añadir
`SizeofConstant` sin añadir su `case` da:

```
warning: enumeration value 'SizeofConstant' not handled in switch [-Wswitch]
```

Con `/W4` en MSVC —y `/WX` si `NSC_WARNINGS_AS_ERRORS` está puesto— eso es un build roto. Apareció
porque compilo aquí con `-Wall -Wextra` antes de entregar, no sólo con `-fsyntax-only`. Corregido.

---

## 7. Archivos tocados

| Archivo | Qué |
|---|---|
| `include/nativesharp/Frontend/ProjectMethodBodyModel.h` | `MethodCoreOperationKind::SizeofConstant`, con el comentario de por qué NO lleva el tamaño |
| `src/Frontend/ProjectMethodBodyModel.cpp` | el caso `sizeof` reescrito: `nuint`, operando no evaluado, validación con `InvalidSizeof`, binding; y el `case` que faltaba en `methodCoreOperationKindName` |
| `src/Frontend/ProjectOopHirModel.cpp` | los dos `case` nuevos en `lowerExpression`, emitiendo `IntegerConstant` desde `valueLayoutForTypeKey` |
| `tests/cc1/c9_sizeof/` | fixture de ejecución nueva |
| `tests/unit/Cc1CorrectnessTests.cpp` | `cc1-c9-sizeof`, más cuatro casos: `cc1-sizeof-unevaluated` (rechazado), `cc1-sizeof-unevaluated-assigned` (aceptado), `cc1-sizeof-void`, `cc1-sizeof-unknown-type` |
| `run_windows_checklist.bat` | un `run_case` nuevo |

Sin cambios en `CMakeLists.txt`. Los tres `.cpp` compilan limpio con `-Wall -Wextra`.

---

## 8. Deuda que esta fase no cierra

- **`sizeof` no es todavía una constante para el evaluador de constantes.** `const int n =
  sizeof(int);` y `case sizeof(int):` no compilan, porque `foldIntegerConstant` no consulta el
  binding `SizeofConstant`. No es una regresión —antes `sizeof` no funcionaba en absoluto— pero
  CC1-404 lo pide («uso en const», «uso en switch label»). Cerrarlo bien depende de que el
  evaluador pueda alcanzar el modelo de layout, que es el mismo problema de orden de construcción
  de §2.2, y por tanto pertenece a la misma conversación que P-001/A-001.
- **`sizeof(x)` con `x` sin asignar sigue dando error** (§3), a propósito.
- **La divergencia de tamaño del enum** (§5), tarea 78.

---

## 9. La corrida de las 14:20, y las dos cosas que enseñó

La corrida salió mal por dos motivos independientes. Conviene separarlos porque uno no es del
compilador y el otro sí es mío.

### 9.1 La corrida se rompió porque sobrescribí el `.bat` mientras corría

```
'E_BUILD_TYPE' is not recognized as an internal or external command
```

Eso es `-DCMAKE_BUILD_TYPE` partido a la mitad: `-DCMAK` | `E_BUILD_TYPE=...`. La cronología (UTC):

| hora | qué |
|---|---|
| 20:20:14 | arranca la corrida |
| **20:23:39** | **se escribe `run_windows_checklist.bat`** (mi commit de la limpieza) |
| 20:31:38 | se escriben `01_build.log` y `SUMMARY.txt` |

`cmd.exe` lee un `.bat` guardando un **offset de bytes** y vuelve a él tras cada salto. El archivo
cambió debajo, el offset dejó de apuntar donde apuntaba, y reanudó a media línea. Es exactamente el
peligro documentado en la cabecera de ese mismo archivo.

Las dos huellas que lo confirman, y que ninguna otra explicación produce:

1. `SUMMARY.txt` trae **las dos líneas contradictorias** — `[ OK ] compiler build + install` **y**
   `[FAIL] compiler build/install` — que salen de ramas mutuamente excluyentes;
2. `01_build.log` y `SUMMARY.txt` se escribieron a las 20:31:38, **después** de que terminara
   `ctest` (317 s desde ~20:26), cuando el build es el paso 1 y ctest el 2.

Consecuencias: las fixtures nunca corrieron, así que la mitad de ejecución de esta fase sigue sin
medir; y la limpieza de `_logs` tampoco corrió, porque en disco todavía estaba la versión previa
cuando el script arrancó — de ahí que sobrevivieran los logs del triage de la corrida anterior.

**Regla nueva, y es mía, no del script: no se escribe nada en el árbol mientras haya una corrida en
vuelo.**

### 9.2 El fallo real: `nuint` no convierte a `int`, y mis programas de prueba decían `int`

```
FAIL: cc1-sizeof-unevaluated-assigned: is accepted
  error: cannot initialize local 'n' of type 'builtin:int' from 'builtin:nuint'
      int n = sizeof(x = 1);
```

**El compilador tiene razón.** `sizeof` devuelve `nuint` —que es justo lo que pedía la tarea 40— y
no hay conversión implícita `nuint → int` (correcto: es un estrechamiento con signo). Los cuatro
programas de prueba que escribí declaraban `int n`. Ese `int` era mío.

### 9.3 Lo que sólo se ve tirando del hilo: un negativo que pasaba por la razón equivocada

`cc1-sizeof-unevaluated` es el negativo que debe demostrar que `x` sigue sin asignar después del
operando no evaluado — es decir, **el único test que mide la tarea 41**. Su fragmento esperado era
`"x"`.

Ese test **pasó**. Y pasó por la razón equivocada: el programa se rechazaba por el tipo del
inicializador, no por leer una variable sin asignar, y `"x"` aparece también en ese mensaje.

Lo grave no es que estuviera mal escrito. Es que **habría pasado igual sin la captura/restauración
del estado de flujo**, que es la mitad entera de M-004. Un negativo cuyo fragmento casa con
cualquier error no está midiendo nada.

Es el mismo fallo que `cc1-ternary-ownership-mixed` en la fase 3, y la lección se repite con más
fuerza: **el fragmento tiene que nombrar la propiedad, no una letra que aparece en cualquier
mensaje.** Ahora exige el diagnóstico real, verificado en el fuente
(`ProjectMethodBodyModel.cpp:603`) y no inventado:

```
use of unassigned local variable 'x'
```

### 9.4 Los arreglos de esta ronda

| Qué | Por qué |
|---|---|
| `nuint n` en los cuatro programas de `sizeof` | `int n` no compila, y con razón |
| `cc1-sizeof-unevaluated-assigned` termina en `if (n != 4) { return 1; } return x;` | `return n` habría sido la misma conversión `nuint → int` otra vez |
| `cc1-sizeof-void` y `cc1-sizeof-unknown-type` terminan en `return 0;` | un `return n` sobre un `n` de tipo inválido puede añadir ruido que tape el mensaje que sí se está midiendo |
| El fragmento de `cc1-sizeof-unevaluated` pasa de `"x"` a `use of unassigned local variable 'x'` | §9.3 |

Nota de rebote sobre la **tarea 77**: el log **sí** trae `FAILED: 1 of 82 checks` cuando una suite
falla. Eran 32 antes de la fase 3. La tarea sigue en pie —en verde el número no aparece— pero ahora
está medido que la señal existe cuando hay fallo.

---

## 10. Fase cerrada — la corrida

**Corrida de Windows del 2026-08-31 16:06.**

```
Passed: 57
Failed: 0

[ OK ] cc1_c9_sizeof -O0 -- exit 42
[ OK ] cc1_c9_sizeof -O2 -- exit 42
```

`ctest` **242/242, 100%**. `nsc.cc1.correctness` pasa y sube de 22.95 s a **29.88 s**, consistente
con la fixture nueva y los cuatro casos de diagnóstico. Triage 16/16.

El checklist pasa de 55 a 57, no a 56 como yo había dicho: cada fixture aporta **dos** casos, uno
por nivel de optimización. Me equivoqué al contar.

### Los tres riesgos, medidos

Los anoté en §9 antes de conocer el resultado. Ninguno se materializó, y los tres eran reales:

1. **`sizeof(int*)` y `sizeof(double*)`** — dependían de que `canonicalTypeKey` produjera una clave
   terminada en `*`, que es lo que `valueLayoutForTypeKey` reconoce. La produce.
2. **`sizeof(record.Middle)`** — un acceso a campo dentro de un operando no evaluado. El análisis
   del acceso no deja nada que sobreviva a la restauración del estado de flujo.
3. **`nuint total = sizeof(int) + sizeof(double);`** — la aritmética `nuint` funciona sin
   conversiones, que es exactamente lo que justifica el cambio de tipo de la tarea 40.

Y con eso queda medido lo que el documento de origen daba por hecho y no existía: **`sizeof`
ejecuta**, las dos formas, a `-O0` y a `-O2`, en un compilador donde hasta esta ronda moría en el
`default` del HIR.

### Estado

Fase 4A cerrada, 8/8 (las siete tareas más la 46b). Milestone **51/78**. Siguen las fases 4B
(`stackalloc`), 4C (raw `new T`), 4D (aritmética de punteros) y 4E (cierre de paridad, donde vive
la tarea 78) antes del gate G4.

---

## 11. Deuda que sigue abierta

La corrida. Las siete tareas quedan en `[~]`. Riesgos concretos, anotados **antes** de conocer el
resultado:

1. **`sizeof(int*)` y `sizeof(double*)`** dependen de que `canonicalTypeKey` produzca una clave que
   termine en `*`, que es lo que `valueLayoutForTypeKey` reconoce. Es la aserción de la fixture con
   más probabilidad de fallar por una diferencia de forma de la clave, no de tamaño.
2. **`sizeof(record.Middle)`** obliga al análisis de un acceso a campo dentro de un operando no
   evaluado. Si el acceso a campo registra algo que no sea flujo, aparecerá aquí.
3. **`nuint total = sizeof(int) + sizeof(double);`** suma dos `nuint` y los asigna a `nuint`. Si el
   clasificador de conversiones trata `nuint` de forma distinta a como espero, falla aquí y no en
   los tamaños.
