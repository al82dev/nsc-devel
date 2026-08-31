# Stage 56 / C8 — Raíz de tipos: `ToString` / `Equals` / `GetHashCode`

**Origen:** medido al cerrar C7 y aprobado por Alberto (*"sí, resuélvelo también"*). El lenguaje no
tenía raíz: `public override string ToString()` fallaba con *"no hay método heredado que
sobrescribir"*, `a.Equals(b)` con *"el tipo no tiene un miembro llamado Equals"*, y `object` no era
un nombre de tipo. Nada podía decir qué era, compararse consigo mismo, ni entrar en una tabla hash.

**Resultado:** `ctest` **237/237**, build limpio con `-DNSC_WARNINGS_AS_ERRORS=ON`, y ni un byte de
cambio en el layout de nada.

---

## 1. La decisión de diseño: no hay boxing

Se enuncia primero porque todo lo demás se sigue de ella.

```
object o = 5;          // error de compilación
object o = miStruct;   // error de compilación
```

No es evitar trabajo. Es la única opción coherente con el resto del lenguaje. Boxear significa una
**asignación en heap oculta**, y en un lenguaje **sin GC** que es explícito con la propiedad en todo
lo demás — `raii`, `owned`, `borrowed`, `move` — la pregunta *"¿quién libera la caja que hizo
`x.GetHashCode()`?"* no tiene buena respuesta. Cualquiera que se dé es peor que no boxear: o hay una
fuga por cada llamada, o hay un `owned string`/`raii` contagiándose hasta el sitio donde alguien
sólo quería un hash.

El mismo problema se resuelve sin cajas, y el resultado es más rápido además de más simple.

---

## 2. Cómo queda el lenguaje

### Clases: una raíz real

`NativeSharp.Core.Object` es una **clase escrita en NativeSharp**, no una ficción del compilador:

```
public class Object
{
    public virtual string ToString()          { return RuntimeHelpers.TypeName(this); }
    public virtual bool   Equals(Object other) { return this == other; }
    public virtual int    GetHashCode()        { return RuntimeHelpers.ReferenceHash(this); }
}
```

Toda clase declarada **sin base explícita** deriva implícitamente de ella. Que sea una clase de
verdad es lo que hizo que el constructor de vtables, el emisor de descriptores, `base.`, la cadena
de constructores y el recorrido de RTTI **no necesitaran ni un caso especial**: ven una clase base
ordinaria.

`object`, en minúscula, es su forma corta, igual que `string` lo es del string nativo. No es palabra
reservada del lexer y se resuelve **como último recurso**, después de que la búsqueda ordinaria
falla: un proyecto que declare su propio tipo `object` gana, y uno que no tenga la raíz a la vista
recibe `unknown type name 'object'` en vez de un parseo que significa otra cosa en silencio.

El `ToString()` por omisión responde el nombre **en tiempo de ejecución**, no el estático:

```
object asRoot = widget;
asRoot.ToString();      // "Widget"
```

sale del descriptor del objeto, no del tipo de la variable.

### Structs: los tres sintetizados, resueltos estáticamente

Un struct es un **valor**: no tiene puntero a vtable y no tiene sitio para uno — eso es exactamente
lo que probó C6 y lo que el header C certifica campo por campo. Así que no hereda los tres miembros;
los recibe **sintetizados por el compilador**, resueltos sobre el tipo **estático**, que es lo que
un struct es.

Sin vtable. Sin caja. Sin cambiar un byte del layout.

```
Vec2 a = {1, 2};
a.ToString();      // "Vec2" -- constante en tiempo de compilación
a.Equals(b);       // hoja por hoja
a.GetHashCode();   // FNV-1a sobre las mismas hojas
```

Y **lo declarado gana**: la síntesis corre sólo después de que la búsqueda ordinaria de miembros
falló, así que un struct que declara su propio `ToString` nunca queda tapado.

### Primitivos

`5.ToString()` ya funcionaba desde Stage 51 (fachada sobre los overloads de
`NativeSharp.Core.String`) y sigue igual.

---

## 3. El contrato de `Equals`/`GetHashCode` en structs, y por qué es de BITS

Ambos miembros leen las **hojas escalares** del valor: un `AggregateElement` por campo primitivo,
con structs anidados y arreglos fijos ya aplanados y con offsets absolutos. **El relleno queda
fuera a propósito** — un valor que cruzó un borde C trae en su relleno lo que el otro lado haya
dejado ahí, y compararlo haría que dos valores iguales discrepen.

Cada hoja se lee como **bits**, no como valor: nada en el backend pregunta de qué *tipo* es la hoja,
sólo cuántos bytes ocupa (el paso de MIR lo resuelve del mismo `valueLayoutForTypeKey` que lee todo
lo demás). La memoria no tiene tipo, así que un `float` se carga como `i32` y una referencia como
`i64`, sin una sola instrucción de cast.

Eso es el contrato entero, y es lo que hace que los dos miembros **concuerden por construcción**:

- comparar floats con `fcmp` y hashear sus bits rompe el par **dos veces**: `+0.0 == -0.0` pero
  hashean distinto, y un `NaN` nunca se iguala a sí mismo mientras hashea idéntico;
