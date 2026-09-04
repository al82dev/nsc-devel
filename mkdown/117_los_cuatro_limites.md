# 117 — Los cuatro límites, cerrados

Fecha: 2026-09-03
Ámbito: sólo el compilador (`A:\nsc-devel`).

Los cuatro que quedaban anotados como limitaciones al cerrar el doc 116. Tres eran defectos del
compilador y uno era una decisión a la que le faltaba la mitad que la hace usable.

---

## 1. `x / cero_const` — el diagnóstico llega al caso corriente

El doc 115 dejó el diagnóstico sólo para el cero **escrito en la expresión**. Un `const int cero = 0;`
usado como divisor compilaba, porque `ExpressionResult::integerConstant` no se rellena para un
identificador.

**Y no se rellena a propósito**, que es lo que hacía la corrección menos obvia de lo que parece:
ese campo lo consume `commonNumericType` para decidir el tipo común de una operación, así que darle
un valor a cada `const` local **cambiaría la promoción de expresiones que hoy compilan**.

Por eso el valor se guarda **aparte** —`MethodLocalSymbol::constantIntegerValue`, plegado al
declarar el local— y se lee en un solo sitio: la pregunta «¿el divisor es cero?». No tipa nada.

```
const int cero = 0;   int malo = 1 / cero;    -> division by the constant zero
const int tres = 3;   int ok  = 9 / tres;     -> compila
int cero = 0;         int malo = 1 / cero;    -> compila, trampa en ejecucion
```

### El límite, medido y no supuesto

```csharp
const int a = 5; const int b = 5; const int cero = a - b;   // x / cero SIGUE compilando
```

Un `const` derivado de **otros `const` locales** no se pliega, y el motivo es concreto: el evaluador
resuelve nombres a través de un `ConstantMemberResolver` que sabe de **miembros** `const` de tipo, no
de locales — y los locales dependen del ámbito, que el evaluador no tiene. Cerrarlo es darle al
resolvedor un espacio de ordinales para locales. Se puede, y no se ha hecho.

Para ése y para cualquier divisor no constante, la guarda en tiempo de ejecución es la respuesta y
es correcta: el programa muere con `DivisionByZero` en vez de compilar a poison.

---

## 2. `enum.ToString()` — fuera la restricción del receptor

Era la peor de las cuatro, porque **era una limitación del compilador escrita como si fuera una
regla del lenguaje**:

```
the compiler-provided 'ToString' on an enum needs a receiver it can read more than once;
bind this value to a local first
```

La causa era la forma de la bajada: una cadena de condicionales que bajaba el receptor **una vez por
enumerador**, así que `Dame().ToString()` habría llamado a `Dame()` una vez por nombre.

### Un nodo propio, y una cadena de `select` sin saltos

`ProjectOopHirNodeKind::EnumName` — `children[0]` el valor, `children[1]` la cadena de reserva, y de
ahí pares (constante, nombre) en orden de declaración. En el IR:

```llvm
%static.call   = call i32 @...Dame()          ; UNA vez
%enum.name.is  = icmp eq i32 %static.call, 0
%enum.name     = select i1 %enum.name.is, ptr @__nsc_str_8, ptr @__nsc_str_7
%enum.name.is1 = icmp eq i32 %static.call, 1
%enum.name2    = select i1 %enum.name.is1, ptr @__nsc_str_9, ptr %enum.name
...
```

Una cadena de literales es una cadena de **punteros a globales constantes**, así que un `select`
entre dos es legal y no cuesta nada — ninguna rama tiene efectos ni puede fallar. **Sin saltos, sin
bloques nuevos, y el valor evaluado una sola vez.**

**Medido:** con un receptor que incrementa un contador, `llamadas == 1`, y en el IR hay **una** sola
llamada a `Dame` frente a tres comparaciones. La fixture `c34` cuenta las llamadas — sin eso, una
bajada que llamara tres veces daría el nombre correcto y pasaría igual.

### El fondo de la cadena sigue siendo el nombre del tipo, y ahora se sabe por qué

`(Color)99` devuelve `"Color"`, no `"99"`. Devolver el número exigiría convertir un entero a cadena
**en ejecución**, o sea una llamada a `String.ToString(int)` como rama de reserva — y ahí está el
obstáculo real, que no es de coste: **los bindings de llamada se indexan por nodo de sintaxis**, y
ese nodo ya lleva el binding de la operación núcleo. Meter una llamada más exigiría un nodo de AST
sintético que este modelo no fabrica.

Es una razón concreta y comprobable, no una preferencia. La condición de reapertura sigue escrita.

---

## 3. Sobrecargas genéricas por **tipos** — se le pasa el problema a quien tiene los datos

