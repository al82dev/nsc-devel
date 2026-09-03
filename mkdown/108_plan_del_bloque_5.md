# 108 -- Plan del Bloque 5: contenedores, algoritmos y memoria

Fecha: 2026-09-02
Ámbito: solo el compilador (`A:\nsc-devel`).

**Criterio de cierre del bloque (doc 81, escrito antes de empezar):** *AST, símbolos y MIR pueden
vivir en una arena por compilación.*

**Depende de:** Bloque 3. Toda esta capa toma `Span<T>` como entrada, y el Bloque 3 la cerró.

---

## 0. Lo que el lenguaje permite hoy, medido antes de diseñar nada

Escribí un proyecto de sonda y lo compilé con el frontend real **antes** de fijar una sola firma,
porque tres de estos hechos cambian la API y descubrirlos a mitad de camino costaría reescribirla.

| Qué | Resultado |
|---|---|
| Método genérico estático sobre `ReadOnlySpan<T>` / `Span<T>` | **Funciona** |
| **Inferencia del argumento de tipo** (`Alg.Find(v, 6)`) | **NO existe.** `receiver/type '…Alg' has no compatible member named 'Find'`. Hay que escribir `Alg.Find<int>(v, 6)` |
| Local de tipo genérico (`T t = span[i];`) | **No parsea** (`expected ';', found 't'`). Se usa `var t = span[i];` |
| Pasar un grupo de métodos como delegado en una llamada | **No.** `method group 'EsPar' can only be used as a call target`. Hay que ligarlo antes: `Predicate<int> p = EsPar;` y pasar `p` |
| `virtual` / `override`, y que un método NO virtual de la base llame al virtual de la derivada | **Funciona**, también con `raii` sobre la derivada |
| Clase con campo `owned byte[]` y campo `owned` del propio tipo (lista enlazada) | **Funciona** (es lo que ya hace `LinkedListNode`) |
| `Spans.Of<byte>(arr).Data()` sobre un **campo** para obtener `byte*` | **Funciona** |

Las dos primeras filas son las que mandan sobre la ergonomía: **todas las llamadas llevan el
argumento de tipo escrito**, y **todo predicado o comparador se liga a una local antes de pasarlo**.
No es bonito; es lo que hay, y es mejor saberlo ahora que a mitad de la fase C.

---

## 1. Dónde vive cada cosa, y por qué no donde parecería

`Predicate<T>`, `Comparison<T>` y `Action<T>` están en **`NativeSharp.Core.Collections.Generic`**, y
ese paquete ya referencia a `NativeSharp.Core`. Poner `Algorithm` en `Core` obligaría a que `Core`
referenciara a `Collections.Generic`: **una dependencia circular**. Así que:

```
NativeSharp.Core                       StringBuilder, Allocator, ArenaAllocator
NativeSharp.Core.Collections.Generic   Algorithm          (donde estan los delegados)
NativeSharp.Core.IO                    TextWriter y sus tres implementaciones
```

`Algorithm` junto a las colecciones no es un apaño: es donde vive `<algorithm>` respecto de
`<vector>` en cualquier biblioteca estándar.

---

## 2. Fase A -- `Algorithm` (SHR-052): **nueve funciones, no dieciocho**

La cifra sale de contar los usos reales en `src/`, no de copiar `<algorithm>`:

```
find 13 · max 7 · min 5 · sort 2 · any_of 2 · reverse 1 · remove_if 1 · find_if 1 · all_of 1
```

**No se usan operaciones de heap, ni `partition`, ni `unique`.** No entran.

```csharp
Algorithm.Find<T>(ReadOnlySpan<T> donde, T que)            -> int   indice o -1
Algorithm.FindIf<T>(ReadOnlySpan<T> donde, Predicate<T>)   -> int   indice o -1
Algorithm.AnyOf<T>(ReadOnlySpan<T>, Predicate<T>)          -> bool
Algorithm.AllOf<T>(ReadOnlySpan<T>, Predicate<T>)          -> bool
Algorithm.Min<T>(T a, T b)                                 -> T
Algorithm.Max<T>(T a, T b)                                 -> T
Algorithm.Reverse<T>(Span<T>)                              -> void
Algorithm.RemoveIf<T>(Span<T>, Predicate<T>)               -> int   nueva longitud
Algorithm.Sort<T>(Span<T>)                                 -> void
Algorithm.SortWith<T>(Span<T>, Comparison<T>)              -> void
```

