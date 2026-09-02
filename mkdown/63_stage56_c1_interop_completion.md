# Stage 56 / C1 — Interop completion

**Fase:** C1 del roadmap del compilador (`01_Compiler_Roadmap.md`, secciones 3.1, 3.2, 3.6).
**Estado de partida:** Stage 40.2 Aggregate ABI + Stage 55.x Interop Foundation.
**Resultado:** las tres primitivas que Stage 55.x dejó nombradas como faltantes están cerradas,
verificadas por forma de IR y por enlace real, con `ctest` **233/233** y build limpio con
`-DNSC_WARNINGS_AS_ERRORS=ON`.

El objetivo de esta fase, en los términos del propio roadmap: cerrar lo que le falta al compilador
para que **una library C desconocida pueda exigir functions, callbacks y function-pointer returns
sin que el compiler contenga un solo caso especial por nombre**. Sigue siendo cierto que ningún
archivo del compilador nombra a `wglGetProcAddress`, a `user32.dll` ni a ninguna otra API: lo único
que este cambio agrega es forma, no catálogo.

---

## 1. Function-pointer round trip (roadmap 3.1) — CERRADO

### Lo que estaba roto, medido

Antes de esta ronda, este programa —cuatro líneas, y el corazón de cualquier API de proc-address—
no compilaba:

```csharp
Adder f = AddImpl;
nint raw = (nint)f;
Adder back = (Adder)raw;
return back(20, 22);
```

El error era `nsc: error: Stage 34 local initialization failed`. Sin línea, sin tipo, sin nada. La
causa raíz: **nada en el frontend clasificaba un cast que tocara un tipo delegate**. Un tipo
delegate no es ni clase ni interfaz, así que la rama de conversiones de referencia de
`CastExpression` ni siquiera lo miraba; el cast pasaba de largo con el `typeKey` destino puesto y
moría mucho después, en `ProjectOopLlvmLowerer::convert`, sin contexto de origen.

Las cuatro formas del round trip fallaban idéntico: `(nint)fp`, `(Adder)raw`, `(void*)fp` y
`(Other)fp`.

### Lo que hay ahora

El round trip es una conversión explícita de primera clase, en ambas direcciones, entre los tres
lados de un mismo valor de 64 bits:

```text
extern C delegate  ←→  nint / nuint
extern C delegate  ←→  T* / void*
extern C delegate  ←→  extern C delegate     (reinterpretación entre firmas foráneas)
```

En LLVM eso es exactamente una instrucción por dirección (`ptrtoint` / `inttoptr`) y **cero**
entre dos extremos con forma de puntero, porque con punteros opacos eso es un no-op. El IR emitido
para el programa de arriba contiene un `ptrtoint ptr` y un `inttoptr i64` y **ninguna** referencia
a `delegate.code`: la celda de delegate ordinario no aparece en ningún punto del camino, que es
justamente el punto.

### Por qué la reinterpretación es explícita y nunca implícita

Dos `extern C delegate` con la misma forma siguen siendo dos tipos. En Win64 hay **exactamente una**
convención de llamada nativa para C, así que la convención queda fijada por `extern C` mismo y es
parte del tipo; lo que distingue a dos tipos de function pointer es su **firma**. Reinterpretar una
firma como otra es precisamente el acto sin verificación que un proc-address le impone a quien lo
llama: la dirección llega sin tipo y quien la usa *afirma* una firma. Exigir el cast mantiene esa
afirmación visible en el código fuente. Asignar sin cast sigue siendo un error de tipos.

### Retorno de function pointers

`extern C delegate` como **tipo de retorno** estaba prohibido de plano —`Stage 47 C ABI does not
return delegate values`— tanto para imports como para exports. Eso obligaba a que todo binding
declarara `nint` y hiciera un cast a ciegas en cada call site. La prohibición se estrechó a lo que
realmente la justifica:

- un delegate **ordinario** es una celda de 16 bytes (code + receiver) y no tiene forma C: sigue
  rechazado, ahora con el arreglo en el mensaje;
- un `extern C delegate` **es** un puntero a función crudo (desde Stage 55.x, crear uno desde un
  método estático devuelve la dirección de la función misma): es tan retornable como `void*`.

Con eso, la declaración contractual se puede escribir con su firma real:

```csharp
extern C delegate int GlSlot(int slot);

public class Gl {
    extern C from "opengl32.dll" {
        public GlSlot wglGetProcAddress(byte* name);
    }
}
```

y su prototipo LLVM es `declare ptr @wglGetProcAddress(ptr)` — sin wrapper, sin celda, sin caso
especial.

### Los negativos que antes eran silencio

Dos casts que antes llegaban al backend y morían sin diagnóstico ahora se rechazan en el frontend
con el motivo y el arreglo:

- `(nint)delegateOrdinario` → *"an ordinary delegate is a code+receiver cell, not an address;
  declare the type 'extern C delegate' to convert…"*. Este es literalmente el miscompile que Stage
  55.x midió del lado de la creación (una celda entregada a C, invocarla ejecutaba sus bytes,
  exit 5 para una respuesta de 42) — con el cast permitido, volvía por la otra puerta.
- `(Fn)referenciaDeClase` → *"an extern C function pointer converts only to and from another extern
  C function pointer, a raw pointer or nint/nuint"*.

---

## 2. Import identity (roadmap 3.2) — CERRADO

### Lo que estaba roto, medido

```csharp
public class A { extern C from "a.dll" { public int Init(); } }
public class B { extern C from "b.dll" { public int Init(); } }
```

```text
lld-link: error: undefined symbol: Init.1
```

El compilador identificaba un import **por el nombre del símbolo, y nada más**. Dos declaraciones
pidiendo llamarse `Init` hacían que LLVM renombrara la segunda a `Init.1` por su cuenta —sin avisar
a nadie— y el enlace terminaba pidiendo un símbolo que no existe en ninguna parte.

### La identidad, ahora

La identidad de un import es el **triple**:

```text
(library, external symbol, signature)
```

y se hace valer en tres mitades:

1. **Mismo (library, symbol), firma distinta → conflicto duro.** Dos firmas para un mismo entry
   point foráneo no pueden ser ambas correctas, y elegir una en silencio miscompilaría todos los
   call sites de la otra. Diagnóstico: *"conflicting declarations of imported C symbol 'a.dll!Ping':
   an import's identity is (library, symbol, signature)"*, con nota apuntando a la declaración
   previa.
2. **Mismo triple declarado dos veces → UN import, no un conflicto.** Dos clases pueden envolver
   cada una `kernel32!GetCurrentProcessId`; los nombres de librería se comparan sin distinguir
   mayúsculas, igual que ya los fusiona el planificador de import libraries. Una identidad, una
   declaración LLVM. (Este caso lo encontró la propia suite de esta ronda: sin la deduplicación, el
   segundo miembro producía `GetCurrentProcessId.1` y el mismo fallo de enlace.)
3. **Mismo símbolo, librerías distintas → legal**, y se resuelve por *runtime binding*.

### Por qué el tercer caso no puede resolverse estáticamente (y qué se hace en cambio)

Una import table de PE tiene **exactamente un slot por nombre de símbolo**, y una import library
nombra a su símbolo según el símbolo C. Dos librerías reclamando un mismo nombre no pueden
resolverse ambas de forma estática: serían el mismo símbolo del linker. Se midieron y descartaron
las salidas de linker que parecían prometer lo contrario:

- un `.def` con `alias == real` en `lld-link` genera un **weak alias**, no un `ExportAs`: el enlace
  sigue exigiendo el símbolo original y el conflicto vuelve;
- `EXPORTAS` como palabra clave no lo interpreta el parser de `.def` de esta versión (queda como
  un símbolo más llamado `EXPORTAS`);
- un short import record escrito a mano con `NameType = IMPORT_NAME_EXPORTAS` **enlaza**, pero el
  PE resultante importa por ordinal 0 con nombre vacío — verificado con `llvm-readobj --coff-imports`
  contra un control con `NameType = IMPORT_NAME`, que sí imprime `Symbol: Init (0)`.

Así que el import ambiguo se liga como liga Windows un nombre ambiguo: **por handle de librería, en
tiempo de ejecución**. El backend le da cuerpo real a esa función:

```llvm
@__nsc_import_a_dll__Init__address = global ptr null

define i32 @__nsc_import_a_dll__Init() {
import.entry:                       ; ¿ya ligado?
import.bind:                        ; LoadLibraryA("a.dll") + GetProcAddress(h, "Init")
import.call:                        ; llamada indirecta con el MISMO prototipo
}
```

