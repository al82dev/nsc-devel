# 106 -- Una sola tabla de operaciones núcleo, y el compilador vigilándola

Fecha: 2026-09-02
Ámbito: solo el compilador (`A:\nsc-devel`).
Fichero tocado: `src/Frontend/ProjectOopHirModel.cpp`.

## QUÉ estaba mal

Añadir una operación núcleo obliga a tocar diez sitios (doc 100). **Dos de ellos estaban en el
mismo fichero, a doscientas líneas uno de otro, y no había nada que los atara**:

1. una cadena de `core->kind == ... ||` que decidía si la operación entraba en el bloque
   compartido de "llamada tipo string" de `lowerExpression`;
2. una cadena de `if`/`else if` que le asignaba la aridad, con un `expected = 2U` **por omisión**.

Olvidar una de las dos no daba ningún error, y cada olvido fallaba distinto — ninguno de los dos
mensajes nombra la operación ni la lista:

- **Sólo en (1):** la aridad caía al 2 por omisión. Si la operación no tomaba dos operandos, el
  resultado era `Stage 51 string intrinsic received an invalid operand count`.
- **Sólo en (2):** la operación nunca entraba al bloque, así que la llamada **se quedaba siendo el
  método NativeSharp normal** — el que devuelve `-1` o `""` como marcador de posición. Compila,
  enlaza, ejecuta, y da una respuesta plausible y **falsa**. Medido al implementar `File.GetSize`:
  el gancho aparecía declarado en el IR y sin una sola llamada.

El segundo es el peligroso, y es la misma forma del fallo que costó la corrida de esta noche: algo
verde que no mide lo que dice medir.

Esto estaba **anotado y no hecho** desde el Bloque 4 fase A. Es la quinta copia de una regla en
esta ronda, después de las 62 entradas de KERNEL32 (doc 102), las tres copias en las pruebas de
host (doc 103), las dos tablas de casos (doc 105) y la búsqueda del enlazador (doc 105).

## CÓMO queda

Una sola función en el espacio de nombres anónimo del fichero:

```cpp
std::optional<std::size_t> stringCallOperandCount(MethodCoreOperationKind kind);
```

Las **dos** preguntas salen de la **misma** respuesta: `has_value()` es "¿entra en este bloque?" y
el valor es la aridad. No pueden discrepar porque no son dos cosas.

Los dos sitios de uso quedan así:

```cpp
const std::optional<std::size_t> coreOperandCount =
    core != nullptr ? stringCallOperandCount(core->kind) : std::nullopt;
if (coreOperandCount.has_value())
...
const std::size_t expected = *coreOperandCount;
```

### Y el `switch` no tiene `default`, que es todo el punto

`MethodCoreOperationKind` tiene **104 enumeradores**. Los 104 están escritos, uno por uno: 50 con
aridad (1 con cero operandos, 30 con uno, 15 con dos, 4 con tres) y 54 devolviendo `std::nullopt`
porque se bajan por otro camino.

Listar los 54 que **no** entran parece redundante y es justo lo contrario: sin ellos el `switch`
necesitaría un `default`, y con un `default` el compilador deja de obligar a nadie a decidir dónde
va la operación número 105.

Con `-Wswitch` —que clang enciende por omisión y el proyecto convierte en error con `/WX` en
Windows y `-Werror` fuera— añadir un enumerador y no tocar esta función es un **error de
compilación que nombra el enumerador que falta**. Esa es la comprobación mecánica que quedó
anotada en el doc 100 y que no existía: ahora la hace el compilador, no la memoria de quien añade
la operación.

### Los comentarios por operación se movieron, no se perdieron

Las dos cadenas viejas llevaban 29 bloques de comentario explicando por qué cada operación cae en
el grupo en el que cae —"`WriteAllText`/`AppendAllText` **no** están aquí a propósito: toman dos
operandos", "`GetCommandLineArgs` entra con un `builtin:string[]` y el bloque nunca exigió que el
operando fuera una cadena"—. Están todos, encima de su `case`, tal cual. Un fichero que se
reescribe y se lleva por delante la historia de por qué es peor que el fichero que había.

## Verificación

Tres cosas, y la primera es la que cuenta.

**1. El IR de todo el corpus es idéntico byte a byte.** Se compiló el árbol *anterior*, se volcó
el IR de los 146 proyectos, se compiló el árbol nuevo y se volcó otra vez:

```
sha256 del corpus entero, compilador ANTERIOR:  80a19aa48609adda...d117126d
sha256 del corpus entero, compilador NUEVO:     80a19aa48609adda...d117126d
diff -rq: 0 ficheros distintos
```

Esto importa por la regla del proyecto: *ninguna verificación cuenta si el modelo contra el que
verifico lo construí yo*. Aquí no se compara contra un modelo mío de lo que las cadenas hacían —
se compara contra **el compilador que las tenía**. La tabla se derivó además leyendo las dos
cadenas con un script, no a mano.

**2. La prueba negativa: el guardián funciona.** Se borró un `case` y se compiló:

```
error: enumeration value 'FileGetSize' not handled in switch [-Werror,-Wswitch]
```

y con `ProcessRun`, lo mismo. Es un error, no un aviso, y dice el nombre.

**3. El barrido del frontend sigue igual:** `OK=170 FAIL=5`, los cinco negativos intencionados de
siempre.

## Lo que esto no arregla

De los diez sitios del doc 100, esto une dos. **Quedan ocho**, y siete de ellos siguen sin nada
que los ate: el valor del enum, el reconocimiento en `ProjectMethodBodyModel.cpp`, el nodo HIR, el
nodo MIR y su mapeo, los cinco puntos de `ProjectOopLlvmLowerer.cpp`, el `build*` del runtime, el
método de la biblioteca `.ns`, y la tabla de imports —que sí quedó unificada en el doc 102 pero
sigue siendo un sitio que hay que tocar.

Lo que este cambio demuestra es que la forma de atarlos existe y es barata: un `switch` sin
`default` sobre el enum. Donde el sitio sea un `switch` ya (los `case` de nombre, el mapeo
HIR→MIR) probablemente ya lo esté; donde sea una cadena de `if`, es el mismo trabajo que este.
Queda anotado, no hecho, y con la forma concreta escrita.
