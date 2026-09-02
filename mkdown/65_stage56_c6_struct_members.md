# Stage 56 / C6 — Miembros de struct: constructores, propiedades, indexers, operadores

**Origen:** una pregunta directa de Alberto, portando la matemática de Saru: *"los structs de
NativeSharp no admiten constructores, propiedades ni operadores — la matemática de saru es structs
con las cuatro cosas. ¿Por qué son tan limitados? ¿No podría haber dos tipos de structs?"*

**Resultado:** las cuatro cosas se pueden declarar. `ctest` **235/235**, build limpio con
`-DNSC_WARNINGS_AS_ERRORS=ON`, y un `Vector3` con constructor, propiedades, indexer y seis
operadores certificado por clang como **idéntico en layout y ABI** a un struct C plano.

---

## 1. Por qué estaban limitados: la respuesta medida

El bloqueo era **un solo `if` en `Parser.cpp`**, con un mensaje que empezaba con "Stage 40" y
rechazaba de una sola vez constructores, destructores, propiedades, indexers, operadores,
conversiones, `protected` y todas las formas virtuales. Era una decisión de **alcance por etapa**,
no una ley derivada del ABI.

Antes de tocar nada, se levantó ese `if` y se midió qué pasaba con cada miembro. El resultado
separa tres categorías que estaban confundidas en una sola:

| Miembro | Con el gate levantado | Qué era realmente |
|---|---|---|
| **Propiedades** | Compila y llama al getter correctamente: `call float @...get_LengthSquared(ptr %local0.addr)` | **Sólo prohibido.** Una propiedad de struct baja a un método de instancia ordinario que toma la dirección del receptor — algo que este compilador emite desde Stage 40 |
| **Conversiones de usuario** | Compila y enlaza | **Sólo prohibido** |
| **Operadores** | `binary operator requires compatible numeric operands` | **Una línea**: `stage38OperatorCandidates` filtraba candidatos con `isClass`, así que para un struct ni siquiera se buscaba uno |
| **Indexers** | `index expression requires an indexer reference or a pointer` | El lookup del receptor de accesores sólo aceptaba tipos referencia |
| **Constructores** | `new T(args) requires T to resolve to a class type` | **Trabajo real**: `new` estaba atado a asignación en heap |
| **Destructores** | *"Compila"* — y el IR muestra que el cuerpo se emite y **nunca se llama** | **Debe seguir prohibido** |
| **`virtual`** | *"Compila"* — y se degrada a **llamada directa**, sin vtable | **Debe seguir prohibido** |

Los dos últimos son la razón por la que el `if` existía y por la que no se podía simplemente
borrar: **compilan y mienten**, que es peor que ser rechazados.

---

## 2. Por qué NO hay dos tipos de struct

La propuesta natural —un struct "plano C-compatible" y otro "rico"— resolvería un problema que no
existe, y a un costo alto: duplicaría layout, ABI, generación de headers, la matriz contractual de
interop y cada `switch` sobre tipos nombrados.

**No existe porque ninguno de estos miembros toca el layout ni el ABI.** Un constructor es una
función que recibe la dirección del almacenamiento; una propiedad es un método con otro nombre; un
operador es un método estático. Ninguno agrega un byte, mueve un campo ni introduce un puntero
oculto.

Eso no se afirma: **se verifica**. La suite de esta ronda entrega a un host C un `Vector3` con
constructor, propiedad, indexer, dos operadores y una propiedad estática, y hace que **clang,
apuntando al ABI de MSVC con `-Werror`**, compruebe:

```c
_Static_assert(sizeof(NS_Vector3) == 12, "a rich struct is still three floats");
_Static_assert(sizeof(NS_Matrix4) == 64, "a rich struct is still sixteen floats");
_Static_assert(offsetof(NS_Vector3, z) == 8, "members added no hidden field");
```

El header generado declara `float x; float y; float z;` y nada más. Si alguno de esos miembros
hubiera costado un byte o movido un campo, eso no compila.

**El eje donde un struct de interop sí difiere de uno de lenguaje es el LAYOUT**, y esa separación
ya existe desde C2.2: `union`, `pack(N)`, `align(N)` y `at(N)`. Es ortogonal a los miembros y ya
está hecha.

---

## 3. Qué se implementó

### 3.1 Constructores — `new Vec2(1, 2)`, en sitio

`new` sobre un struct significa **inicializar**, no asignar. Todo lo que el camino de clase hace
por estar en el heap está ausente, y esa ausencia es el punto:

```llvm
%struct.new = alloca i64, align 4                      ; el almacenamiento, en el bloque de entrada
call void @llvm.memset.p0.i64(ptr %struct.new, i8 0, i64 8, i1 false)
call void @__nsc_ctor_...Vec2__builtin_float__builtin_float(ptr %struct.new, float 42.0, float 1.0)
%struct.new.value = load i64, ptr %struct.new, align 4 ; el valor de la expresión
```