**`SortWith` y no `Sort` sobrecargado, y ese nombre es obligado.** ID 132: las plantillas genéricas
se identifican por `nombre` + `aridad genérica`, no por lista de parámetros, así que `Sort<T>(span)`
y `Sort<T>(span, cmp)` chocan con *duplicate generic template identity*. Se rodea renombrando, que
es lo mismo que se hizo en el Bloque 3 (`Of` → `OfRange`). `List<T>.Sort()` y
`List<T>.Sort(Comparison<T>)` **sí** conviven porque son miembros no genéricos de una clase
genérica: otra cosa.

**`RemoveIf` devuelve la nueva longitud, no un iterador.** `std::remove_if` devuelve el nuevo final
y el llamador hace `erase`. Aquí no hay iteradores: compacta en sitio y dice cuántos quedan, que es
la misma información sin inventar un tipo.

### Sort: **heapsort**, y la razón no es la velocidad

`O(n log n)` **garantizado**, en sitio, **sin recursión**. Un quicksort medio es dos o tres veces
más rápido y tiene un peor caso cuadrático y una pila que crece; un compilador ordenando tablas de
símbolos prefiere que el peor caso sea el caso medio y que no haya profundidad de pila que estimar.
Introsort daría lo mejor de ambos a cambio de tres veces más código en un sitio donde no se puede
ejecutar nada para comprobarlo.

Heapsort **no es estable**, y `std::sort` tampoco, así que no se pierde ninguna garantía que el
código de origen tuviera.

### `List<T>.Sort()` pasa a delegar

Hoy `List<T>.Sort()` es una **insertion sort O(n²)** escrita a mano, y su propio comentario dice que
es así "porque este paquete no tiene forma de ejecutar y observar el binario compilado". Ese motivo
ya no aplica: el corpus ejecuta. Dejar la insertion sort ahí y añadir un heapsort en `Algorithm`
sería crear la sexta copia de una regla en dos días, y esta vez a sabiendas.

`List<T>.Sort()` y `List<T>.Sort(Comparison<T>)` pasan a llamar a `Algorithm`. **Lo que cambia
observablemente:** deja de ser estable. Real C# tampoco lo es, y el único llamador del árbol es
`examples/project_stage52/Main.ns:41` sobre enteros, donde la estabilidad no se puede observar.
Queda escrito aquí porque es un cambio de comportamiento, no una optimización invisible.

---

## 3. Fase B -- `StringBuilder` (SHR-019)

Sustituye los 6 usos de `<sstream>`. Un `char[]` que dobla, exactamente el patrón de `ByteBuffer`
del Bloque 4, con `Append` para lo que el compilador concatena: `string`, `char`, `int`, `long`,
`uint`, `ulong`, `bool`, más `AppendLine`, `Clear`, `Length`, `ToString`.

**No lleva formateo con plantilla (`AppendFormat`).** `String.ToString(...)` ya cubre cada tipo por
separado y un mini-lenguaje de formato es una superficie entera que nadie ha pedido por su nombre.

---

## 4. Fase C -- `TextWriter` (SHR-020)

Una base con **un solo método virtual**:

```csharp
public class TextWriter
{
    public virtual void Write(string texto) { }      // lo unico que cambia por destino
    public void WriteLine(string texto)              // todo lo demas se construye encima
    public void Write(int valor) ...
}
```

Tres implementaciones: `ConsoleTextWriter` (con un `bool` para el canal de error),
`StringTextWriter` (envuelve un `StringBuilder`), `FileTextWriter` (acumula y vuelca con
`File.WriteAllText`/`AppendAllText`).