- leyendo los mismos bits para ambos, *"iguales implica mismo hash"* es cierto por construcción y
  no por cuidado.

Quien quiera otra semántica — comparación por valor de un campo `string`, comparación profunda a
través de una referencia — declara su propio `Equals` y su propio `GetHashCode`, y el compilador se
aparta por completo.

**Uniones:** rechazadas, con su razón. Sólo un miembro de una unión está vivo a la vez y nada en el
valor dice cuál, así que comparar o hashear todos los campos no significaría nada. C se niega a
comparar uniones por lo mismo. `ToString` sí funciona: un nombre es un nombre.

**`Equals` compara contra su propio tipo**, no contra una referencia a la raíz. Ensancharlo
implicaría boxear el argumento.

---

## 4. Qué cuesta

| | Costo |
|---|---|
| Objetos | **cero bytes.** `Object` no declara campos |
| Structs | **nada en absoluto.** Ni layout, ni ABI, ni un puntero de vtable que no tienen |
| Vtable de cada clase | tres slots |
| Descriptor de cada clase | un puntero, al nombre del tipo |
| Nombre del tipo | una **constante** en el binario, con el layout exacto de un string nativo (`{i64 longitud, [N+1 x i8] bytes}`). Devolverlo es una carga, no una asignación |

El descriptor pasó de seis campos a siete — **añadido al final**, así que ningún índice de campo
existente cambió.

Las dos primitivas que la raíz necesita y que no se pueden escribir en NativeSharp
(`RuntimeHelpers.TypeName`, `RuntimeHelpers.ReferenceHash`) son **operaciones núcleo**, el mismo
mecanismo que ya usaban `NativeClock.QueryCounterRaw` y compañía: el compilador las reconoce por
nombre completo y las baja él.

---

## 5. La raíz es opcional

Un proyecto que no tenga `NativeSharp.Core.Object` a la vista compila **exactamente** como antes de
C8: sin base implícita, sin slots extra, sin nombre en el descriptor.

Es la misma convención que `NativeSharp.Core.Exception` ya había establecido para `catch`/`throw`
tipados. Inventar una raíz donde no la hay cambiaría la vtable, el descriptor y la cadena de
constructores de cada clase de cada proyecto, por una facilidad que ese proyecto no puede usar.

Es también lo que permitió que esto aterrizara **sin perturbar ninguno** de los 236 tests ni de los
128 proyectos de ejemplo previos, incluidos los 73 que sí referencian la biblioteca estándar y por
lo tanto sí recibieron la raíz.

---

## 6. Qué se tocó

| Archivo | Qué |
|---|---|
| `lib/NativeSharp.Core/Object.ns` | **nuevo.** La raíz y `RuntimeHelpers` |
| `src/Frontend/ProjectInheritanceModel.cpp` | inyección de la base implícita cuando la raíz está a la vista |
| `src/Frontend/ProjectNameLookup.cpp` | `object` como último recurso hacia `NativeSharp.Core.Object` |
| `src/Parser/Parser.cpp` | `object` como nombre de tipo conocido, para que `object x = ...;` parsee como declaración |
| `include/.../ProjectMethodBodyModel.h`, `src/Frontend/ProjectMethodBodyModel.cpp` | las dos operaciones núcleo, la fachada de los tres miembros de struct en el acceso a miembro, su reconocimiento y chequeo en la llamada, y el rechazo de uniones |
| `include/.../ProjectOopHirModel.h`, `src/Frontend/ProjectOopHirModel.cpp` | `AggregateEquals` / `AggregateHash`, `shortTypeName`, y el aplanado a hojas escalares |
| `include/.../ProjectOopMirModel.h`, `src/Frontend/ProjectOopMirModel.cpp` | los nodos correspondientes, el nombre del tipo en la tabla de despacho, y el ancho en bytes de cada hoja |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | séptimo campo del descriptor, la constante de nombre por tipo, `lowerObjectTypeName`, `lowerObjectReferenceHash`, `lowerAggregateEquals`, `lowerAggregateHash` |
| `tests/unit/Stage56C8TypeRootTests.cpp` | 55 checks |
| `tests/interop_contract/c8_type_root/`, `examples/project_stage56_c8/` | caso contractual y ejemplo empaquetado |

---

## 7. Cómo se verificó

- **`ctest` 237/237**, con `-DNSC_WARNINGS_AS_ERRORS=ON`.
- **Forma del IR**: la constante de nombre por tipo con sus bytes exactos, el descriptor de siete
  campos, la lectura de `descriptor.type_name` en el cuerpo de la raíz, y — para la mitad de
  structs — la **ausencia** de vtable y de descriptor.
- **Código máquina**: la mitad de structs se pliega en `-O2` a `mov eax, 42; ret`. Para plegarlo el
  optimizador tuvo que demostrar cada comparación y cada relación de hash por separado; es una
  verificación más fuerte que ejecutarlo.
- **La raíz opcional**: un proyecto sin ella compila y su IR no menciona `Object` por ningún lado.
- **Ejecución real** de la mitad de clases: pendiente de Wine o Windows, como el resto de la matriz
  de ejecución de C5. Donde no hay Wine, la mitad de ejecución degrada a un *skip* explícito, nunca
  a un check verde que no verificó nada.
