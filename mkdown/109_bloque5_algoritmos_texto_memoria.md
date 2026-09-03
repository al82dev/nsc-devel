# 109 -- Bloque 5: algoritmos, texto y memoria

Fecha: 2026-09-02
Ámbito: solo el compilador (`A:\nsc-devel`).
Plan previo: `mkdown/108_plan_del_bloque_5.md`.

**Criterio de cierre (doc 81, escrito antes de empezar):** *AST, símbolos y MIR pueden vivir en una
arena por compilación.* La capacidad está; el consumidor —el `nsc` escrito en NativeSharp— es el
Bloque 8.

---

## Lo que se midió antes de escribir la primera firma

Compilé un proyecto de sonda con el frontend real **antes** de fijar ninguna API, porque tres de
estos hechos la cambian y descubrirlos a mitad de camino costaría reescribirla:

| Qué | Resultado |
|---|---|
| Método genérico estático sobre `Span<T>` / `ReadOnlySpan<T>` | Funciona |
| **Inferencia del argumento de tipo** (`Algorithm.Find(v, 6)`) | **No existe.** Da `has no compatible member named 'Find'`, que no dice qué falta |
| Local de tipo genérico (`T t = span[i];`) | **No parsea.** Se usa `var t = ...` |
| Grupo de métodos como argumento | **No.** `method group 'EsPar' can only be used as a call target`. Hay que ligarlo a una local primero |
| `virtual`/`override`, base no virtual llamando al virtual de la derivada | Funciona, también con `raii` |
| Campo `owned byte[]` + campo `owned` del propio tipo | Funciona (es lo que hace `LinkedListNode`) |
| `Spans.Of<byte>(campo).Data()` → `byte*` estable | Funciona — **y de esto depende la arena entera** |

Las dos primeras mandan sobre la ergonomía de todo el bloque: **cada llamada lleva el argumento de
tipo escrito** y **cada predicado se liga a una local antes de pasarlo**.

---

## Fase A — `Algorithm` (SHR-052)

`lib/NativeSharp.Core.Collections.Generic/Algorithm.ns`. **Nueve funciones**, y la cifra sale de
contar `src/`, no de copiar `<algorithm>`: `find` 13 · `max` 7 · `min` 5 · `sort` 2 · `any_of` 2 ·
y una cada uno `reverse`, `remove_if`, `find_if`, `all_of`. **No hay operaciones de heap, ni
`partition`, ni `unique`**, porque no se usan.

**Vive en `Collections.Generic` y no en `Core`** porque `Predicate<T>` y `Comparison<T>` están ahí y
`Core` no puede referenciarlo sin un ciclo. Es además donde vive `<algorithm>` respecto de
`<vector>`.

### Las decisiones que no son obvias

**`AnyOf` sobre vacío es `false`, `AllOf` sobre vacío es `true`.** Verdad vacua. Parece raro y es lo
correcto: cambiarlo rompería la propiedad de que partir un rango en dos y preguntar por cada mitad
da lo mismo que preguntar por el entero.

**`RemoveIf` devuelve la nueva longitud, no un iterador.** `std::remove_if` devuelve el nuevo final
y el llamador hace `erase`; aquí no hay iteradores, y un entero lleva la misma información sin
inventar un tipo para transportarla.

**`SortWith` y no una sobrecarga de `Sort`.** ID 132: las plantillas genéricas se identifican por
nombre + **aridad genérica**, no por lista de parámetros, así que `Sort<T>(span)` y
`Sort<T>(span, cmp)` chocarían. Se rodea renombrando, igual que el Bloque 3 hizo `Of` → `OfRange`.
(`List<T>.Sort()` y `List<T>.Sort(Comparison<T>)` sí conviven: son miembros **no genéricos** de una
clase genérica, que es otra cosa.)

**Heapsort, y la razón no es la velocidad.** O(n log n) **garantizado**, en sitio, **sin
recursión**. Un quicksort medio es dos o tres veces más rápido, con peor caso cuadrático y una pila
que crece con la entrada; un compilador ordenando tablas de símbolos prefiere que el peor caso sea
el caso medio y no tener profundidad de pila que estimar. Introsort daría lo mejor de ambos a cambio
de tres veces más código, en un sitio donde el contenedor no puede ejecutar nada para comprobarlo.
No es estable, y `std::sort` tampoco.

### `List<T>.Sort` deja de tener su propia ordenación

Era una **insertion sort O(n²)** escrita a mano, y su propio comentario decía que era así *"porque
este paquete no tiene forma de ejecutar y observar el binario compilado"*. Ese motivo ya no aplica:
el corpus ejecuta. Dejarla ahí y añadir un heapsort al lado habría sido la **sexta copia de una
regla en dos días**, esta vez a sabiendas.

