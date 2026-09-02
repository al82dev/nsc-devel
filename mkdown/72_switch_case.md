# 72 — `switch` / `case`

**Fecha:** 2026-08-31.
**Ronda:** segunda entrada de §2.1 de `70_roadmap_del_compilador.md`. Cierra el hueco de lenguaje
más caro que quedaba.
**Alcance:** el compilador y su biblioteca. Nada más.

**Estado: VERDE en Windows real** (`run_windows_checklist.bat`, modo completo, 09:05→09:12):

```
ctest                    239 / 239     incluye nsc.stage57.language (fixture + 6 negativos)
checklist de ejecucion    39 / 39      (Passed 39, Failed 0)
triage de bugs            16 / 16      (cero vivos)
l3_switch                 exit 42 a -O0 y a -O2
```

---

## 1. Por qué éste, y por qué no es azúcar

El sustituto de un `switch` es una cadena de `else if`. La diferencia no es de estilo:

- Una cadena de `else if` es **n comparaciones** en el peor caso, siempre, sin importar cómo se
  distribuyan los valores.
- Un `switch` baja a **una sola instrucción `switch` de LLVM**, que se convierte en **jump table**
  cuando los valores están suficientemente densos y en árbol de decisión balanceado cuando no.

Un `WndProc` de Win32 es cuarenta ramas sobre un id de mensaje. Ésa es la forma para la que existe
el constructo, y es exactamente el caso donde la cadena de `else if` cuesta más.

Segundo motivo, menos medible pero real: un `switch` **dice** que las ramas son mutuamente
excluyentes sobre un mismo valor. Una cadena de `else if` no lo dice, y un lector tiene que
verificar cuarenta condiciones para saber si alguna se solapa.

---

## 2. La decisión de diseño: **no hay fall-through**

**Una sección que llega a su final sale del switch.** No cae en la siguiente.

Ésta es la regla de C#, no la de C, y es la única parte de este documento que un lector que viene
de C va a suponer al revés. El razonamiento:

- El fall-through de C tiene **un** uso legítimo —agrupar varias etiquetas en un mismo cuerpo— y
  **un** modo de falla clásico: olvidar el `break` y ejecutar la sección siguiente sin querer.
- La agrupación se expresa poniendo **varias etiquetas sobre UNA sección**:
  `case 1: case 2: case 3: hacerAlgo();`. Eso da el 100% del beneficio y 0% del modo de falla.
- Lo que queda del fall-through —encadenar cuerpos distintos— es exactamente la parte que produce
  el bug.

C# resuelve lo mismo **rechazando** una sección que no termine en `break`/`return`/`goto case`.
NativeSharp no lo rechaza: **la sección simplemente termina**. Es menos ceremonia para el mismo
resultado, y no hay ambigüedad que resolver porque no existe la otra interpretación.

**`break` sigue siendo legal y significa lo de siempre**: salir del switch ahora. Escribirlo no
cambia nada, no escribirlo tampoco. Es explícito, no portante.

### 2.1 El resto del contrato

| Regla | Nota |
|---|---|
| Tipo gobernante **integral o enum** | No `string`: un switch sobre strings es una tabla hash y una cadena de comparaciones, no una jump table, y el azúcar escondería ese costo. No `float`: la igualdad sobre flotantes es una trampa. |
| Etiquetas **constantes en compilación** | Literales enteros y de carácter, operadores sobre ellos, casts, miembros de enum y campos `const`. |
| **Etiquetas duplicadas: error** | No es una regla de estilo. La segunda etiqueta es código muerto que parece vivo, y el `switch` de LLVM tiene **comportamiento indefinido** con un valor de caso repetido. |
| `default` opcional, **como máximo uno**, en **cualquier posición** | No tiene que ir al final. |
| Sin `default` y sin coincidencia | Se salta el statement completo. |
| Sección con etiquetas y **sin statements** | Legal: "coincide y no hace nada". Nunca "cae a la siguiente". |
| `break` dentro de un switch dentro de un bucle | Sale **del switch**. |
| `continue` dentro de un switch dentro de un bucle | Continúa **el bucle**. |