Tres propiedades que valen la pena decir explícitas:

- **Sólo lo pagan los imports que colisionan.** Todo import cuyo nombre reclama una sola librería
  —que son todos, en un proyecto normal— sigue por el camino estático certificado: resuelto por la
  import library, visible en el import directory del PE. Hay un test de regresión dedicado a eso
  (`SingleClaimantStaysStaticallyBound`: el IR no contiene `LoadLibraryA` ni `__nsc_import_`, y el
  PE sigue importando `user32.dll` por nombre).
- **`LoadLibraryA`/`GetProcAddress` entran por la misma puerta** que los imports del proyecto, así
  que la tabla única del runtime (`WindowsRuntimeImports`, de Stage 55.x) los fusiona y deduplica.
  Un proyecto sin nombres ambiguos no los importa nunca.
- **El prototipo se reenvía verbatim**, y eso es lo que hace que sea ABI-correcto gratis: desde
  Stage 40.2 ese prototipo YA es el ABI C de Win64 para todo tipo C-expresable (retornos `sret` y
  agregados indirectos incluidos), así que reenviar cada parámetro sin tocarlo reenvía el ABI sin
  tocarlo. El atributo `sret` se replica también en el call site.

Si `LoadLibrary`/`GetProcAddress` devuelven null en la primera llamada, se trapea
(`LlvmTrapReason::ImportBindingFailure`, exit **140**): la firma C de la función importada no tiene
canal de error por donde reportarlo, y una llamada indirecta a null sería un access violation sin
atribución.

### El caso que el triple no puede rescatar

Un `extern C { ... }` sin `from` resuelve **por nombre** desde un archive estático, y ahí no hay
nada que renombrar. Si su nombre lo reclama además una DLL, se diagnostica en vez de dejarlo fallar
en el linker: *"a statically resolved symbol cannot share its name"*.

---

## 3. Callback exception boundary (roadmap 3.6) — CERRADO

### El contrato

> Una excepción NativeSharp no puede salir de código NativeSharp por un borde C.

No es una regla de estilo. Un `throw` de NativeSharp es un `RaiseException` de SEH Win64; dejarlo
seguir más allá del borde desenrolla **frames foráneos** —un message pump de Win32, un driver de
OpenGL, el event loop de SDL— cuyo contrato de limpieza este compilador desconoce por completo. Lo
que el código foráneo haga con un exception code extraño no es un comportamiento que NativeSharp
pueda definir, así que el borde se niega a producirlo.

### Dónde está el borde

En las dos —y sólo dos— puertas por las que entra código foráneo:

1. una función cuya dirección se entregó como `extern C delegate` (un callback);
2. un entry point `export extern C`.

Las dos reciben **el mismo guard**, porque las dos son "C nos llamó".

### Qué es el guard

Un thunk con la firma exacta del objetivo —que desde Stage 40.2 ya *es* el ABI C de Win64— que
invoca al objetivo dentro de un solo `catchpad` filtrado por `__nativesharp_eh_filter`, y trapea
ahí (`LlvmTrapReason::CallbackExceptionEscape`, exit **141**).

```llvm
define i32 @...AddImpl__c_boundary(i32 %0, i32 %1) personality ptr @__C_specific_handler {
boundary.entry:
  %r = invoke i32 @...AddImpl(i32 %0, i32 %1) to label %boundary.return unwind label %boundary.unwind
boundary.unwind:
  %cs = catchswitch within none [label %boundary.catchpad] unwind to caller
boundary.catchpad:
  %c = catchpad within %cs [ptr @__nativesharp_eh_filter]
  call void @__nativesharp_trap(i32 13) [ "funclet"(token %c) ]
  unreachable
}
```

Tres consecuencias que conviene dejar dichas:

- **Una excepción SEH FORÁNEA no se atrapa.** El filtro sólo acepta el exception code propio de
  este compilador; un access violation lanzado dentro de un callback lo declina, el `catchswitch`
  hace `unwind to caller` y sigue propagándose a lo que sea que el host haya instalado. El borde
  **contiene las excepciones de este compilador, no se traga las del sistema operativo.**
- **No cuesta nada en el camino feliz:** un `invoke` sin cleanup entre él y el `ret`, más la arista
  de unwind en la tabla de excepciones de la propia función.