Sin llamada al asignador, sin rama de null, sin descriptor de clase en el offset 0 — y por lo
tanto sin nada que liberar. La resolución de **cuál** constructor corre es literalmente el mismo
código que para una clase (mismos candidatos, misma sobrecarga, mismas conversiones de argumentos),
porque nada de eso depende de dónde vive el objeto.

**Un struct siempre acepta `new T()`**, declare o no constructores propios. Aquí es donde un struct
y una clase difieren de verdad, y la razón no es comodidad: `Vector3 v;` ya produce el valor en
cero, así que negar `new Vector3()` no protege ninguna invariante — sólo obliga a escribir el mismo
valor de otra forma. C# lo resuelve igual y por lo mismo. En una clase, declarar un constructor
sigue eliminando el default, porque una referencia no tiene un estado "recién puesto en cero" al
que caer.

### 3.2 Propiedades e indexers

Una propiedad de struct **ya funcionaba**; sólo había que dejar de prohibirla. El getter y el
setter son métodos de instancia que reciben la dirección del receptor —

```llvm
call float @...get_LengthSquared(ptr %local0.addr)
call void  @...set_X__builtin_float(ptr %local0.addr, float 3.0)
```

— que es exactamente lo que un indexer sobre un valor necesita para poder **escribir** a través de
él. Los indexers requirieron que el lookup del receptor de accesores aceptara structs, no sólo
clases e interfaces: `accessorReceiverTypeOrdinalForTypeKey`, distinta de
`referenceTypeOrdinalForTypeKey` porque un struct es un valor (y esa diferencia importa para cómo
se pasa el receptor, no para si puede declarar un indexer).

Funcionan: propiedades de instancia con `get`/`set`, auto-propiedades, propiedades estáticas
(`Vector3.Zero`), e indexers de lectura y escritura.

### 3.3 Operadores y conversiones

Una línea. `stage38OperatorCandidates` filtraba con `isClass`, y esa única condición era la razón
completa de que `a + b` sobre un struct reportara *"binary operator requires compatible numeric
operands"*: el candidato nunca se buscaba. Un operador definido por el usuario es un **método
estático**, así que nada en resolverlo o llamarlo depende de si su tipo es clase o struct.

Verificado con la matemática completa: binarios, unario, mezclados con un escalar, `==`/`!=`, y
conversiones implícitas entre structs. Un `Vec2` de 8 bytes viaja **directo en un i64**, que es el
ABI Win64 correcto para ese tamaño.

### 3.4 Temporales materializados — el hallazgo que destapó el fixture

Esto no estaba en el plan, y sin ello la ronda habría quedado a medias.

Stage 40 rechazaba **cualquier** acceso a miembro sobre un temporal struct, porque un temporal no
tiene dirección y un miembro de struct necesita una. Eso costaba poco mientras una expresión struct
era casi siempre un local o un campo. **Los operadores lo cambian por completo**: `(a + b).x`,
`Cross(u, v).z`, `(-v).LengthSquared`, `Lerp(from, to, t)` son matemática ordinaria, y todas leen
un miembro de un valor que no vive en ningún lado.

Medido: el camino de llamada daba *"temporary struct values are not addressable"*; el camino de
**operador no tenía chequeo alguno** y producía IR que el verificador rechazaba —

```text
GEP base pointer is not a vector or a vector of pointers
  %field.addr25 = getelementptr i8, [12 x i8] %abi.sret.load24, i64 8
```

Ahora un temporal struct se **materializa**: se le da un slot en el bloque de entrada y el miembro
se alcanza a través de él. Lo que ya es direccionable por naturaleza (un identificador, un campo,
un elemento, un `*p`) conserva su propia dirección sin cambios — esto sólo agrega un slot donde no
había ninguno.

**Una suposición que hubo que corregir, y que vale la pena dejar escrita** porque es fácil hacerla
al revés: un agregado **indirecto no es ya un puntero** a este nivel. Una llamada que devuelve uno
escribe en su slot `sret` y después lo **carga** (`abi.sret.load`), así que lo que entrega una
expresión de ese tipo es el valor `[N x i8]` cargado, igual que un agregado directo entrega su
entero. La primera versión tomó un atajo asumiendo lo contrario y el verificador lo atrapó de
inmediato.

**La trampa que esto hereda, dicha explícitamente**: llamar a un método **mutador** sobre un
temporal ahora compila, y la mutación se descarta con la copia — exactamente como en C# y en C++.
Rechazar sólo eso exigiría saber qué métodos escriben (análisis de efectos que este compilador no
hace), y la misma regla rechazaría `(a + b).LengthSquared`, que es justamente el código que la
ronda existe para hacer escribible.

---

## 4. Qué sigue prohibido, y por qué

Ya no hay un mensaje único de "no soportado". Cada refusal explica su razón, porque en estos dos
casos el problema no es esfuerzo:

**Destructor en un struct**

> *a struct cannot declare a destructor: a value type has no owner and is copied freely, so there
> is no last copy to run it on; use a raii class, or an owned field, for a resource with a
> lifetime*