**Cambio observable, dicho claramente:** `List<T>.Sort()` deja de ser estable. Real C# tampoco lo
garantiza, y el único llamador del árbol (`examples/project_stage52/Main.ns:41`) ordena enteros,
donde la estabilidad no se puede observar.

---

## Fase B — `StringBuilder` (SHR-019)

`lib/NativeSharp.Core/StringBuilder.ns`. Un `char[]` que dobla; mismo patrón que `ByteBuffer`.
Sustituye los 6 usos de `<sstream>`: construir un texto de N trozos con `String.Concat` cuesta
O(N²) bytes copiados y N cadenas intermedias que se tiran.

**No hay `AppendFormat`.** `String.ToString(...)` ya cubre cada tipo por separado; un mini-lenguaje
de formato es una superficie entera —parseo, escapes, errores en tiempo de ejecución— que nadie ha
pedido por su nombre.

**Se llama `ToOwnedString()` y no `ToString()`, y no es un capricho.** `Object.ToString()` es
virtual y su contrato —escrito en `Object.ns`— es devolver el nombre del tipo, una **constante que
vive en el binario y no asigna**. Aquí hay que asignar y devolver algo que el llamador posee: otro
contrato. El compilador lo detecta (`method hides an inherited virtual signature; use override`) y
hace bien. Sobrescribirlo habría sido peor que renombrar: un llamador con una referencia `Object`
recibiría una cadena asignada creyendo que es una constante, y la fuga no se vería en ningún sitio.

---

## Fase C — `TextWriter` (SHR-020)

`lib/NativeSharp.Core.IO/TextWriter.ns`, con `ConsoleTextWriter`, `StringTextWriter` y
`FileTextWriter`.

**Un solo método virtual.** `Write(string)`. Todo lo demás —`WriteLine`, los `Write` de números,
`WriteIndent`— se construye **encima**, no al lado. Cada método virtual es un sitio donde una
implementación futura puede ser incoherente con las demás (un `FileTextWriter` cuyo `WriteLine`
usara CRLF mientras el de consola usa LF, y nadie lo nota hasta comparar dos salidas). Con uno solo,
`WriteLine` significa lo mismo en las tres **por construcción**. El coste es una llamada virtual por
trozo en vez de una por línea.

**`\n` y no `\r\n`, también en Windows.** El proyecto compara salidas byte a byte entre corridas y
entre máquinas; un final de línea que dependa del sistema convierte cualquier comparación en una
diferencia falsa.

**`FileTextWriter` acumula y vuelca en `Flush`.** La alternativa era `File.AppendAllText` por trozo:
un `CreateFileA` + `SetFilePointer` + `WriteFile` + `CloseHandle` **por coma**. La contrapartida,
dicha claramente: si el proceso muere antes del `Flush`, no hay fichero — correcto para un
compilador (un fichero de salida a medias es peor que ninguno) y la herramienta equivocada para una
bitácora que deba sobrevivir a un cuelgue; para eso está el canal de error, que no acumula.

---

## Fase D — `Allocator` + `ArenaAllocator` (SHR-038 / SHR-039)

**Tres operaciones, y no hay `Free`.** `Allocate`, `Reset`, `BytesUsed`. Un `Free(puntero)` en una
arena no puede hacer nada, y uno que fuera un no-op sería **peor que no tenerlo**: el llamador
creería que libera. Y no hay consumidor que necesite liberar pieza a pieza — un compilador asigna
mucho, libera casi nada y tira todo al terminar la unidad de compilación, que es exactamente el
patrón para el que existe una arena. **Tampoco hay `HeapAllocator`**, por lo mismo.

**La propiedad que define la arena: los bloques no se mueven.** Reparte punteros que el llamador
guarda; si al crecer realojara y copiara —como hacen `List<T>`, `ByteBuffer` o `StringBuilder`—
todos quedarían colgando. Por eso **no duplica un arreglo: encadena uno nuevo** y el viejo se queda
donde estaba. Cada bloque es un `owned byte[]`, que vive en el heap y no se mueve nunca; el
siguiente cuelga de un campo `owned` del propio tipo, igual que `LinkedListNode`.

**Alineación a 8.** Repartir sin alinear funcionaría hoy en x86-64 para casi todo y sería una bomba
de relojería en cuanto algo pidiera una lectura alineada o se cambiara de arquitectura.

**Una petición más grande que un bloque encadena uno a medida.** Sin eso, pedir 100 KiB a una arena
de bloques de 64 KiB devolvería `null` para siempre y el fallo aparecería muy lejos de aquí.

**`Reset` conserva los bloques.** Tirarlos y quedarse con el primero ahorraría memoria y convertiría
cada compilación en la primera; una arena que ya sabe que el trabajo típico ocupa cinco bloques debe
seguir teniendo cinco.

---

## Cómo se prueba

