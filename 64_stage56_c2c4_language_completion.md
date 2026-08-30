# Stage 56 / C2–C3–C4 — C aggregate completeness, aliases FFI, referencias del lenguaje

**Fases:** C2, C3 y C4 del roadmap del compilador (`01_Compiler_Roadmap.md`, secciones 3.3, 3.4,
3.5 y 3.7), más la parte de C5 que se puede certificar sin Wine.
**Estado de partida:** Stage 56 / C1 Interop completion (ver `63_stage56_c1_interop_completion.md`).
**Resultado:** `ctest` **234/234**, build limpio con `-DNSC_WARNINGS_AS_ERRORS=ON`, y la matriz
contractual de interop de 15 casos **completa**.

Donde C1 hizo expresable una **función** C, estas tres fases hacen expresable una **declaración**
C. Con esto se cierra el criterio de Definition of Done que el propio roadmap fija para esta
instancia:

> una library C desconocida puede exigir functions, structs, fixed arrays, unions, typedefs,
> callbacks, function-pointer returns y external libraries **sin añadir casos especiales por
> nombre dentro del compiler**.

Las ocho cosas de esa lista están hoy expresables. Ningún archivo del compilador nombra a una API.

---

## 1. C2.1 — Arrays embebidos en campos (roadmap 3.3)

### Lo que estaba roto, medido

El tipo **sí** parseaba y el layout **sí** era correcto —`float[4]` daba `offset=0 size=16
align=4`— pero indexarlo se rechazaba:

```text
error: index expression requires an indexer reference or a pointer with one integral index
```

Y la mitad "or a pointer" de ese mensaje también era falsa: `p[0]` sobre un `int*` **pasaba** ese
chequeo y después moría en HIR con *"Stage 38 OOP lowering requires class/interface index
expressions to resolve to an indexer accessor"*. **Ninguna de las dos formas tenía lowering.**

La causa raíz estaba antes que todo eso, en la clave de tipo: un `float[4]` se codificaba como
`builtin:float[38:4:3:4]` —una huella opaca de la **sintaxis** del contador— que ningún consumidor
posterior podía interpretar. No se podía sacar de ahí ni el tipo del elemento ni cuántos hay.

### Lo que hay ahora

La clave canónica lleva el **contador evaluado**: `builtin:float[4]`. Con eso, el elemento y su
tamaño se leen de la clave, y el acceso baja a una sola instrucción:

```llvm
%field.addr = getelementptr i8, ptr %local0.addr, i64 0     ; el campo
%raw.element.addr = getelementptr i8, ptr %field.addr, i64 12   ; + index * elementSize
store float 4.100000e+01, ptr %raw.element.addr, align 4
```

Un `T[N]` mapea a `[N x T]` en LLVM, así que un **local** de tipo array embebido es un solo
`alloca` de todo el objeto, y un **campo** ocupa sus bytes reales dentro del struct.

**Una operación cubre las dos formas** —`campoArray[i]` y `punteroCrudo[i]`— porque la aritmética
es idéntica (`base + index * elementSize`) y ninguna de las dos tiene cabecera de longitud contra
la cual verificar. Es el **tier crudo**: sin bounds check, sin null check, semántica C por diseño,
el mismo tier donde ya viven `&x` y `*p`. El array **manejado** (`T[]`, heap, prefijado con su
longitud, con bounds check) queda exactamente como estaba: son dos cosas distintas y siguen
siéndolo.

### Lo que se rechaza, y por qué

Un array embebido **no puede cruzar una llamada por valor**. C dice lo mismo degradando un
parámetro array a puntero y negándose a retornar uno; NativeSharp lo dice en voz alta en vez de
copiar 16 bytes en silencio:

```text
error: a fixed array cannot be passed by value; declare the parameter as a pointer to its element type
error: a fixed array cannot be returned by value; return a pointer to its element type
```

Campo, local, y lectura/escritura de un elemento es justo donde un array embebido sí pertenece, y
las tres funcionan.

---

## 2. C2.2 — `union` y layout explícito (roadmap 3.4)

Las cuatro cosas que dice un registro C hecho a mano, con la grafía de NativeSharp:

```csharp
public union Payload { public int asInt; public float asFloat; public byte[4] asBytes; }
public pack(1) struct WireRecord { public byte kind; public int length; }
public align(16) struct Vector4Aligned { public float[4] values; }
public struct Overlapping { public int first; public at(0) int alias; public at(8) long tail; }
```

