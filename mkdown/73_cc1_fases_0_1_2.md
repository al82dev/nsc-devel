# 73 — CC1, fases 0–2: identidad, constantes tipadas y flujo real

**Fecha:** 2026-08-31.
**Milestone:** CC1 — Compiler Correctness & Semantic Consolidation (`MILESTONE_1_TASKS.md`).
**Bloque:** tareas 1–31. **Alcance:** el compilador y su biblioteca. Nada más.

**Cierra 10 de los 13 P1 del registro:** C-001, C-002, C-003, C-004, C-005, F-001, F-002, F-003,
F-004, más F-005 (P2). Quedan abiertos EH-001, EH-002, EH-003 — fase 3, siguiente bloque.

**Verificación local** (`g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow`):

```
ProjectIntegerEvaluator.cpp   ProjectTypeConversion.cpp   ProjectMethodBodyModel.cpp
ProjectOopHirModel.cpp        ProjectOopMirModel.cpp      Parser.cpp
Cc1CorrectnessTests.cpp       Cc1IntegerEvaluatorTests.cpp  Cc1BuildIdentityTests.cpp
ProjectTypeConversionTests.cpp   Stage57LanguageTests.cpp
                                                          -> todas limpias

nsc.cc1.integer-evaluator     EJECUTADO aqui: PASS, 37 checks
nsc.version.identity          EJECUTADO aqui: PASS, 7 checks
ProjectTypeConversionTests    EJECUTADO aqui: PASS, 22 checks
bloque CMake de build id      CONFIGURADO aqui: genera b614c2ddd84c54d5
```

**Corrida en Windows, 2026-08-31 12:06:**

```
checklist   45 / 45      Failed 0
ctest      240 / 242     dos fallos, los dos de EXPECTATIVA DE TEST (ver §8)
triage      16 / 16
cc1_c1_constants          exit 42 a -O0 y -O2
cc1_c2_ternary_ownership  exit 42 a -O0 y -O2
cc1_c3_dowhile_flow       exit 42 a -O0 y -O2
nsc --version             nsc 0.1.0-dev55 (NativeSharp 0.1, stage 57) / build fdafdc549658fab8
```

Las dos fixtures que señalé como más probables de tronar —`c2_ternary_ownership` y
`c3_dowhile_flow`, las que cambian qué acepta el compilador— pasaron a la primera. Nada del
backend LLVM se tocó en este bloque, que es por qué se cortó aquí.

---

## 1. El defecto que más importa, y su repro

Tres líneas de NativeSharp bastaban para entregarle a LLVM comportamiento indefinido:

```csharp
char c = ...;
switch (c) { case 65: ... case 321: ... }
```

`constantFits` agrupaba `builtin:char` con `builtin:ushort` y aceptaba hasta 65535, mientras
`llvmType` mapea `char` a **i8**. El frontend veía 65 y 321 como etiquetas distintas; el backend
emitía `i8 65` para las dos. Un `switch` de LLVM con un valor de caso repetido es UB — y el chequeo
de duplicados que escribí en la ronda anterior existe **exactamente** para impedir eso. Tenía un
agujero debajo.

Son en realidad **dos** defectos que se componen:

- **C-005**, el ancho de `char`: el camino por el que era alcanzable.
- **C-001**, comparar antes de normalizar: el defecto estructural. Aunque `char` midiera 16 bits,
  cualquier cast que estreche reproduce lo mismo.

Los dos están cerrados, y el repro exacto es hoy un test: `normalize(321, "builtin:char") == 65`.

---

## 2. Fase 0 — identidad del build (tareas 1–3)

**El problema no era de compilación.** Un generador de bindings recibió un compilador nuevo, leyó
las capacidades del **anterior** y las reportó como actuales, porque `nsc --version` decía
`0.1.0-dev55` en los dos. Su solución fue calcular una huella del binario.

- `include/nativesharp/Basic/BuildId.h.in` → CMake lo regenera **en cada configure** con el mtime
  más nuevo del árbol más el timestamp del configure, hasheados a 16 hex.
- `Version.h` lo incluye, así que todo consumidor de `Version.h` lo tiene sin un segundo include.
- `nsc --version` imprime `build <id>` en su propia línea.
- `nsc.version.identity` falla si la sustitución no ocurrió — un `@NSC_BUILD_ID@` sin sustituir da
  string vacío, **no** un error de compilación, y ése es el modo de falla que el test cubre.