Tres fixtures, y son tres para que un fallo de la fase A no se mezcle con uno de la D.

- **`c27_algoritmos`** — las nueve funciones con sus bordes. **`Sort` no se comprueba con "está
  ordenado"**, sino **elemento a elemento** contra la posición esperada en cinco formas de entrada
  (desordenada con duplicados, ya ordenada, orden inverso, todos iguales, negativos mezclados) más
  un tamaño de 13 —ni potencia de dos ni pequeño— donde el índice del hijo derecho se sale del
  montículo. Un "está ordenado" pasaría igual con un sort que perdiera o duplicara elementos.
- **`c28_texto`** — crecer sin perder nada (100 caracteres comprobados uno a uno), `Clear`
  conservando capacidad, `ToOwnedString` con el tamaño exacto. Y **el mismo productor de texto
  volcado a tres destinos**, con las salidas de memoria y de fichero comparadas **byte a byte**:
  eso es lo que justifica todo el diseño de `TextWriter`.
- **`c29_arena`** — alineación exacta (1 byte cuesta 8, 9 cuestan 16, 8 cuestan 8), y la
  comprobación central: se reparten 40 trozos forzando varios bloques, **se guardan los punteros
  viejos**, y al final se releen todos. Si la arena realojara, se vería ahí y en ningún otro sitio.

---

## Verificación hecha en el contenedor

```
./fe lib/NativeSharp.Core/NativeSharp.Core.nsproj                       -> OK
./fe lib/NativeSharp.Core.Collections.Generic/...nsproj                 -> OK
./fe lib/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj                 -> OK
./fe tests/cc1/c27_algoritmos/c27_algoritmos.nsproj  --stage=llvm       -> OK
./fe tests/cc1/c28_texto/c28_texto.nsproj            --stage=llvm       -> OK
./fe tests/cc1/c29_arena/c29_arena.nsproj            --stage=llvm       -> OK
./sweep.sh llvm                                      -> OK=173  FAIL=5
```

Los 5 son los negativos intencionados de siempre. El corpus pasa de 170 a 173.

`Cc1CorrectnessTests.cpp` compila con `clang++-18 -std=c++20 -Wall -Wextra` contra las cabeceras
reales, sin avisos.

**Lo que el contenedor no puede verificar:** que heapsort ordene, que la aritmética de punteros de
la arena reparta donde dice, y que `FileTextWriter` escriba el fichero. El frontend llega al IR y
para. Eso lo mide `all.bat` en Windows, y es lo que las tres fixtures existen para decir.

---

## El único rojo de la primera corrida, y era mío

```
FAIL cc1_c28_texto -O0 -- exit 14, se esperaba 42
FAIL cc1_c28_texto -O2 -- exit 14, se esperaba 42
```

Los otros 94 trabajos en verde, incluidos c27 y c29. El código 14 apunta a la comparación del
bloque de `Append` numéricos, y la causa es de una línea:

```csharp
public static owned string ToString(bool value)
{
    if (value) { return Copy("True"); }
    return Copy("False");
}
```

**`"True"`/`"False"` con mayúscula** — la convención de .NET, que es la que el proyecto sigue en
todo lo demás. Yo escribí `"true"/"false"` en el fixture por costumbre de C++ y **sin mirar la
biblioteca**. El fallo era del fixture, no del compilador ni de `StringBuilder`.

Dos cosas de esto:

**Estaba en dos sitios, no en uno.** La misma suposición estaba en la comprobación de `Volcar`
(`"id=42 ok=true\n…"`), porque `TextWriter.Write(bool)` pasa por la misma conversión. Arreglar sólo
el que falló habría movido el rojo a la comprobación 18 en la corrida siguiente. Los dos corregidos.

**Y la comprobación era demasiado gorda.** Cuatro `Append` en una sola comparación de cadenas: el
fallo decía "no es igual" y no cuál de los cuatro se había equivocado, ni si el problema era la
conversión o la sobrecarga elegida. Ahora va partido —cada pieza con su código de salida— más la
comparación de los cuatro juntos, que es lo que de verdad se usa. Cuatro líneas más a cambio de que
el siguiente rojo diga el nombre del culpable.

---

## La auditoría antes de la segunda corrida, y el bug que encontró

Después del rojo de c28 no volví a lanzar nada: repasé **cada expectativa literal de los tres
fixtures leyendo la biblioteca**, no suponiéndola. Salieron tres cosas, y una es un defecto real.

### 1. `ArenaAllocator.Allocate` tiraba la cadena después de un `Reset` — bug

```csharp
// antes
if (actual.usado + pedido > actual.almacen.Length)
{
    owned ArenaBlock nuevo = new ArenaBlock(tamanoNuevo);
    actual.siguiente = move(nuevo);      // PISA lo que ya colgara de ahi
    ...
}
```