| Grafía | Equivalente C | Qué cambia |
|---|---|---|
| `union` | `union` | todos los campos arrancan en el mismo lugar; el tamaño es el del mayor |
| `pack(N)` | `#pragma pack(N)` | cota superior a la alineación de cada campo |
| `align(N)` | `__declspec(align(N))` / `alignas(N)` | cota inferior a la alineación del agregado |
| `at(N)` | offset explícito / `[FieldOffset(N)]` | el campo nombra su propio byte de inicio; **solaparse es la feature**, no un error |

Los cuatro son **palabras clave contextuales**, igual que `partial`: siguen siendo identificadores
ordinarios en cualquier otro lugar, así que ningún programa que ya usara `pack`, `align`, `at` o
`union` como nombre deja de compilar.

### La decisión de diseño que mantiene esto chico

Una `union` **es** un struct en todo lo que este compilador ya modela: mismos miembros, misma
semántica por valor, misma clasificación de ABI C, mismo carrier `[N x i8]` en LLVM, misma
destrucción. Lo único que difiere es **dónde caen sus campos**. Así que en vez de un
`NamedTypeDeclarationKind` paralelo rippleando por cada `switch` del compilador, la diferencia se
registra como un marcador de layout y se honra en **exactamente un lugar**:
`ProjectObjectLayoutModel`. El resto del compilador nunca se entera.

Lo mismo vale para la regla de colocación: una sola pasada en `TargetDataLayout` cubre las cuatro
formas, porque difieren nada más en cómo se elige el offset de cada campo y cómo avanza el tamaño.

### Verificado contra un toolchain foráneo, no contra sí mismo

```text
struct type:Val      size=4  align=4    i@0   f@0   bytes@0
struct type:Packed   size=5  align=1    tag@0 value@1
struct type:Aligned  size=16 align=16   x@0
struct type:Overlap  size=16 align=8    a@0   b@0   c@8
```

Cada uno de esos números es una afirmación de layout, y **clang la verifica**. El generador de
headers C emite hoy `union`, `#pragma pack(push, N)` / `pop`, `NS_ALIGNAS(N)` y `float rgba[4]`
con la extensión del lado correcto del nombre — junto con los `NS_STATIC_ASSERT` de tamaño,
alineación y offset por campo que ya emitía. La suite compila ese header con **clang apuntando al
ABI de MSVC, con `-Werror`**. Si NativeSharp y clang discreparan sobre un array embebido, una
union, un pack o una alineación, eso es un build roto, no un test verde con un número equivocado
adentro.

