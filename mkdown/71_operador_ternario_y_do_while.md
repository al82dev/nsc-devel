# 71 — Operador condicional `?:` y `do…while`

**Fecha:** 2026-08-31.
**Ronda:** primera entrada de §2.1 de `70_roadmap_del_compilador.md` (huecos de lenguaje).
**Alcance:** el compilador y su biblioteca. Nada más.

**Estado verificado en Windows real** (`run_windows_checklist.bat`, modo completo, 08:00→08:07):

```
ctest                    239 / 239
checklist de ejecucion    37 / 37     (Passed 37, Failed 0)
triage de bugs            16 / 16     (cero vivos)
l1_conditional            exit 42 a -O0 y a -O2
l2_dowhile                exit 42 a -O0 y a -O2
```

---

## 1. Por qué estos dos primero

El doc 69 midió los huecos de lenguaje contra `TokenKind.h` y encontró que **`?` no existía como
token**. No es que el operador ternario estuviera sin implementar: el lexer no sabía leer el
carácter. Lo mismo con `do`: no estaba en la tabla de keywords.

Eso los pone en una categoría distinta al resto de la lista. Un hueco como `params` o
`Nullable<T>` es trabajo de diseño; éstos dos son huecos de *cobertura*, características que
cualquiera espera encontrar y cuya ausencia deforma el código que se escribe hoy:

- Sin `?:` no se puede elegir un valor **dentro de una expresión**. El sustituto es un `if/else`
  más un local, que no sirve dentro de un argumento, de un inicializador ni de un `return`. Es la
  diferencia entre `Draw(hovered ? hot : cold)` y tres líneas con una variable temporal.
- Sin `do…while` no se puede expresar "corre el cuerpo y **después** pregunta". El sustituto es
  duplicar el cuerpo o meter un flag booleano; las dos formas son peores y las dos se equivocan
  distinto.

Son además los dos más baratos de la lista, y comparten casi toda la tubería, así que se hicieron
en la misma ronda.

---

## 2. `do…while`

### 2.1 El contrato

```
do { <statements> } while ( <expression> ) ;
```

El cuerpo es un bloque con llaves, no un statement suelto. Es una restricción deliberada del
subconjunto —la misma que ya aplican `if`, `while` y `for` en este árbol— y el parser la exige con
un error propio (`'{' after 'do'`) en vez de dejar que falle tres tokens después.

Tres propiedades, y las tres están medidas en el fixture:

1. **El cuerpo corre antes de la primera prueba.** Un `do { } while (false)` ejecuta una vez.
2. **`continue` salta a la CONDICIÓN**, no al principio del cuerpo. Es lo que especifican C y C#, y
   equivocarse aquí convierte un bucle que termina en uno que no.
3. **`break` sale sin volver a evaluar la condición.**

### 2.2 Qué se tocó, capa por capa

**Lexer.** `KwDo` en `TokenKind.h` y en la tabla de nombres de `TokenKind.cpp`; la entrada
`{"do", TokenKind::KwDo}` en el mapa de keywords del lexer. Sin esto, `do` se leía como un
identificador y el error salía tres tokens después, en el `{`.

**AST.** `DoWhileStatement` en `AstNodeKind`. Nodo con la **misma forma que `WhileStatement`**:
`first` = condición, `second` = cuerpo. Que el orden de los hijos no siga al orden textual es
intencional: lo que distingue a los dos bucles no es qué contienen sino en qué orden se ejecuta, y
eso no existe hasta el lowerer. Compartir la forma deja que las tres capas intermedias compartan
también el código, y concentra toda la diferencia en un solo sitio.

**Parser.** `parseDoWhileStatement()`. Consume `do`, parsea el bloque del cuerpo, exige `while`,
`(`, expresión, `)` y **el `;` final** — que es obligatorio en C# y en C, y es el único punto donde
la gramática de `do…while` puede sorprender a alguien que la escribe de memoria. Se enganchó en
`parseStatement` con `if (at(TokenKind::KwDo)) return parseDoWhileStatement();`.

**Body model** (`ProjectMethodBodyModel.cpp`, análisis semántico). El `case
DoWhileStatement` **comparte el cuerpo del `case WhileStatement`**. No es pereza: para el análisis
semántico los dos bucles son idénticos —condición de tipo `bool`, un cuerpo, un `LoopContext`
empujado para que `break` y `continue` encuentren destino—, y la única diferencia, el orden de
ejecución, no existe todavía en esta capa. Duplicar el bloque habría creado dos sitios que
mantener.