En la primera pasada no se nota: `siguiente` siempre es `null`, así que no hay nada que pisar. Se
nota **después de un `Reset`**: `Reset()` rebobina todos los bloques a cero y devuelve `actual` al
primero, así que al llenarse el primero otra vez esa asignación **tiraba la cadena entera** —se
liberaba, porque el campo es `owned`— y volvía a pedirle memoria al sistema.

O sea: **`Reset()` no conservaba nada**, que es exactamente lo contrario de lo que su propio
comentario promete y lo único que hace que una arena valga la pena entre compilaciones. Y
`cuantosBloques` seguía subiendo mientras los bloques se caían, así que además **mentía**.

Arreglado avanzando por los bloques que ya existen antes de crear uno:

```csharp
while (actual.usado + pedido > actual.almacen.Length && actual.siguiente != null)
{
    actual = actual.siguiente;
}
if (actual.usado + pedido > actual.almacen.Length) { ...encadenar uno nuevo... }
```

**Y c29 no lo cazaba.** Comprobaba `BlockCount()` justo después del `Reset` —cuando todavía es
correcto— y luego sólo pedía 16 y 40 bytes, sin volver a pasar del primer bloque. Le añadí el paso
**4b**: rellenar otra vez hasta pasar del primer bloque y exigir que el número de bloques **no haya
crecido**. Con el bug, crecía. Ésa es la prueba que faltaba.

### 2. c28 dependía de cómo acabó la corrida anterior

El código 21 comprueba que el fichero **no** existe todavía —`FileTextWriter` acumula y no toca el
disco hasta el `Flush`—. La comprobación es correcta y era **frágil**: si c28 falla alguna vez entre
el `Flush` y el `Delete` del final, el fichero sobrevive y la corrida **siguiente** se pone roja en
el 21 por un motivo que no tiene nada que ver. Un rojo que no se reproduce dos veces, que es la peor
clase que hay. Ahora borra al entrar además de al salir.

### 3. Dos constructores no se ejercitaban en ningún sitio

`StringBuilder(int)` y `StringTextWriter(int)`. Y el primero es justo el camino que más fácil
rompe: con capacidad 1, el crecimiento arranca desde un buffer mínimo, y la rama que sube una
capacidad absurda al mínimo (`if (capacidad < 1)`) no la recorría nadie. Cubiertos los tres casos
—capacidad 1, 0 y negativa— y el `StringTextWriter(2)` volcando el mismo productor y comparando
contra la salida del constructor por omisión.

### Lo que se verificó y estaba bien

- `String.ToString(int)` delega en `ToString(long)`: `-7` → `"-7"`, sin signo `+` ni relleno.
  `1234567890123` → sus trece dígitos. Las dos expectativas de c28 eran correctas.
- `File.AppendAllText` con cadena vacía es un `WriteFile` de 0 bytes, legal y sin efecto: el segundo
  `Flush` de c28 no duplica ni corrompe nada.
- `Spans.OfRange` con longitud 0 devuelve un span vacío y `CopyTo` no escribe nada — el crecimiento
  de `StringBuilder` desde vacío es seguro.
- Los 35 pares contiguos que c29 espera salen de 5 bloques × 7 pares; el umbral de 30 tiene margen.
- La alineación: `Alinear(1)=8`, `Alinear(8)=8`, `Alinear(9)=16`, `Alinear(1000)=1000`.

```
./sweep.sh llvm  ->  OK=173  FAIL=5   (despues de los tres cambios)
```

---

## Tres cosas que el lenguaje obligó a hacer distinto

1. **`raii` no vale para arreglos** (`raii local requires a NativeSharp class/interface reference
   type or native string`). El patrón para un arreglo propio es el de `ByteBuffer`: local `const`,
   copia por `Span`, `delete[]` del viejo, asignación al campo.
2. **`base` es palabra reservada** y no se puede usar como nombre de variable.
3. **`Object.ToString()` bloquea el nombre `ToString`** para cualquier cosa que asigne — ver fase B.

---

## Registrado en los tres sitios

| Dónde | Qué hace |
|---|---|
| `run_cases.py` | Los ejecuta. 48 casos → **96 trabajos** (eran 90) |
| `nslink_pipeline.py` | Los ejecuta en la pasada de NSLink |
| `tests/unit/Cc1CorrectnessTests.cpp` | Los compila y enlaza dentro de `nsc.cc1.correctness` |

Las dos tablas de casos comprobadas idénticas caso por caso: **48 y 48**.

## Números esperados en la próxima corrida

```
ctest       244   (los tres van DENTRO de nsc.cc1.correctness: crecen sus comprobaciones,
                   no el numero de pruebas de ctest)
trabajos     96   (48 casos x 2, eran 90)
triage    16/16
```