**Offsets explícitos en el borde C**: un struct C plano no puede expresar campos solapados, así que
generar el header se **rechaza** con la razón y la alternativa (*"expose it as a union across the C
boundary"*) en vez de emitir un header cuyas propias aserciones fallarían en el host.

---

## 3. C3 — Aliases FFI (roadmap 3.5)

### Lo que estaba roto, medido

`using X = Y;` existía, pero el destino sólo podía ser un **nombre calificado**. Es decir: las tres
formas que un header C realmente usa eran inescribibles.

```text
using DWORD = uint;          -> error: expected declaration name, found ';'
using HANDLE = void*;        -> idem
using Rgba = float[4];       -> idem
```

### Lo que hay ahora

El destino es un **tipo completo**, parseado por la misma rutina que todo otro tipo del lenguaje.
Con eso quedan cubiertas las cuatro clases de typedef de un header C, más el encadenamiento:

```csharp
using DWORD   = uint;            // builtin
using HANDLE  = void*;           // puntero
using Rgba    = float[4];        // array embebido
using PDWORD  = DWORD*;          // sobre otro alias
using LPDWORD = PDWORD;          // cadena
using Callback = WndProcLike;    // function pointer (delegate extern C)
```

### Un alias es un typedef, no un tipo distinto

La expansión ocurre **al ligar nombres**, dentro del propio nodo de sintaxis del tipo. Después de
eso el AST literalmente dice `uint` donde decía `DWORD`, y **nada aguas abajo se entera de que los
aliases existen**: ni las claves canónicas de tipo, ni el layout, ni las reglas de ABI C, ni HIR,
ni el backend. Eso es exactamente lo que significa un typedef, y es la razón por la que esta fase
no tocó ninguna de esas capas.

Dos detalles que sí importan:

- **Los modificadores del sitio de uso quedan por fuera**: `PDWORD*` es `uint**`, no `uint*` dos
  veces.
- **El nombre escrito se preserva**: la ligadura sigue diciendo `PointAlias`, no el tipo en que
  resultó, para que los diagnósticos hablen del código que la persona escribió. (Esto lo atrapó la
  suite existente `nsc.project-name-lookup` en el primer intento, no una revisión.)
- Los **alias de namespace** (`using IO = NativeSharp.Core.IO;`) siguen funcionando sin cambios: el
  name lookup separa las dos formas por si el destino resuelve como namespace, no por sintaxis.

Una cadena cíclica se corta con un tope de expansión y se reporta (*"expands cyclically"*); un
destino que no nombra nada visible sigue dando el diagnóstico que siempre dio.

---

## 4. C4 — `ref` / `out` / `in` (roadmap 3.7)

### La decisión que hace esto pequeño y seguro

Las tres formas por referencia son **una sola cosa a nivel máquina** —un puntero— y eso es
deliberado: C1 ya dejó `&x`, `*p` y los parámetros puntero funcionando y certificados de punta a
punta, así que `ref`/`out`/`in` son **reglas encima de maquinaria ya certificada**, no una segunda
convención de llamada. En el IR no aparece nada nuevo:

```llvm
define void @...Bump__ref__builtin_int_(ptr %p0) {
  %param.load = load ptr, ptr %p0.addr
  %pointer.load = load i32, ptr %param.load
  %add = add i32 %pointer.load, 1
  store i32 %add, ptr %param.load
  ret void
}
```

Lo que sí es nuevo, y es la diferencia ergonómica entera con un `int*` pelado, es el
**dereferenciado implícito**: el símbolo tiene tipo `T*` (que es lo que el llamado físicamente
recibe), pero la persona escribió `ref T x` y **cada uso de `x` significa la T del otro lado del
puntero** — leerla, asignarla, pasarla adelante.

### Las reglas, que es lo único que distingue a las tres

| | El argumento | El llamado |
|---|---|---|
| `ref` | ya tiene un valor | puede leer y escribir |
| `out` | **no** necesita estar inicializado | **debe** asignarlo |
| `in` | ya tiene un valor | sólo puede leer |

- **La palabra clave es obligatoria en el sitio de la llamada**, igual que en C#, y por la misma
  razón: quien lee la llamada tiene que poder ver que el argumento puede cambiar sin ir a leer la
  firma. Se exige donde el parámetro la declara y se rechaza donde no.
- **`in` es de sólo lectura**: existe para evitar la copia, no para abrir un canal de vuelta.
- **La ref-idad es parte de la firma**: `F(int)` y `F(ref int)` son miembros distintos. Bajan a la
  misma firma de máquina, así que tanto la clave de firma como el **nombre mangleado** tienen que
  separarlos, o los dos se volverían silenciosamente una sola función.
- **Diagnósticos precisos**: cuando exactamente una sobrecarga tiene la aridad correcta y lo único
  mal es la palabra clave faltante (o de más), el mensaje lo dice —*"argument 1 of 'Bump' must be
  passed with 'ref'"*— en vez de mostrar el desajuste de tipos `int` vs `int*`, que es un artefacto
  de la representación y no algo que la persona escribió.

### `out`: qué se verifica exactamente, y qué no

Un `out` es una **obligación**, no un permiso: el llamador puede pasar una variable sin inicializar
precisamente porque el llamado promete llenarla. La verificación de esa promesa acepta cualquier
escritura **a través** del parámetro —`rect = otro;`, `rect.left = 0;`, `valores[0] = 1;`,
`*p = v;`— porque la forma GetMessage/GetClientRect (el llamado llenando campo por campo un
registro que el llamador posee) es justamente para lo que existe `out` sobre un struct.

**Precisión sobre el alcance, porque la diferencia importa**: se verifica que el cuerpo lo asigne
**al menos una vez**. **No** se prueba que lo asigne en todos los caminos de retorno — eso es
definite assignment sensible a caminos, necesita el CFG, y el modelo de cuerpos corre antes que
él. Así que `out int x` con `if (c) { x = 1; }` y sin `else` se acepta hoy y un análisis completo
lo rechazaría. Lo que el chequeo **sí** atrapa es el error que la gente comete de verdad: declarar
un `out` y no escribirlo nunca, lo que le entregaría al llamador los bytes que hubiera en el slot
de pila. Endurecerlo a cobertura de caminos queda como pendiente descendiente, y está nombrado
como tal en vez de quedar implícito.

---

## 5. C5 — Endurecimiento de la certificación (lo que se pudo hacer sin Wine)

El roadmap pide para C5 una matriz de ABI foráneo. La parte que **no** necesita ejecución está
hecha y es un chequeo duro:

**Matriz de tamaños de agregado, contra clang con ABI de MSVC y `-Werror`:**

```text
1, 2, 3, 4, 8, 12, 16, 24, 64, 80 bytes
```

Cada struct se exporta, el header generado declara cada uno con sus `NS_STATIC_ASSERT` de tamaño,
alineación y offset por campo, y clang compila. Los tamaños 3, 12, 24 y 80 están en la lista a
propósito: no son ni ancho de registro ni números redondos, y son exactamente donde se esconde un
bug de layout.

**Lo que sigue pendiente de C5**, y necesita Wine o Windows real: la matriz de **ejecución**
cruzada (host MSVC/clang llamando y siendo llamado, en las dos direcciones, para cada tamaño),
sanitizers, e inspección de PE más allá de la que ya hace `PeExecutableVerifier`.

---

## 6. Verificación

| Qué | Resultado |
|---|---|
| Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON` | limpio, 0 warnings, 0 errores |
| `ctest` completo | **234/234** |
| Suite `Stage56C1InteropCompletionTests` | 92 checks |
| Suite `Stage56C2C4LanguageCompletionTests` | **81 checks** |
| Header C generado, compilado por clang (ABI MSVC, `-Werror`) | pasa, aserciones incluidas |
| Matriz de tamaños 1…80 bytes contra clang | pasa |
| Fixtures `tests/interop_contract/` (8) | compilan y verifican desde el árbol de fuentes |
| Ejemplos empaquetados (`project_stage56_c1`, `project_stage56_c2c4`) | compilan a `-O0` y `-O2` |

**Limitación de entorno, dicha explícitamente**: este sandbox **no tiene Wine**. Por instrucción
directa de Alberto no se invirtió tiempo en instalarlo. La mitad de **ejecución real** de la
política de testing queda como *skip explícito* —cada escenario reporta literalmente `"Wine
unavailable -- execution skipped"`, nunca un rojo falso ni un pase silencioso—. Lo que compensa
esa ausencia en esta ronda, y no es poco: **la certificación de layout ya no depende de ejecutar
nada**, porque un segundo compilador (clang, ABI de MSVC) verifica cada número.

Para correr en Windows real, del lado de Alberto:

```
tests\interop_contract\c1_03_function_pointer_parameter   -> 42
tests\interop_contract\c1_04_function_pointer_return      -> 42
tests\interop_contract\c1_05_raw_pointer_cast             -> 42
tests\interop_contract\c2_11_fixed_field_array            -> 42
tests\interop_contract\c2_12_union                        -> 42
tests\interop_contract\c4_ref_out_in                      -> 42
tests\interop_contract\c1_14_callback_exception_boundary  -> 141   (NO 0)
examples\project_stage56_c1                               -> 0
examples\project_stage56_c2c4                             -> 0
```

---

## 7. Estado del roadmap del compilador

```text
C1 Interop completion        ✅   function-pointer round trip, import identity, borde de excepciones
C2 C aggregate completeness  ✅   arrays embebidos, union, pack/align/offsets explícitos
C3 Alias/FFI ergonomics      ✅   typedefs de builtin/puntero/array/function-pointer, cadenas
C4 Language references       ✅   ref/out/in (definite assignment por caminos: pendiente)
C5 Certification hardening   ◑    matriz de layout contra clang ✅ ; matriz de EJECUCIÓN: necesita Wine/Windows
```

**Definition of Done de esta instancia — alcanzada.** Una library C desconocida puede hoy exigir
functions, structs, fixed arrays, unions, typedefs, callbacks, function-pointer returns y external
libraries sin un solo caso especial por nombre dentro del compilador. Las subinstancias de bindings
(Win32, OpenGL, GLFW, SDL, OpenAL, Vulkan) pueden avanzar como proyectos NativeSharp ordinarios.

**Pendientes descendientes, ninguno bloqueante:**

1. Definite assignment sensible a caminos para `out` (necesita el CFG).
2. Matriz de ejecución cruzada de C5 (necesita Wine o Windows real).
3. `union` anónima dentro de un struct — la fase 3 de la sección 3.4 del roadmap, que se dejó
   fuera a propósito: una union **nombrada** como campo ya expresa `INPUT` de Win32, y la anónima
   es azúcar sobre eso.
4. Alias genéricos (`using Pair<T> = ...`), marcados en el roadmap como "después".