- **El thunk se cachea por objetivo**, y tiene que ser así: un host al que se le entrega el mismo
  callback dos veces tiene que ver **un solo puntero**, o se rompe cualquier chequeo de identidad
  que haga (dar de baja un handler, comparar dos `WndProc`). Un objetivo, una definición de thunk
  en el módulo — verificado contando ocurrencias en el IR.

Esto no contradice la nota de Stage 55.x ("el arreglo no necesita thunk"): aquella era sobre
**corrección de ABI**, y sigue vigente —la firma interna ya es la firma C, y el thunk la conserva
byte por byte—. Lo que el thunk agrega es el borde, no una traducción.

---

## 4. Verificación

| Qué | Resultado |
|---|---|
| Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON` | limpio, 0 warnings, 0 errores |
| `ctest` completo | **233/233** (232 previas + `nsc.stage56.c1-interop-completion`) |
| Suite nueva `Stage56C1InteropCompletionTests` | **92 checks** |
| Fixtures `tests/interop_contract/` (5) | compilan y verifican desde el árbol de fuentes |
| Ejemplo empaquetado `examples/project_stage56_c1` | compila a `-O0` y a `-O2` |

**Limitación de entorno, dicha explícitamente:** este sandbox **no tiene Wine** (a diferencia de
todas las rondas anteriores de este proyecto). Por instrucción directa de Alberto no se invirtió
tiempo en instalarlo. Consecuencia concreta: la mitad de **ejecución real** de la política de
testing (`IR shape + real executable + O0 + O2`) queda como *skip explícito* en esta ronda, nunca
como rojo falso ni como pase silencioso — cada escenario reporta literalmente `"Wine unavailable --
execution skipped"`. Lo que **sí** se verificó de punta a punta aquí:

- forma de IR, con asserts positivos y negativos (lo que el módulo debe contener y lo que **no**);
- compilación a `-O0` y `-O2` de cada escenario;
- **enlace real** (`lld-link`) de los casos que antes fallaban justamente en el enlace — el caso de
  las dos librerías con el mismo símbolo produce hoy un `.exe` real;
- que el PE sigue importando `user32.dll` por nombre en el camino estático.

Queda del lado de Alberto, en Windows real, correr:

```
tests\interop_contract\c1_03_function_pointer_parameter   -> 42
tests\interop_contract\c1_04_function_pointer_return      -> 42
tests\interop_contract\c1_05_raw_pointer_cast             -> 42
tests\interop_contract\c1_14_callback_exception_boundary  -> 141   (NO 0)
examples\project_stage56_c1                               -> 0
```

`c1_13_two_libraries_same_symbol` compila y enlaza a propósito contra `a.dll`/`b.dll` que no
existen: el caso es que dos identidades que comparten nombre puedan coexistir, y el binding es
perezoso, así que sólo fallaría al llamar.

---

## 5. Qué desbloquea, y qué sigue

```text
        Interop Foundation ✅              C1 Interop completion ✅
     imports/libs/callbacks/punteros    proc-address round trip
                  │                     identidad de import
                  │                     borde de excepciones
                  └───────────────┬─────────────────┘
                                  │
                    B1 OpenGL: ABIERTO
        (wglGetProcAddress declarable con su firma real;
         GL_* cargables como function pointers tipados)
```

`wglGetProcAddress` / `glfwGetProcAddress` / `SDL_GL_GetProcAddress` / `alGetProcAddress` son hoy
declarables con su tipo real y usables sin cast a ciegas en cada call site: es la primitiva que la
subinstancia B1 (OpenGL) estaba esperando.

**Pendientes descendientes** (ninguno bloquea empezar bindings de OpenGL):

1. **C2 — C aggregate completeness**: arrays embebidos en fields (`float[4]`, `char[256]`) y
   `union`/layout explícito. Son las filas 11 y 12 de la matriz contractual y lo que piden `INPUT`
   de Win32 y Vulkan.
2. **C3 — aliases FFI** (`using DWORD = uint;`): ergonomía de bindgen.
3. **C4 — `ref`/`out`/`in`** como azúcar sobre los punteros ya funcionales.
4. Certificación cruzada de esta ronda bajo un host C real (clang/MSVC) recibiendo un function
   pointer de vuelta — la suite de Stage 55.x ya tiene el andamiaje; falta la fila de C1 en esa
   matriz, y necesita Wine o Windows real.