Las dos últimas son el par que una implementación ingenua invierte, y **equivocarlas no falla en
compilación**: es un salto a la etiqueta equivocada. Están medidas en la fixture.

---

## 3. Qué se tocó, capa por capa

### 3.1 Lexer

`KwSwitch` y `KwCase` en `TokenKind.h`, en la tabla de nombres y en la tabla de keywords del lexer.

**`default` no se agregó**: ya existía como keyword (es también la palabra del valor por omisión de
un tipo, `default(T)`). La gramática del switch la reutiliza en vez de inventar una segunda
ortografía de la misma palabra. Eso obliga a distinguir los dos usos, y de ahí sale `atSwitchLabel()`
en §3.3.

### 3.2 AST

Tres nodos:

```
SwitchStatement   first()   = expresión gobernante
                  children()= secciones, en orden de fuente
SwitchSection     children()= las etiquetas primero, después los statements
                  integerValue() = cuántas de esas children son etiquetas
SwitchLabel       first()   = la expresión constante, o nullptr si es `default`
                  flag()    = true si es `default`
```

**Por qué el contador de etiquetas y no distinguir por `kind()`**: guardar el punto de corte
explícito significa que ningún consumidor tiene que decidir "¿esto es una etiqueta o un statement?"
inspeccionando el tipo de nodo. Ese tipo de decisión deja de funcionar en silencio el día que
alguien agrega un tipo de statement nuevo.

### 3.3 Parser

`parseSwitchStatement()` y `atSwitchLabel()`.

`atSwitchLabel()` es el punto delicado: `case` siempre abre una etiqueta, pero **`default` sólo la
abre si el token siguiente es `:`**. Sin esa condición, una sección que empezara con `default(int)`
se leería como una etiqueta y el parser se perdería.

El bucle interior de statements de una sección corre hasta la siguiente etiqueta o hasta el `}`. Un
token que no abre etiqueta al principio de una sección produce un diagnóstico **y se consume**, para
que el parser avance en vez de girar en el sitio con entrada malformada.

La expresión de un `case` se parsea como una **expresión completa**. Que sea constante, que convierta
al tipo gobernante y que no duplique otra etiqueta son preguntas semánticas: se contestan donde vive
el evaluador de constantes y la tabla de tipos, no aquí, donde habría que adivinarlas por sintaxis.

### 3.4 Análisis semántico — `analyzeSwitch`

Aquí se deciden las cuatro cosas del contrato y aquí **una etiqueta deja de ser una expresión**:

1. Tipo gobernante integral o enum, con diagnóstico que nombra el tipo que sí tiene.
2. Cada etiqueta se analiza, se comprueba la conversión implícita al tipo gobernante, y **se pliega
   a un entero**.
3. Duplicados rechazados, comparando **valores**, no sintaxis: `case Messages.Destroy:` y `case 2:`
   son la misma etiqueta si `Destroy` vale 2, y la fixture negativa lo prueba con esa forma exacta.
4. Un solo `default`.

El resultado se guarda en un `MethodSwitchBinding` (una entrada por switch, con una
`MethodSwitchSectionBinding` por sección). **Todo lo que está aguas abajo lee números**, así que ni
el HIR, ni el MIR, ni el backend necesitan saber qué es una expresión constante.

**Flujo de ownership.** Cada sección se analiza **partiendo del estado en el switch**, nunca del
estado de salida de la sección anterior — que es la consecuencia directa de que no haya
fall-through. Al final se unen: los estados de salida de las secciones que llegan a su final, los
estados de cada `break`, y —**sólo si no hay `default`**— el estado de entrada, porque sin `default`
un valor que no coincide con nada se salta el statement entero.

**El `LoopContext` ahora tiene `isSwitch`.** Un switch empuja un contexto porque `break` tiene que
poder nombrarlo; pero **no es destino de `continue`**, así que `analyzeLoopExit` camina hacia afuera
saltándose los marcos con esa bandera cuando busca el bucle. Sin la bandera, un `continue` dentro de
un `switch` dentro de un `while` sería rechazado, o peor, ligado a un destino que no existe.