**HIR / MIR.** Un `ProjectOopHirNodeKind::DoWhile` y un `ProjectOopMirNodeKind::DoWhile`, con su
nombre para los volcados (`--dump-*`) y su línea de mapeo HIR→MIR. El `case` del HIR también está
compartido con `While` por la misma razón. Aquí tampoco hay lógica: el nodo viaja hasta el lowerer,
que es la primera capa a la que sí le importa la diferencia.

**LLVM** (`lowerDoWhile`). Aquí está la única lógica real, y son tres bloques:

```
  br label %dowhile.body       <- entrada incondicional: ESTA es la propiedad 1
dowhile.body:
  <cuerpo>
  br label %dowhile.cond
dowhile.cond:
  %c = <condicion>
  br i1 %c, label %dowhile.body, label %dowhile.end
dowhile.end:
```

Compárese con `lowerWhile`, que emite `br label %while.cond` como entrada: **esa única línea es
toda la diferencia entre los dos bucles.**

El bloque de continuación que se empuja a `loopStack_` es **`dowhile.cond`**, no `dowhile.body`
(`loopStack_.push_back({endBlock, condBlock, funcletStack_.size()})`). Esa línea es la propiedad 2
entera. Si se empujara `dowhile.body`, un `continue` saltaría el incremento y el bucle no avanzaría
— y compilaría perfecto.

---

## 3. Operador condicional `?:`

### 3.1 El contrato

```
<condicion> ? <cuando-verdadero> : <cuando-falso>
```

- La condición es `bool`.
- **Sólo se evalúa la rama tomada.**
- **Asocia a la derecha**: `a ? b : c ? d : e` agrupa como `a ? b : (c ? d : e)`.
- Está por debajo de `||` y por encima de la asignación en precedencia, así que
  `x = c ? a : b` asigna el condicional completo y `a || b ? x : y` condiciona sobre `a || b`.
- Si las dos ramas tienen el mismo tipo, ése es el resultado. Si no, y las dos son numéricas, el
  resultado es su tipo común. Si no hay tipo común, es un diagnóstico que **nombra los dos tipos**.

### 3.2 Qué se tocó, capa por capa

**Lexer.** El token `Question` y `case '?': return makeToken(TokenKind::Question, begin);`. Antes
de esto, un `?` en el fuente producía `unexpected character in source`.

**AST.** `ConditionalExpression`, con tres hijos: `first` condición, `second` rama verdadera,
`third` rama falsa.

**Parser.** `parseConditionalExpression()`, insertado **entre** `parseAssignmentExpression` y
`parseLogicalOrExpression`, que es exactamente donde va en la tabla de precedencia:

```
parseAssignmentExpression  ->  parseConditionalExpression  ->  parseLogicalOrExpression -> ...
```

`parseAssignmentExpression` ahora llama a `parseConditionalExpression()` en lugar de bajar directo
a `parseLogicalOrExpression()`. La asociatividad a la derecha sale gratis de la recursión: la rama
falsa se parsea con `parseConditionalExpression()` otra vez, así que un `?` que aparezca ahí se
absorbe en el nodo interior.

**Body model.** Un `case ConditionalExpression` nuevo:

1. La condición se analiza y se exige `bool` — mismo camino que usa `if`.
2. Se analizan las dos ramas.
3. Se calcula el tipo del resultado: si son iguales, ése; si no, `commonNumericType`; si no hay,
   diagnóstico con los dos nombres de tipo, que es la parte que hace el error legible.

**HIR.** `ProjectOopHirNodeKind::Conditional` y su lowering. **Aquí estuvo el único defecto real de
la ronda** y merece contarse completo, porque es el patrón que produce bugs silenciosos:

el nodo se creó con el tipo del resultado, pero **las dos ramas se dejaban con su tipo original**.
Para `int ? : long` eso genera un `phi` cuyos dos operandos son `i32` y `i64` mientras el `phi`
mismo dice `i64`, y el verificador de LLVM lo rechaza:

```
PHI node operands are not the same type as the result!
  %cond51 = phi i64 [ %local.load49, %cond.true.58 ], [ %local.load50, %cond.false.59 ]
```

Esa vez salió ruidoso —fallo del verificador, `ctest` en rojo— porque los enteros de distinto ancho
no son compatibles en LLVM. Pero la causa es "olvidé insertar la conversión implícita", y esa misma
causa con dos tipos del mismo tamaño (`int` y `uint`, por ejemplo) **no habría fallado**: habría
compilado, pasado el verificador y producido el valor equivocado sólo en los casos con signo. El
arreglo es un lambda que compara cada rama contra el tipo del resultado y, si difieren, clasifica
la conversión implícita y envuelve la rama:

```cpp
const auto convertBranch = [&](std::unique_ptr<ProjectOopHirNode>& branch, const ast::AstNode* syntax)
{
    if (branch->typeKey == resultType) return;
    const MethodExpressionTypeBinding* binding = expressionType(syntax);
    const ProjectImplicitConversion conversion = ProjectTypeConversion::classifyImplicit(
        branch->typeKey, resultType, binding != nullptr ? binding->integerConstant : std::nullopt);
    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
        branch = wrapImplicitConversion(std::move(branch), conversion, syntax);
};
convertBranch(whenTrue,  expression->second());
convertBranch(whenFalse, expression->third());
```

**MIR.** `ProjectOopMirNodeKind::Conditional`, nombre y mapeo. Sin lógica, igual que `DoWhile`.

**LLVM** (`lowerConditional`). Cuatro bloques y un `phi`:

```
  br i1 %c, label %cond.true, label %cond.false
cond.true:
  %a = <rama verdadera>       <- SOLO se ejecuta si %c
  br label %cond.end
cond.false:
  %b = <rama falsa>           <- SOLO se ejecuta si !%c
  br label %cond.end
cond.end:
  %r = phi [ %a, %cond.true ], [ %b, %cond.false ]
```

**Por qué no `LLVMBuildSelect`.** Un `select` es una sola instrucción, se ve más limpio, y es
**incorrecto**: `select` evalúa *los dos* operandos y después elige. Con eso,
`p != null ? p.Value : 0` desreferencia el nulo. Y no falla ruidosamente: compila, pasa el
verificador y pasa cualquier prueba cuyo condicional no tenga efectos. Los bloques con `phi` son la
forma que preserva la pereza, y por eso el fixture la mide con un contador en vez de asumirla.

El `phi` toma como bloque de entrada **el bloque en el que terminó cada rama**
(`LLVMGetInsertBlock(builder_)` después de bajar la rama), no `cond.true` / `cond.false` literales:
si una rama contiene a su vez un condicional o un bucle, el bloque activo al final ya no es aquél
con el que empezó, y usar la etiqueta original produce IR mal formado. Es el mismo cuidado que ya
tenía `lowerShortCircuit` para `&&` y `||`, y el fixture lo ejercita con el condicional anidado.

Además, un condicional **no es addressable**: pedir su dirección produce el diagnóstico
`a conditional expression produces a value, not addressable storage` en vez de un fallo aguas
abajo.

---

## 4. Cómo se verifica

Dos fixtures nuevas en `tests/language/`, cada una un proyecto completo que **devuelve 42 cuando el
compilador está sano** y un código distinto por cada propiedad que falle, para que un rojo diga
*qué* se rompió sin abrir un depurador:

- **`l1_conditional`** — forma básica en las dos direcciones · **pereza medida con un contador
  estático** (`trueCalls` / `falseCalls`) · condición no constante, para que nada se pliegue ·
  asociatividad a la derecha en una cadena de tres · anidamiento en la rama verdadera · ramas de
  tipos numéricos distintos (`int` / `long`) · composición dentro de un argumento, dentro de un
  `return` y dentro de un inicializador de local.
- **`l2_dowhile`** — cuerpo antes de la primera prueba (`while (false)` y aun así corre) · bucle
  contado normal · `continue` que llega a la condición y por lo tanto **termina** · `break` sin
  reevaluar · anidamiento con `break`/`continue` ligados al bucle interior · un `do…while` dentro de
  un `while` · y un caso combinado con `?:` dentro del cuerpo, porque los bugs interesantes viven
  donde se tocan dos características nuevas.

Y dos enganches, para que no dependa de acordarse:

- **`ctest`**: suite `nsc.stage57.language` (test 239), que compila las dos a `-O0` y `-O2` desde
  `NSC_SOURCE_DIR`.
- **`run_windows_checklist.bat`**: sección `--- language contract fixtures ---`, que además las
  **ejecuta** y compara el código de salida. Ésa es la mitad que mide la pereza; compilar no la
  mide.

---

## 5. Lo que esta ronda dejó abierto

- **`switch` / `case`** sigue siendo el hueco de lenguaje más caro que queda: un `WndProc` es hoy
  una cadena de cuarenta `else if` sin jump table. Es lo siguiente.
- **`??` y `?.`** no se hicieron. Ahora que el token `?` existe, el lexer ya no es el obstáculo,
  pero `??` necesita `Nullable<T>` o una decisión explícita sobre qué significa con referencias, y
  eso es diseño, no cobertura.
- **`goto`** no se hizo y no está claro que deba hacerse.
- La deuda de §3 del doc 70 (propiedades sólo-`set`, archivo y línea en los errores internos de
  Stage 34) sigue igual.

---

## 6. Archivos tocados

Ver `STAGE57_LANGUAGE_CHANGED_FILES.txt`.