**Dos decisiones que vale la pena justificar.** El header generado se llama `BuildId.h` y no
`Version.h`: con el mismo nombre habría dos archivos en la misma ruta lógica y cuál gana dependería
del orden de include paths de cada target. Y el id **no** es una constante que alguien deba
acordarse de subir — eso es precisamente lo que falló.

`kCompilerVersion` sigue en `0.1.0-dev55` a propósito: es la identidad de *release* y NativeSharp
0.1 sigue siendo 0.1. `kCompilerStage` sí pasó a `57`, y la `DESCRIPTION` de CMake dejó de decir
"stage 50 ownership types".

---

## 3. Fase 1 — un solo evaluador entero tipado (tareas 4–19)

### 3.1 La causa común de C-001..C-004

El compilador tenía **dos** evaluadores de constantes enteras: el de Stage 37, completo y tipado,
encerrado dentro de `isCompileTimeConstantExpression` que sólo devuelve `bool`; y el que escribí
para las etiquetas de `switch`, que operaba todo sobre `std::int64_t`.

Los cuatro defectos salen de la misma frase: **un evaluador que no conoce el tipo del valor que
evalúa no puede responder correctamente** a división sin signo, a shift aritmético contra lógico, ni
a un cast que estrecha.

### 3.2 `ProjectIntegerEvaluator`

Archivos nuevos: `include/nativesharp/Frontend/ProjectIntegerEvaluator.h` y
`src/Frontend/ProjectIntegerEvaluator.cpp`.

**El invariante:** una constante entera es SIEMPRE el par `(bits, typeKey)`. Nunca un `int64_t`
suelto. Toda operación normaliza su resultado antes de devolverlo, así que los bits que salen del
evaluador son **exactamente** los que el backend va a materializar.

`normalize(bits, typeKey)` es la operación central: trunca al ancho del tipo y extiende el signo si
lo tiene. Es lo que convierte "el frontend cree que vale 321" en "el backend va a materializar 65".

| Defecto | Cómo queda |
|---|---|
| C-001 | El caso `CastExpression` re-normaliza al tipo destino, resuelto por el llamador (`canonicalTypeKey`), no por un tercer resolvedor de tipos. |
| C-002 | `/`, `%` y `>>` ramifican por signedness del tipo. `>>` lógico trunca al ancho real **antes** de desplazar. |
| C-003 | `/` y `%` devuelven `nullopt` si el divisor es 0 **o** si es el par mínimo-con-signo `/ -1`. Un texto de entrada no puede llevar al compilador a UB de C++. |
| C-004 | El shift toma el tipo del **operando izquierdo** y valida el contador contra el ancho de ese tipo, no contra los 64 bits de la representación interna. |

**Acoplamiento:** el evaluador no conoce `ProjectMethodBodyModel`. Recibe un
`ProjectConstantMemberResolver` con tres métodos — resolver un miembro, describirlo, y resolver una
clave de tipo. `ProjectMethodBodyModel::ConstantMemberResolver` lo implementa. Así el mismo
evaluador sirve a etiquetas de `switch`, a inicializadores de campo y a lo que venga.

**Lo que sigue siendo un rodeo:** dentro del inicializador de un campo `const` puede no haber
binding que leer, porque `build()` analiza los cuerpos de método **antes** que los inicializadores
de campo. Se conserva la resolución contra el tipo declarante del campo. La solución de fondo —
reordenar esos dos bucles — sigue anotada como deuda.

### 3.3 La normalización va antes de comparar

En `analyzeSwitch`, cada etiqueta plegada se normaliza al tipo gobernante **antes** de entrar en
`seenValues` y de compararse. Y el valor que viaja al binding — y de ahí a HIR, MIR y el backend —
es el ya canónico: si el binding llevara el valor sin normalizar, el chequeo de duplicados estaría
protegiendo un número distinto del que acaba en la instrucción.

### 3.4 Qué se mide

- `nsc.cc1.integer-evaluator` — **37 checks, ejecutados aquí**: anchos y signo de los diez tipos
  integrales, `normalize` en los casos frontera (321→char, 300→byte, 0xFFFF→short con signo,
  0x80000000→int.MinValue), overflow envolvente, división y módulo sin signo con bit 63, división
  por cero y mínimo/-1 rechazados, shift lógico contra aritmético, shift fuera de rango, tipos por
  sufijo, y formas no constantes devolviendo `nullopt`.
- `ProjectTypeConversionTests` — **22 checks**: 255 cabe en `char`, 256 y 321 no, 65535 sigue
  cabiendo en `ushort`.