`analyzeBlock` gana **una línea**: saltar las children de tipo `SwitchLabel`. Eso deja que una
sección reutilice esa rutina entera —scope anidado, continuaciones de cleanup, sonda de código
inalcanzable— en vez de hacer crecer una segunda copia ligeramente distinta.

### 3.5 El plegador de constantes de las etiquetas

`foldIntegerConstantExpression`. Deliberadamente estrecho, y deliberadamente separado:

- **Qué pliega:** literales enteros y de carácter; `+ - ~` unarios; `+ - * / % & | ^ << >>`
  binarios; casts; **miembros de enum**; y **campos `const`** — que es lo que es un id de mensaje de
  Win32 (`public const int WM_DESTROY = 2;`).
- **Qué no:** flotantes, strings, llamadas, `sizeof`. Contestan "no es constante" y salen con el
  diagnóstico que lo dice, en vez de un valor que nadie puede explicar.
- La aritmética se hace sobre el gemelo sin signo para que el desbordamiento **envuelva** en vez de
  ser indefinido.

**El detalle no obvio, que costó pensarlo:** un `const` que se define en términos de otro `const`
—`public const int MyMsg = WmUser + 1;`, el idioma de Win32— no se puede resolver leyendo la tabla
de bindings de nombres, porque **`ProjectMethodBodyModel::build()` analiza los cuerpos de método
ANTES que los inicializadores de campo**. Cuando una etiqueta en un cuerpo pide plegar `MyMsg`, la
expresión `WmUser + 1` todavía no pasó por `analyzeExpression` y su `WmUser` no tiene binding.

La salida: al expandir el inicializador de un campo `const`, el plegador lleva consigo **el tipo que
declara ese campo**, y un nombre sin calificar que no tenga binding se resuelve contra ese tipo —que
es exactamente la regla bajo la que ese inicializador está escrito. Doce líneas, sin reordenar
`build()` y sin tocar el evaluador de constantes de Stage 37.

### 3.6 HIR y MIR

`Switch` y `SwitchSection` en los dos, con la misma forma:

```
Switch         children[0]   = expresión gobernante
               children[1..] = secciones
SwitchSection  switchLabels     = los valores YA PLEGADOS de sus etiquetas
               switchHasDefault = si lleva el `default`
               children[0]      = un Block con los statements de la sección
```

Las expresiones de las etiquetas **no se bajan nunca**. El cuerpo de la sección se convierte en un
`Block` ordinario para que todo consumidor vea una forma que ya sabe manejar.

Los dos campos nuevos (`switchLabels`, `switchHasDefault`) se agregaron a los nodos HIR y MIR y se
copian tal cual en la conversión: los valores se plegaron **una vez**, en el modelo semántico.

### 3.7 Backend LLVM — `lowerSwitch`

Antes de nada, un hecho de este árbol que hay que saber para tocar el backend: **no se incluyen los
headers de LLVM.** `src/LLVM/LlvmCApi.h` declara a mano el subconjunto de la API C que el compilador
usa, y se linkea contra `LLVM-C.lib`. Esta ronda agregó dos entradas ahí (`LLVMBuildSwitch`,
`LLVMAddCase`). Ver §7.


```
  %g = <gobernante>
  switch i32 %g, label %default-o-switch.end [
    i32 <v1>, label %switch.case.0
    i32 <v2>, label %switch.case.0     <- dos etiquetas, una sección
    i32 <v3>, label %switch.case.1
  ]
switch.case.0:
  <statements>
  br label %switch.end                 <- NO cae en switch.case.1
switch.case.1:
  <statements>
  br label %switch.end
switch.end:
```

Tres cosas que vale la pena señalar:

1. **La ausencia de fall-through se ve en el layout de bloques**: cada bloque de caso termina
   ramificando a `switch.end`, exactamente como si hubiera escrito `break`. Un `break` explícito
   produce la misma rama por el camino ordinario de salida estructurada — que es por qué `break`
   sigue siendo legal y significando lo mismo.