El doc 116 cerró la mitad fácil: separar por **cuenta de parámetros**. Quedaba la difícil —
`F<T>(T)` contra `F<T>(T[])`, las dos con un parámetro— y el motivo por el que era difícil sigue
siendo cierto: **elegir necesita los tipos de los argumentos, y la pasada genérica corre antes del
modelo semántico.**

**La solución no es adivinar: es no elegir.** Cuando varias plantillas comparten nombre y aridad
genérica, se materializan **todas** con el **mismo nombre especializado**. A partir de ese momento
dejan de ser un problema de genéricos y son **sobrecargas ordinarias** — métodos no genéricos, mismo
nombre, listas de parámetros distintas — que este compilador resuelve por tipos desde siempre.

```csharp
public static int F<T>(T a)              { return 1; }
public static int F<T>(T[] a)            { return 100 + a.Length; }
public static int F<T>(T[] a, int extra) { return a.Length + extra; }
```

Las tres conviven y cada llamada va a la suya.

### Y el duplicado de verdad se detecta mejor que antes

Dos plantillas que tras sustituir tienen la **misma** lista de parámetros ya no dan un error de
genéricos, sino el de siempre, en su sitio:

```
duplicate method signature 'F__g1__int(builtin:int)' in type 'pz.U';
return type is not part of overload identity
```

### El desempate de la clave cambió, y es lo que evitaba un verde falso

La clave de una plantilla que colisiona lleva ahora **su posición de declaración**, no la cuenta de
parámetros. Con la cuenta, `F<T>(T)` y `F<T>(T[])` habrían compartido clave — y por tanto **clave de
especialización** — así que la segunda habría dado en la caché de la primera y **una de las dos
sobrecargas habría desaparecido en silencio**. No un rojo: un verde que mide otra cosa.

La detección de duplicados a nivel de plantilla queda **sólo para tipos**: dos clases o structs con
el mismo nombre y aridad siguen siendo un choque, porque ahí no hay lista de parámetros que los
distinga.

---

## 4. `string` son bytes — y ahora eso tiene salida

Esto **no era un defecto**: es la decisión del doc 114 (ID 125). `Length` cuenta bytes y es O(1);
`s[i]` da un byte. Lo que faltaba es la mitad que la hace usable — sin ella, «bytes opacos» es un
callejón sin salida: no hay forma de contar caracteres, de recorrerlos, ni de saber si una cadena
que viene de fuera es UTF-8 válido.

`lib/NativeSharp.Core/Utf8.ns`, con dos propiedades deliberadas:

- **No es el comportamiento por omisión.** Hay que llamarlo. Quien paga el O(n) sabe que lo paga.
- **Los nombres dicen lo que cuestan.** `CodePointCount` no se llama `Length`.

`SequenceLength` · `DecodeAt` · `NextIndex` · `CodePointCount` · `IsValid` · `IsBoundary` ·
`BoundaryAtOrBefore` · `EncodedLength` · `Encode`.

### La validación es estricta, y ésa es la mitad que protege

Un decodificador permisivo que acepta secuencias **sobrelargas** es un agujero de seguridad clásico:
`C0 AF` decodifica a `/` en un decodificador flojo, y por ahí se han saltado comprobaciones de rutas
durante veinte años. Se rechazan las sobrelargas, los sustitutos `D800–DFFF`, todo lo que pase de
`10FFFF`, los bytes de continuación sueltos y las secuencias cortadas.

### Y las secuencias esperadas no salen de mi cabeza

```
U+0041  'A'    1 byte   65
U+00F1  'ñ'    2 bytes  195 177
U+20AC  '€'    3 bytes  226 130 172
U+1D11E clave  4 bytes  240 157 132 158
```

Generadas con el codificador UTF-8 de Python — un tercero. **Escribirlas a ojo es exactamente como
me equivoqué dos veces en el `c30` con la aritmética de coma flotante y una en el `c31` con la
longitud de una cadena.** La fixture `c36` decodifica lo que acaba de codificar, y mide en una línea
la diferencia que la decisión pone sobre la mesa: **diez bytes, cuatro caracteres.**

---

## 5. Lo medido

| | |
|---|---|
| `./sweep.sh llvm` | **OK=181 FAIL=5** (los cinco negativos; eran 180) |
| `nsc.cc1.integer-evaluator` | 39/39 |
| `nsc.llvm-lowering` | 39/39 |
| casos del corpus | **56 → 112 trabajos** |
| `Dame().ToString()` | **una** llamada en el IR, tres comparaciones |
| duplicado genuino de plantilla | detectado por el modelo de miembros, con mejor mensaje |

**No verificado desde el contenedor:** que `c34`, `c35` y `c36` pasen ejecutando.

## 6. Números esperados

```
ctest      281   (279 + c36 + ... ver nota)
trabajos   112   (56 casos x 2; eran 110)
triage   16/16
```

`ctest` sube en uno por cada fixture nueva, porque cada una es su propia prueba desde el reparto del
doc 116.