- Fixtures: `cc1-char-range`, `cc1-switch-dup-narrowed`, `cc1-switch-dup-cast` (negativas) y
  `tests/cc1/c1_constants/` (ejecución, 42 a `-O0` y `-O2`, con gobernantes `int`, `byte` y `char`).

---

## 4. Fase 2 — el análisis de flujo representa el CFG real (tareas 20–31)

### 4.1 El patrón, en un solo sitio

El compilador **ya tenía** la infraestructura correcta —`captureOwnershipFlow`,
`restoreOwnershipFlow`, `joinOwnershipFlow`— y `case IfStatement` la usaba bien. Pero cada
construcción con ramas tenía que acordarse de repetir el patrón, y ni `?:` ni `&&` ni `||` lo
hicieron.

`analyzeBranchState(entry, work)` y `commitBranchJoin(left, right)` convierten ese patrón en algo
que **se llama** en vez de algo que **se recuerda**.

### 4.2 Por qué el análisis secuencial es inseguro y no sólo impreciso

Definite assignment es un conjunto **MUST** y su join es una **intersección**. Analizar dos ramas en
secuencia equivale a **unir** — la dirección insegura: el compilador da por asignada una variable
que sólo se asigna en una rama.

Y no explotaba, que es lo que lo hacía difícil de ver: C7 pone a cero el almacenamiento de los
locales, así que `F(false)` devolvía `0`. **El backend tapaba el bug del análisis.**

- **F-001** `?:` — las dos ramas se analizan cada una desde el estado tras la condición.
- **F-003/F-004** `&&` y `||` — el operando derecho se analiza como rama, y el estado posterior une
  "RHS evaluado" con "RHS no evaluado". Es el mismo defecto y se arregla con la **misma pieza**, no
  con dos parches locales.

### 4.3 Ownership del ternario (F-002)

Al combinar las ramas sólo se copiaban `typeKey` y `valid`. Consecuencia visible: esta línea **no
compilaba**, y no había forma de escribirla dentro de una expresión:

```csharp
raii string chosen = flag ? (prefix + "A") : (prefix + "B");
```

Las dos ramas son concatenaciones, que sí transfieren propiedad; el ternario entregaba un resultado
sin ownership, y la comprobación de inicialización de un `raii` lo rechazaba.

**La regla:** el resultado conserva una propiedad de ownership sólo si **ambas** ramas coinciden en
ella. Si difieren, el valor no tiene una procedencia única que un checker de lifetime pueda seguir,
y afirmar que sí la tiene sería peor que no afirmar nada. El origen concreto (local o parámetro) se
propaga sólo cuando las dos ramas nombran exactamente el mismo.

### 4.4 `do…while` (F-005)

Dos diferencias reales con `while`, ninguna de las cuales existía:

1. La condición se evalúa **después** del cuerpo, sobre el estado del back-edge — así puede leer una
   variable asignada dentro del cuerpo.
2. **No hay camino de cero iteraciones**, así que `loopEntry` no entra en el join posterior. Al ser
   una intersección, incluirlo borraba toda asignación hecha en el cuerpo.

**Desviación deliberada de la tarea 28.** La tarea decía "separar en dos ramas independientes". No
lo hice: mantuve un `case` con un flag `isDoWhile`. Lo único que distingue a los dos bucles son esas
dos cosas; todo lo demás —el `LoopContext`, la unión de estados de `continue`, y sobre todo las
sesenta líneas de comprobación de RAII a través del back-edge— es idéntico. Duplicarlo daría dos
copias de la parte difícil que habría que mantener en paso para siempre, que es el problema que este
milestone existe para **reducir**. La conducta exigida por §12 del documento de origen está completa.

**Detalle que casi se me pasa:** si el cuerpo de un `do…while` nunca cae ni continúa (todas sus
salidas retornan), la condición es código inalcanzable — pero el HIR la va a recorrer igual y
necesita su binding de tipo. Se analiza en una sonda alcanzable desechable, el mismo patrón que ya
usa `analyzeBlock` para código muerto.

### 4.5 Qué se mide

Negativas: `cc1-ternary-definite-assignment`, `cc1-andand-definite-assignment`,
`cc1-oror-definite-assignment`, `cc1-ternary-ownership-mixed`, y **`cc1-while-flow-still-strict`** —
el gemelo que fija que `while` **sigue** rechazando la misma forma, porque un `while` sí tiene camino
de cero iteraciones. Sin ese gemelo, "arreglar" `do…while` podría haber relajado `while` sin que
nadie lo notara.