Medido con el gate levantado: el cuerpo del destructor se emite y **nunca se llama**. Un valor no
tiene dueño y se copia libremente; no hay un momento que este compilador pueda señalar como "ésta
es la última copia, córrelo ahora". RAII vive en locals `raii` de clase y en campos `owned`, que
tienen exactamente ese momento.

**`virtual` / `override` / `abstract` / `sealed` en un struct**

> *a struct method cannot be virtual, override, abstract or sealed: a value has no vtable pointer,
> and adding one would change the layout the struct crosses the C boundary with; use a class for
> polymorphism*

Medido: se degrada a **llamada directa**. Y agregarle un puntero de vtable cambiaría el layout —
que es el contrato con el que cruza a C.

**`protected` en un struct**

> *a struct member cannot be protected: structs are not inheritable, so there is no derived type
> for protected to mean anything to*

---

## 5. Qué significa para el port de Saru

La matemática de Saru se escribe **tal cual está**, sin cambios mecánicos:

```csharp
public struct Vector3
{
    public float x; public float y; public float z;

    public Vector3(float px, float py, float pz) { x = px; y = py; z = pz; }

    public float LengthSquared { get { return x * x + y * y + z * z; } }
    public float this[int i] { get { if (i == 0) { return x; } if (i == 1) { return y; } return z; } }

    public static Vector3 Zero { get { return new Vector3(0, 0, 0); } }

    public static Vector3 operator+(Vector3 l, Vector3 r) { return new Vector3(l.x + r.x, l.y + r.y, l.z + r.z); }
    public static Vector3 operator*(Vector3 l, float s) { return new Vector3(l.x * s, l.y * s, l.z * s); }
    public static Vector3 operator-(Vector3 v) { return new Vector3(-v.x, -v.y, -v.z); }
    public static bool operator==(Vector3 l, Vector3 r) { return l.x == r.x && l.y == r.y && l.z == r.z; }
    public static bool operator!=(Vector3 l, Vector3 r) { return !(l == r); }

    public static Vector3 Lerp(Vector3 a, Vector3 b, float t) { return a + (b - a) * t; }
    public static Vector3 Cross(Vector3 l, Vector3 r) { /* ... */ }
}
```

Lo que **sí** hay que ajustar al portar, y conviene saberlo de antemano:

1. **Un `Matrix4` con `float[16]`** funciona (arrays embebidos, C2.1), pero un campo array no puede
   pasarse por valor a una función: se pasa la struct que lo contiene, o un puntero al elemento.
2. **Sin destructores en structs.** En la práctica la matemática no los usa; si algún tipo de Saru
   los tiene, es un tipo que debe ser `class` con `raii`.
3. **Sin `virtual` en structs.** Igual: si hay polimorfismo, es una `class`.
4. **Sin genéricos en structs todavía** (`Vector2<T>`) — no se tocó en esta ronda.
5. Los operadores de comparación se declaran **de a pares** (`==` y `!=`), como en C#.
6. Cuidado con la trampa heredada: mutar un temporal (`GetPosition().Normalize()`) compila y
   descarta la mutación.

---

## 6. Verificación

| Qué | Resultado |
|---|---|
| Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON` | limpio |
| `ctest` completo | **235/235** |
| Suite `Stage56C6StructMembersTests` | **54 checks** |
| `Vector3`/`Matrix4` ricos vs. clang (ABI MSVC, `-Werror`) | mismo tamaño, misma alineación, mismos offsets |
| Fixture `tests/interop_contract/c6_struct_math` | compila y verifica |
| Ejemplo `examples/project_stage56_c6` | compila a `-O0` y `-O2` |

Tres tests de contrato existentes se actualizaron, y los tres cambian a propósito:
`ProjectConstructionModelTests` (ahora `new` acepta structs; sigue rechazando un enum),
`Stage40StructValueAbiTests` (el subconjunto de miembros, y el receptor temporal).

**Wine**: este sandbox no lo tiene, así que la mitad de ejecución real queda como skip explícito,
como en las rondas anteriores. Para correr en Windows real:

```
tests\interop_contract\c6_struct_math    -> 42
examples\project_stage56_c6              -> 0
```

---

## 7. Estado del roadmap

```text
C1 Interop completion        ✅
C2 C aggregate completeness  ✅
C3 Alias/FFI ergonomics      ✅
C4 Language references       ✅
C5 Certification hardening   ◑   layout contra clang ✅ ; matriz de EJECUCIÓN: necesita Wine/Windows
C6 Struct members            ✅   ctor, propiedades, indexers, operadores (fase nueva, pedida por el port de Saru)
```

**Pendientes descendientes de C6**, ninguno bloqueante para la matemática:

1. **Structs genéricos** (`Vector2<T>`) — no se tocó.
2. `readonly struct` / `in` sobre el receptor, para que el compilador pueda evitar copias.
3. Un aviso para la mutación de un temporal, si alguna vez hay análisis de efectos.
4. Inicializadores de campo en structs con constructor declarado (hoy el default los corre; con
   constructor explícito la interacción no está ejercitada).