2. **Sin `default`, el destino por omisión del `switch` es `switch.end`**: un valor sin coincidencia
   se salta el statement.
3. **El marco de `loopStack_` que empuja un switch hereda el destino de `continue` del bucle que lo
   encierra.** Ésa es la mitad de backend de la regla `continue`→bucle.

Esa herencia obligó a un cambio pequeño y no obvio en `LoopTargets`: ahora guarda **dos**
profundidades de funclet. El destino de `break` está a la profundidad del constructo que lo posee;
el de `continue` está a la de un bucle que puede quedar varios funclets más afuera. Con una sola
profundidad, un `break` desenrollaría funclets que no debe, o un `continue` se saltaría funclets que
sí debe.

La constante de cada caso se construye con el mismo criterio de signo que usa el lowering de campos
constantes (un enum baja a `i32` y sus miembros son con signo), para que una etiqueta y el valor
contra el que se compara no puedan discrepar sobre el signo.

El guard de "esto es un entero" se expresa **en términos de la clave de tipo**, no probando el tipo
de LLVM, para no tener que traer el enum `LLVMTypeKind` a un header nuestro. Ver §7.

---

## 4. Cómo se verifica

### 4.1 La mitad positiva — `tests/language/l3_switch`

Un proyecto que **devuelve 42 cuando el compilador está sano** y un código distinto por cada
propiedad que falle (34 códigos), para que un rojo diga *qué* se rompió sin abrir un depurador.

Cubre: despacho básico en las dos direcciones · `default` cuando nada coincide · **sin `default` y
sin coincidencia, el statement se salta** · varias etiquetas en una sección · **NO FALL-THROUGH,
medido con un contador** · la misma sección escrita con `break` dando idéntico resultado · `default`
en posición **no final** · sección con etiquetas y sin statements · etiquetas negativas · **etiquetas
de campos `const`, incluida una definida como `Paint + 1`** · tipo gobernante `enum` con etiquetas
`Color.Red` · tipo gobernante `char` con etiquetas `'a'` · gobernante `long` con etiquetas `int` ·
locales declarados dentro de una sección · `return` desde dentro de una sección · anidamiento ·
**`break` dentro de switch dentro de bucle sale del switch** · **`continue` dentro de switch dentro
de bucle continúa el bucle** · composición con `do…while` y `?:`.

### 4.2 La mitad negativa — nueva en `Stage57LanguageTests.cpp`

Seis programas que **no deben compilar**, y de cada uno se verifica **que falló Y por qué**.
Verificar sólo "falló" pasaría también con una errata en la propia fixture, que es como un test
negativo deja de probar algo sin que nadie se entere.

```
governing-type                switch sobre double
duplicate-label              case 2: dos veces
duplicate-label-through-const case 2: y case K.Two: donde K.Two == 2
two-defaults                 dos default en el mismo switch
non-constant-label           case k: donde k es un local
continue-without-loop        continue dentro de un switch sin bucle alrededor
```

Vale tanto como la mitad positiva. Un compilador que acepta dos `case` con el mismo valor no da un
error: da código muerto que parece código vivo, y comportamiento indefinido en LLVM.

### 4.3 Enganches

- **`ctest`**: la fixture positiva y las seis negativas cuelgan de `nsc.stage57.language`, que ya
  estaba registrada. **No hizo falta tocar `CMakeLists.txt`.**
- **`run_windows_checklist.bat`**: una línea nueva en `--- language contract fixtures ---` que
  compila **y ejecuta** `l3_switch` a `-O0` y `-O2` y compara el código de salida. Ésa es la mitad
  que mide el comportamiento; compilar no lo mide.

---

## 5. Lo que este switch NO tiene

Dicho explícitamente para que nadie lo busque:

- **`goto case` / `goto default`.** No existen, y no hacen falta mientras no haya fall-through que
  emular. Si aparece un caso real que los pida, se documenta entonces.