Positivas: `tests/cc1/c2_ternary_ownership/` y `tests/cc1/c3_dowhile_flow/`, más
`cc1-ternary-ownership-both` que fija la forma que antes no compilaba.

---

## 5. Por qué el bloque corta aquí

Las fases 0–2 son **frontend y tests, cero backend LLVM**. Eso permitió compilar cada unidad tocada
con el juego de warnings del proyecto y **ejecutar** tres de las suites nuevas aquí mismo.

La fase 3 (EH, tareas 32–39) toca `ProjectOopLlvmLowerer.cpp`, que no compila en este contenedor
porque el árbol no incluye los headers de LLVM — declara a mano su API en `LlvmCApi.h`. Ahí la
verificación previa es el cruce de símbolos contra ese header, y la de verdad es tu corrida.

## 6. Los dos fallos de la corrida, y lo que enseñan

**Confirmado en verde.** La segunda corrida (2026-08-31 12:18) cerró los tres arreglos de esta
sección: `ctest` **242/242, 100%**, con `nsc.version` (#127) y `nsc.cc1.correctness` (#59) en
`Passed`; checklist **45/45, Failed 0**; triage 16/16. Con eso G1 y G2 quedan cerrados. Lo que
sigue es el registro de qué falló en la corrida anterior y por qué, que es lo que vale la pena
conservar.

Ninguno fue del compilador. Los dos fueron **expectativas de test equivocadas escritas por mí**.

**`nsc.version`** — un test que ya existía asertaba el regex
`nsc 0.1.0-dev55.*NativeSharp 0.1.*stage 55`. La tarea 2 subió la stage a 57, y no busqué quién
más afirmaba `stage 55`. La salida real era exactamente la correcta:

```
nsc 0.1.0-dev55 (NativeSharp 0.1, stage 57)
build fdafdc549658fab8
```

Corregido a `stage 57`, y de paso el regex ahora **también exige la línea `build <id>`** con
`[0-9a-f][0-9a-f]+` y no `.*`: un `@NSC_BUILD_ID@` sin sustituir deja la línea vacía, y un `.*` la
aceptaría igual.

**`nsc.cc1.correctness`** — 31 de 32 checks. El que falló fue `cc1-ternary-ownership-mixed`: el
programa **sí se rechazó**, que es el comportamiento que quería, pero con el mensaje
`RAII ownership cannot be copied or borrowed` en vez del `raii local` que yo esperaba.

Ése es precisamente el motivo por el que la mitad negativa comprueba **el mensaje** y no sólo el
fallo. Un test que sólo mirara "falló" habría pasado en verde sin revelar que yo no sabía qué
diagnóstico estaba disparando.

**El sweep que debí hacer antes.** Cambié una constante global —`kCompilerStage`— y sólo revisé los
sitios que toqué. El chequeo correcto era buscar en todo el árbol quién afirmaba el valor viejo. Al
correrlo después apareció una tercera cosa que nadie asertaba pero que era igual de falsa:
`nsc --help` se describía a sí mismo como compilador de **Stage 55**, sin mención de Stage 56 ni 57.
Añadidas las dos descripciones, con ID propio (tarea 76) y no como arreglo "de paso" — que es lo que
la regla de alcance del documento de origen prohíbe explícitamente.

Es la tercera vez en dos días que el mismo patrón me muerde: **verifiqué lo que construí contra un
modelo que también construí yo.** El stub de LLVM tenía los símbolos porque yo lo escribí; el doc 69
daba `sizeof` por presente porque miré la mitad delantera; y aquí la expectativa del test la escribí
sin leer el diagnóstico real.

---

## 7. Deuda que este bloque no cierra

- **Orden de `ProjectMethodBodyModel::build()`**: cuerpos de método antes que inicializadores de
  campo. Sigue obligando al rodeo del tipo declarante en el evaluador.
- **El evaluador de Stage 37** sigue encerrado en `isCompileTimeConstantExpression`. Ahora hay dos
  evaluadores tipados en vez de uno tipado y uno no; unificarlos es su propia tarea.
- EH-001, EH-002, EH-003 — fase 3.
- Todo lo de las fases 4 y 5 (`sizeof`, `stackalloc`, raw `new`, aritmética de punteros,
  reproducibilidad).

## 8. Archivos tocados

Ver `STAGE_CC1_CHANGED_FILES.txt`.