**Un solo método virtual y el resto encima, a propósito.** Cada método virtual es un punto donde
una implementación futura puede ser incoherente con las demás. Con uno, `WriteLine` significa lo
mismo en las tres por construcción.

---

## 5. Fase D -- `Allocator` + `ArenaAllocator` (SHR-038 / SHR-039)

```csharp
public class Allocator
{
    public virtual byte* Allocate(int bytes) { return null; }
    public virtual void Reset() { }
    public virtual long BytesUsed() { return 0; }
}
```

`ArenaAllocator` es una **cadena de bloques que no se mueven**, y eso es la definición del problema:
una arena reparte punteros que tienen que seguir siendo válidos, así que no puede realojar y copiar.

Cada bloque es un `owned byte[]`. Un arreglo de NativeSharp vive en el heap y **no se mueve nunca**,
así que `Spans.Of<byte>(bloque).Data()` da un `byte*` estable — medido en la sonda. El bloque
siguiente cuelga de un campo `owned` del propio tipo, igual que `LinkedListNode`.

`Allocate(n)` alinea a 8 y reparte por incremento dentro del bloque actual; si no cabe, encadena
uno nuevo (el mayor entre el tamaño por omisión y `n`, para que una petición grande no falle nunca).
`Reset()` pone todos los bloques a cero usado **sin liberar la memoria**: eso es lo que hace que una
arena valga la pena entre compilaciones.

**No hay `HeapAllocator`, y es deliberado.** Un allocator cuyo `Free` fuera un no-op sería peor que
no tener `Free`: parecería que libera. La abstracción es `Allocate` + `Reset`, que es exactamente lo
que una arena por compilación necesita, y la única implementación hoy es la arena. Cuando aparezca
un consumidor que necesite liberar pieza a pieza, se añade con ese motivo escrito.

---

## 6. Cómo se prueba

Tres fixtures nuevas, y una razón para que sean tres y no una: un fallo en la fase A no se debe
mezclar con uno de la D.

```
tests/cc1/c27_algoritmos       las nueve funciones, con casos de borde (vacio, un elemento,
                               todos iguales, ya ordenado, orden inverso)
tests/cc1/c28_texto            StringBuilder y las tres implementaciones de TextWriter
tests/cc1/c29_arena            ArenaAllocator: alineacion, encadenado, Reset conserva memoria,
                               y que los punteros repartidos SIGUEN VALIDOS despues de encadenar
```

La última comprobación es la que de verdad mide la arena: se pide memoria hasta forzar un bloque
nuevo, se escribe en los punteros **viejos** y se vuelve a leer. Si la arena realojara, ahí es donde
se vería.

---

## 7. Riesgos, dichos antes de empezar

- **`Sort` no se puede observar desde el contenedor.** El frontend llega al IR; que heapsort ordene
  bien lo dice el fixture al ejecutar en Windows. Por eso c27 no comprueba "está ordenado" sino
  **cada elemento contra su posición esperada** en cinco formas distintas de entrada.
- **La aritmética de punteros de la arena es la parte peligrosa.** Un fallo de alineación o de
  encadenado no da un error: da una escritura en el sitio equivocado. c29 escribe patrones
  distinguibles y los vuelve a leer todos al final, no según los escribe.
- **ID 132 va a molestar otra vez.** Cualquier par de métodos genéricos con el mismo nombre choca.
  Se rodea renombrando y se anota; no se arregla dentro de este bloque.
- **Cambiar `List<T>.Sort` toca una colección con pruebas.** Va en su propia fase, después de que
  `Algorithm` esté verde por su cuenta, para que un rojo tenga una sola causa posible.

## 8. Orden

```
A  Algorithm                 sin incognitas, y lo usa la fase siguiente
A' List<T>.Sort delega       despues de que A este verde, no antes
B  StringBuilder             mismo patron que ByteBuffer, ya medido
C  TextWriter                encima de B
D  Allocator + ArenaAllocator la unica con aritmetica de punteros; ultima y sola
```