- **`switch` sobre `string`.** Ver §2.1.
- **Patrones** (`case int i when i > 0:`). Fuera de alcance por definición: eso es otro lenguaje.
- **Exhaustividad sobre enums.** Un switch sobre un enum al que le falta un miembro **no** avisa.
  Es un candidato razonable a warning más adelante; hoy no lo es.

---

## 6. Deuda que esta ronda tocó de lado

- Un campo `const` que se define en términos de un `const` **de otra clase** (`case A.X:` donde
  `X = B.Y + 1`) no pliega: el fallback de §3.5 sólo resuelve nombres **sin calificar** contra el
  tipo del propio campo. Da el diagnóstico correcto ("must be a compile-time integer constant"), no
  un valor equivocado. La solución de fondo es reordenar los dos bucles de
  `ProjectMethodBodyModel::build()` —inicializadores de campo antes que cuerpos de método— o
  extraer el evaluador de constantes de Stage 37 de dentro de `isCompileTimeConstantExpression` para
  que sea reutilizable. Lo segundo es mejor y es trabajo de su propia ronda.
- Sigue abierta la deuda de §3 del doc 70: propiedades sólo-`set`, y archivo y línea en los errores
  internos de Stage 34.

---

## 7. El fallo de build, y la lección de método

La primera entrega **no compiló**, con cuatro errores en `lowerSwitch`:

```
error: use of undeclared identifier 'LLVMGetTypeKind'
error: use of undeclared identifier 'LLVMIntegerTypeKind'
error: use of undeclared identifier 'LLVMBuildSwitch'
error: use of undeclared identifier 'LLVMAddCase'
```

**La causa:** este proyecto **no incluye los headers de LLVM**. Declara a mano el subconjunto de la
API C que usa, en `src/LLVM/LlvmCApi.h`, y linkea contra `LLVM-C.lib`. Usar una función de LLVM que
no esté en esa lista no falla al enlazar: falla al compilar, como identificador desconocido.

**El arreglo, en dos partes:**

1. `LLVMBuildSwitch` y `LLVMAddCase` declarados en `LlvmCApi.h`, junto a `LLVMBuildCondBr`, con su
   firma real de `llvm-c/Core.h`.
2. El guard de tipo **reescrito sin `LLVMGetTypeKind`**. Traer el enum `LLVMTypeKind` a mano
   significaría fijar valores numéricos de LLVM en un header nuestro, y el guard es defensivo: el
   frontend ya rechazó todo lo que no sea integral o enum. Reescrito en el vocabulario que el
   archivo ya habla:

```cpp
if (!isEnumTypeKey(governingTypeKey) &&
    !frontend::ProjectTypeConversion::isIntegral(governingTypeKey))
{
    return fail(error, "switch governing value is not integral or an enum: " + governingTypeKey);
}
```

Dice lo mismo, no necesita entrada nueva en `LlvmCApi.h`, y sigue fallando ruidosamente si algún día
`llvmType` deja de mapear esos dos a un entero.

**La lección, que es la parte que importa.** El resto del código se había verificado antes de
mandarlo compilando las siete unidades tocadas con el juego de warnings del proyecto — y salió
limpio, incluida esta. Pero `ProjectOopLlvmLowerer.cpp` no se puede compilar sin LLVM, así que
`lowerSwitch` se verificó **contra un stub escrito a mano** de la API. Un stub escrito por quien
escribe el código tiene, por construcción, exactamente los símbolos que ese código usa: probó la
sintaxis y no probó nada sobre disponibilidad.

El chequeo correcto no era un stub: era **cruzar los símbolos `LLVM*` que el archivo usa contra los
declarados en `LlvmCApi.h`**. Sobre el archivo completo son 141 símbolos, y hoy faltan cero. Es una
comparación de dos listas, cuesta segundos, y habría atrapado esto antes de la corrida.

**Regla para rondas futuras que toquen el backend:** después de tocar
`src/LLVM/ProjectOopLlvmLowerer.cpp`, cruzar sus símbolos `LLVM*` contra `src/LLVM/LlvmCApi.h` antes
de pedir una compilación.

## 8. Archivos tocados

Ver `STAGE57_SWITCH_CHANGED_FILES.txt`.
