# 115 -- La deuda anotada, cerrada

Fecha: 2026-09-03
Ámbito: sólo el compilador (`A:\nsc-devel`).
Cierra: ID 124, el literal mínimo, el diagnóstico de `x / 0`, las dos tablas de casos, la
conversión a cadena C, el consumidor de `lastErrorRange_`, y el séptimo sitio de la cadena del
doc 100.

Todo lo de este documento estaba **anotado y no hecho**, algunas cosas desde el doc 61. Una lista
de deuda que sólo crece deja de ser una lista y pasa a ser una excusa.

---

## 1. ID 124 — cerrado como falso, sin escribir código

El registro pedía *"la fixture profunda del criterio de SHR-003: `catch → switch → continue`"*.

**Ya existe.** `tests/cc1/c8_loop_catch_switch` la cubre entera, y mejor de lo que el ID pedía: mide
las dos salidas por separado —`break` sale del switch y nada más, `continue` sale del switch, del
catch y de la vuelta a la vez— **contando lo que se ejecuta DESPUÉS del switch**, que es donde la
confusión entre los dos destinos se nota. Y lo repite con los destinos **intercambiados**, para que
un cruce dé cuentas distintas en los dos bloques y no la misma en los dos.

`c7_finally_switch` cubre el caso hermano, el switch dentro de un `finally`, que es el que sí
distingue `funcletDepth` de `continueFuncletDepth`.

**No se escribe nada.** Escribir una tercera fixture que mida lo mismo habría sido trabajo que
parece progreso.

---

## 2. `-2147483648` ya se puede escribir

Era el hueco encontrado al escribir `c30` (doc 110 §3.5):

```
cannot initialize local 'plegadoResta' of type 'builtin:int' from 'builtin:long'
```

El menos unario se aplicaba a `2147483648`, que no cabe en un `int` positivo, así que era `long` y
arrastraba a la expresión entera. El rodeo `-2147483647 - 1` estaba en tres sitios de la biblioteca
y las pruebas.

**La regla, que es la de C#:** el menos unario aplicado **directamente** a un literal entero se tipa
junto con él, así que la magnitud admitida sube en uno — exactamente lo que hace falta para el
mínimo de cada ancho, que es uno más que su máximo. `-(2147483648)` con paréntesis no es el caso
especial y sigue siendo `long`; `2147483648` a secas también.

### Y era la tercera copia de una regla

La regla del tipo de un literal estaba escrita **dos veces**, y la segunda llevaba este comentario:

> *"Duplicarlo aquí sería justo el error que este archivo existe para eliminar, así que la regla se
> mantiene deliberadamente idéntica y el test la fija contra casos concretos."*

Reconocía el problema y luego lo cometía. **"Se mantiene idéntica" es una promesa, no un guardián.**
Ahora hay una sola: `include/nativesharp/Frontend/IntegerLiteralTyping.h`, con cuatro
`static_assert` que fijan en el build lo que cambia y lo que no.

### El error que casi se me escapa, y cómo apareció

La primera versión funcionaba: el programa compilaba. Y el IR decía esto:

```llvm
%local427.addr = alloca i32, align 4
store i64 -2147483648, ptr %local427.addr, align 4
```

**Ocho bytes en una ranura de cuatro.** El modelo semántico decía `int` y el backend seguía
diciendo `long`, porque el backend no lee el resultado del análisis: lee `expressionTypes_`, donde
el literal se había anotado por su cuenta antes de que el menos lo tocara. Faltaba volver a
anotarlo.

**No lo cazó ningún rojo.** Lo cazó mirar el IR emitido, que es la diferencia entre "compila" y
"está bien".

### Medido

El corpus entero volcado a IR antes y después. **La diferencia entera son dos líneas**, en `c21`:

```
<   %sext = sext i32 %local.load15 to i64
<   %icmp16 = icmp ne i64 %sext, -2147483648
---
>   %icmp16 = icmp ne i32 %local.load15, -2147483648
```

Una extensión de signo que sobraba. Ni un cambio más en 180 proyectos.

`NumericLimits.Int32Min()` ahora es `return -2147483648;` y emite `ret i32 -2147483648`.

---

## 3. `x / 0` con constantes: ahora es un error de compilación

El doc 113 dejó la división por cero como **trampa determinista** en el IR. Faltaba la otra mitad:
cuando el divisor es una constante cero, el compilador **ya sabe** que ese programa muere siempre —
emitía `br i1 false` hacia el bloque de trampa. Un fallo demostrado, compilado en silencio.

```
division by the constant zero
remainder by the constant zero
```

**Sólo enteros.** En coma flotante `x / 0.0` es infinito y `0.0 / 0.0` es NaN: valores legítimos de
IEEE 754 (doc 110 §2.6), no fallos. **Y sólo cuando el cero es constante**: un divisor que sale de
una variable no se toca, porque ahí el compilador no sabe nada y la guarda en tiempo de ejecución es
la respuesta correcta.

**Límite honesto:** un `const int cero = 0;` usado como divisor **no** se diagnostica todavía. La
constancia de un identificador no llega a este punto del análisis. La trampa en tiempo de ejecución
lo cubre; el diagnóstico queda anotado con su condición de reapertura.

### Y su caso negativo, que ninguna otra fixture podía cubrir

`tests/cc1/c32_division_por_cero`, **salida esperada 129**. La trampa hace
`ExitProcess(128 + razón)` y `DivisionByZero` es la 1. Todas las demás fixtures certifican
devolviendo 42, y un programa que aborta no puede devolver nada — por eso este caso llevaba
anotado desde el doc 110 §4.

Lo que de verdad mide, y no es obvio: **que la trampa esté en el IR**. Antes se dejaba al `idiv`,
que da `0xC0000094` en `-O0` —un código de Windows, no del programa— y nada en `-O2`, porque
`sdiv x, 0` es poison. Un **129 en los dos niveles** prueba que la emite el compilador.

---

## 4. `CString`: hablar con el sistema operativo sin escribirlo a mano

Un `string` de NativeSharp es `{ i64 longitud; bytes }` y **no promete un cero al final**; toda la
API `A` de Win32 lo exige. El primer programa real del proyecto lo escribió a mano, byte a byte
(doc 112 §C.3). Ése es el consumidor medido que esta clase cierra.

**Por qué una clase y no una función.** La cadena C necesita memoria que sobreviva a la llamada, y
esa memoria tiene que morir cuando alguien lo decida. Una función que devolviera un `byte*` recién
reservado obligaría a cada llamador a acordarse de liberarlo — y "acordarse" es exactamente lo que
el modelo de este lenguaje existe para no necesitar. Un `raii CString` se limpia solo.

`demo_ventana` ya no tiene la función a mano: usa la clase, y el `44` escrito a mano en la llamada a
`TextOutA` ahora sale de `textoC.Length()`.

### La comprobación que vale

`tests/cc1/c31_cadena_c` le pasa la cadena a **`lstrlenA` de kernel32**: quien decide si el NUL está
bien puesto es **el sistema operativo**, no el programa contándose sus propios bytes. Contarse uno
mismo y darse por bueno es verificar contra el propio modelo — justo lo que el criterio del proyecto
prohíbe.

Incluye el caso incómodo: un cero **en medio** del texto. La clase no lo rechaza ni lo sustituye, y
la fixture mide las dos mitades del contrato a la vez — `Length()` devuelve 5, `lstrlenA` devuelve 2.

---

## 5. Las dos tablas de casos, unificadas

`run_cases.py` y `nslink_pipeline.py` tenían la misma lista de 50 casos escrita dos veces, y la
segunda avisaba al final del informe:

> *"La tabla CASES de este script está duplicada de run_cases.py a propósito (aislamiento). Si
> alguien añadió una fixture allá, hay que añadirla aquí."*

**Un aviso impreso al final de un informe no es un guardián; es una nota adhesiva.** Y ya había
mordido: el doc 109 cuenta la vez que las dos quedaron en 48 y 44, y la corrida de NSLink salió
verde midiendo cuatro casos menos.

**El motivo real de la duplicación era el aislamiento**, y un módulo de **datos** lo respeta entero:
`cases_table.py` no importa código, no ejecuta nada, no tiene dependencias. Los dos scripts siguen
siendo independientes entre sí; los dos dependen de la misma tabla, que es lo que se quería.

El aviso de conteos **no se quita**: sigue comparando contra los resultados de la **otra** pasada, y
eso detecta algo distinto — una pasada que no ejecutó todo lo que debía.

---

## 6. `fichero:línea:columna` en los errores del HIR

El envoltorio de `lowerExpression` (doc 112 §A.4) apuntaba el rango del nodo más interno que no se
pudo bajar, y **nadie lo consumía**: se guardaba y se tiraba.

Ahora `ProjectCompilation::compileProjectOopHir` lo traduce:

```
loader-error: A:\nsc-devel\lib\NativeSharp.Core\NumericLimits.ns:120:21: <mensaje>
```

**Por qué ahí y no en el modelo.** `ProjectOopHirModel` no tiene `SourceManager` y no le hace falta
para *recordar* un rango: recordar es barato, traducir necesita los ficheros. `ProjectCompilation`
ya tiene los dos. Enhebrar el `SourceManager` hasta el modelo habría sido pasarlo por seis
constructores para hacer allí lo que ya se puede hacer aquí.

**Verificado inyectando un fallo** en el literal flotante del HIR: señaló `NumericLimits.ns:120:21`,
que es exactamente la columna donde empieza `-3.40282347E+38F`. Luego se revirtió la inyección.

---

## 7. El séptimo sitio de la cadena del doc 100

De los diez sitios que hay que tocar para una operación núcleo nueva, tres se unificaron en el
doc 106 y el 112. Quedaba uno con una **caída silenciosa de verdad**:

`ProjectOopLlvmLowerer::lowerExpression`, 146 casos sobre `ProjectOopMirNodeKind` **y un
`default:`**. Con el `default`, añadir un enumerador nuevo y olvidarse de bajarlo no daba ningún
error: caía ahí y el programa fallaba en tiempo de ejecución con un mensaje que decía *"no es una
expresión"* sobre algo que sí lo era. **Un enumerador olvidado se parecía exactamente a uno mal
colocado.**

Los veinte que caían en el `default` son **sentencias**, y ahora están escritos uno a uno
compartiendo el mismo `failValue`. Sin `default`, con `-Wswitch` y `/WX`, olvidarse es un error de
compilación que **nombra el enumerador**:

```
error: enumeration value 'Rethrow' not handled in switch [-Werror,-Wswitch]
```

**Lo que NO se toca, y por qué.** `lowerStatement` también tiene un `default`, pero el suyo es una
regla de verdad —"todo lo que no es una sentencia se baja como expresión"— y encaminarlo hacia
`lowerExpression`, que ahora sí está guardado, deja la cadena protegida de punta a punta. Listar allí
145 casos sería ruido que esconde la regla.

---

## 8. `Thread.Join`, la única medición del contrato de memoria que podía fallar

El doc 111 §2.3 promete que `Join` es un punto **acquire**. Es lo que hace correcto el patrón
*lanzar N hilos, unirlos, leer los resultados*, y es el que el proyecto **usa sin pensarlo**.

`tests/cc1/c33_hilos_join`: ocho hilos, **dieciséis ranuras cada uno**, treinta rondas, con las
ranuras puestas a cero entre rondas y comprobadas. El trabajo es largo a propósito, para que el
`Join` tenga que esperar de verdad: un hilo que termina antes de que el principal llegue al `Join`
no mide nada.

**Y la advertencia del propio doc 111, que hay que repetir:** un modelo de memoria **no se verifica
con fixtures**. Si el contrato es cierto, esto pasa siempre; si es falso, falla **a veces** — y "a
veces" ya es información que hoy no existe.

---

## 9. Lo medido

| | |
|---|---|
| `./sweep.sh llvm` | **OK=178 FAIL=5** (los cinco negativos de siempre; eran 175) |
| corpus IR antes/después del literal mínimo | **dos líneas de diferencia**, las dos correctas |
| `nsc.cc1.integer-evaluator` | 39/39 |
| guardián nuevo del bajador | comprobado borrando un `case`: error que nombra el enumerador |
| `fichero:línea:columna` | comprobado inyectando un fallo, y revertido |
| tablas de casos | una sola, **53 casos → 106 trabajos** |

**No verificado desde el contenedor:** que `c31`, `c32` y `c33` pasen ejecutando. El frontend llega
al IR y para. `c32` en particular sólo significa algo ejecutando: su éxito es **salir con 129**.

---

## 10. Lo que sigue abierto

| Qué | Estado |
|---|---|
| ID 129 (enums por nombre) | abierto |
| ID 132 (sobrecargas genéricas por lista de parámetros) | abierto |
| `x / 0` con un `const` local como divisor | sin diagnóstico; la trampa lo cubre |
| `demo_ventana` con `WNDCLASSEXA`, teclado y ratón | pendiente |
| partir `nsc.cc1.correctness` en tests de ctest | pendiente, **al final por decisión de Alberto**: no es crítico |
| SHR-063, ID 125, ID 87 | decididos provisionalmente en el doc 114 |
