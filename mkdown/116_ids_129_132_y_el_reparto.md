# 116 — IDs 129 y 132, la ventana de verdad, y ctest repartido

Fecha: 2026-09-03
Ámbito: sólo el compilador (`A:\nsc-devel`).
Cierra: **ID 129** (enums por nombre), **ID 132** (sobrecargas genéricas), la ampliación de
`demo_ventana` con clase de ventana propia, teclado y ratón, y el reparto de `nsc.cc1.correctness`.

Con esto la lista de deuda anotada queda vacía.

---

## 1. ID 129 — `ToString()` de un enum da el nombre

Llevaba anotado desde el doc 92 §7, y el motivo por el que no era trabajo de `String.ns` sigue
siendo el correcto: **los nombres de los enumeradores no sobreviven a la compilación.** Un enum es
un entero con nombres en el código fuente y nada más en el binario. Los datos existen sólo en el
compilador (`isEnumConstant` + `enumConstantValue`).

### Cómo, y por qué no hizo falta ni un sitio nuevo de la cadena del doc 100

La llamada se convierte en una **cadena de condicionales**:

```
v == Rojo ? "Rojo" : (v == Verde ? "Verde" : (v == Azul ? "Azul" : "Color"))
```

Construida con nodos que ya existían (`Conditional`, `Binary`, `StringLiteral`, `IntegerConstant`).
**Sin nodo de MIR nuevo, sin bajada de LLVM nueva, sin operación del runtime, sin import.** Cuando
el valor es constante, el optimizador pliega la cadena entera y queda un literal.

De los diez sitios que el doc 100 enumera, esto tocó **tres**: el enumerador, el reconocimiento y
el lowering del HIR. Y los dos guardianes puestos en las rondas anteriores hicieron su trabajo —
añadir el enumerador dio, inmediatamente:

```
warning: enumeration value 'EnumToString' not handled in switch [-Wswitch]   (x2)
```

Los dos sitios exactos que había que tocar, nombrados por el compilador. Eso es lo que el doc 106
compró.

### El fondo de la cadena es el nombre del TIPO

`(Color)99` es legal y tiene que imprimir algo. C# devuelve el número; hacerlo aquí exigiría
convertir un entero a cadena **en ejecución**, que es una operación núcleo más y una llamada más
por cada `ToString`. Devuelve `"Color"`. Anotado con su condición de reapertura: cuando alguien
necesite ver el número de un valor fuera de rango y no le baste con saber de qué enum era.

### La limitación, y por qué se rechaza en vez de hacerse

El receptor se baja **una vez por enumerador**. Con un receptor que tenga efectos, eso los repite:
`Dame().ToString()` llamaría a `Dame()` una vez por cada nombre.

La primera versión que escribí **lo hacía**, en silencio. Compilaba. Lo vi al probar justamente ese
caso, no porque nada fallara.

Se rechaza, con la **misma regla que ya usa la asignación compuesta sobre propiedades** (ID 82) —
un local, un parámetro, `this`, una constante — y con un mensaje que dice qué hacer:

```
the compiler-provided 'ToString' on an enum needs a receiver it can read more than once;
bind this value to a local first
```

Reutilizar esa lista es mejor que inventar una segunda con los mismos casos y otro nombre.

### Fixture `c34_enum_nombre`

Valores automáticos y explícitos, un **alias** (dos enumeradores con el mismo valor: gana el
primero declarado, y que sea *estable* es lo que la fixture fija), el valor fuera de rango, y —lo
que importa— un valor que el compilador **no puede plegar**, donde la cadena se recorre de verdad.

---

## 2. ID 132 — sobrecargas genéricas por lista de parámetros

La identidad de una plantilla genérica era `nombre + aridad de TIPOS`, sin mirar los parámetros:

```
duplicate generic template identity 'NativeSharp.Core.Spans.Of``1'
```

El rodeo era renombrar, y estaba escrito **en dos sitios de la biblioteca con su motivo**:
`Spans.OfRange` en vez de `Spans.Of`, y `Algorithm.SortWith` en vez de una sobrecarga de `Sort`.

**Ahora la identidad incluye la cuenta de parámetros de valor**, y la resolución de una llamada
genérica filtra por cuántos argumentos trae, de lo más específico a lo más general.

**Lo que sigue sin poderse, y está escrito donde se arregló:** dos sobrecargas genéricas con la
**misma** cuenta de parámetros y tipos distintos. Elegir entre ellas necesita los tipos de los
argumentos, y la pasada genérica corre **antes** del modelo semántico — allí sólo hay sintaxis. El
diagnóstico lo dice ahora en el mensaje, no sólo en un comentario:

```
duplicate generic template identity '...F``1#1'; two generic methods can differ by how many
parameters they take, but not by their types alone -- rename one
```

### La primera versión rompió cuatro pruebas, y tenían razón

La metí en la identidad **siempre**, y esa clave sale del compilador por tres sitios: el volcado de
HIR, el de MIR y los **metadatos de biblioteca que lee un host**.

```
nsc.stage44.generics-monomorphization   FAIL: HIR records the canonical method specialization key
nsc.stage48_2.library-metadata          FAIL: generic method specialization key survives metadata
nsc.stage44.hir-dump                    Regex=[generic-specialization=.*Identity``1<int>
nsc.stage44.mir-dump                    Regex=[generic-specialization=.*Identity``1<long>
```

**No era un problema de texto de prueba: cambiar el formato de una identidad pública por un
problema interno rompe a quien la lea.** La corrección es que el sufijo se aplica **sólo donde hay
colisión** — dos plantillas que comparten `nombre``aridad`. Toda clave que era única se queda
byte a byte como estaba.

Y eso se puede demostrar sin medir nada: antes de este cambio, una colisión **era un error de
compilación**. El corpus compilaba limpio, luego no hay ninguna colisión en él salvo la fixture
nueva — así que no hay ni una clave que cambie.

### El consumidor, que es la mitad que importa

`Algorithm.SortWith` **es ahora `Algorithm.Sort`**, una sobrecarga de verdad. Un nombre peor que
sobrevive al motivo que lo justificaba es deuda disfrazada de decisión.

`Spans.OfRange` se queda con su nombre: 51 llamadas, y `OfRange` describe bien lo que hace. Lo que
sí se corrigió es su comentario, que decía que el nombre era un rodeo obligado.

Fixture `c35_sobrecarga_generica`: tres sobrecargas con la misma aridad genérica y uno, dos y tres
parámetros; dos parámetros de tipo; argumentos que el compilador no puede plegar; y al final
`Algorithm.Sort` con y sin comparador.

---

## 3. `demo_ventana`: la ventana de verdad

El programa usaba la clase predefinida `"STATIC"` del sistema, y el comentario decía por qué:
*"registrar una clase propia exige un WNDCLASSEXA con un puntero a función en un campo, y eso es
una pieza más"*.

Esa pieza ya está. **Un puntero a función dentro de una struct que cruza a Win32** —la frontera en
la dirección difícil: código de C llamando a código de NativeSharp.

- `extern C delegate long VentanaProc(void*, uint, ulong, long)` — la forma que el lenguaje ya
  tenía, la misma que mide `c1_03`.
- `struct ClaseVentana` con los doce campos de `WNDCLASSEXA` en su orden y con sus huecos: 80 bytes
  en x64, porque quien la lee es el sistema.
- `RegisterClassExA`, `DefWindowProcA`, `LoadCursorA`, `UnregisterClassA`, `GetModuleHandleA`.

**Teclado y ratón:** `WM_KEYDOWN` (con ESCAPE para salir), `WM_LBUTTONDOWN`, `WM_MOUSEMOVE`,
`WM_CLOSE`/`WM_DESTROY`. Los contadores se pintan en la ventana cada cuadro —si no, no habría forma
de saber si el procedimiento se está llamando— y se resumen por consola al terminar.

### Lo que el IR enseñó, y no esperaba

El compilador envuelve el procedimiento en un **thunk de frontera** automáticamente:

```llvm
define i64 @...__Procedimiento__c_boundary(...) personality ptr @__C_specific_handler {
  %boundary.result = invoke i64 @...__Procedimiento(...)
```

Es la guarda C1.3 (`CallbackExceptionEscape`): una excepción de NativeSharp que llegara al borde de
una función llamada por C desenrollaría marcos ajenos cuyo contrato de limpieza este compilador no
conoce. **El procedimiento de ventana la recibe gratis**, sin declarar nada.

Y los dos diagnósticos de ownership del doc 112 §C.3 volvieron a aparecer, los dos correctos:
`String.ToString(int)` devuelve `owned string` y hay que ligarlo a un `raii` antes de pasarlo.

---

## 4. `nsc.cc1.correctness`, repartido

De la corrida del 2026-09-03:

```
ctest -j 16      244 pruebas   reloj 274.47 s
la mas lenta:    nsc.cc1.correctness         273.97 s
la siguiente:    nsc.stage56.c8-type-root     90.77 s
```

**El reloj de ctest era esa prueba.** Compilaba y ejecutaba treinta y seis fixtures en serie dentro
de un proceso, con las otras quince CPU paradas. Repartidas, el techo pasa a ser la siguiente más
lenta: **de ~274 s a ~91 s, unos tres minutos por pasada y seis por corrida.**

Y el segundo motivo, que no es velocidad: un rojo decía `nsc.cc1.correctness` y había que abrir el
log para saber **qué** fixture falló. Ahora lo dice el nombre de la prueba.

### La lista se lee del `.cpp`, no se escribe en CMake

Escribirla en `CMakeLists.txt` habría sido **la quinta copia de una lista** en este proyecto, y las
cuatro anteriores costaron un documento cada una: la tabla de imports (docs 102/103), las tablas de
enteros (112), las cadenas del HIR (106) y las dos tablas de casos (115). CMake extrae las
etiquetas del propio fuente al configurar. **Añadir una fixture sigue siendo una línea, en el
`.cpp`, como siempre.**

Con un guardián, porque una lista extraída por un regex puede quedarse corta en silencio:

```cmake
if(_cc1_count LESS 30)
    message(FATAL_ERROR "...solo se reconocieron ${_cc1_count} fixtures...")
```

Sin eso, cambiar la forma de `checkFixture` daría una corrida **verde que mide una décima parte** —
que es peor que un rojo.

### Y una carrera que había que evitar antes de repartir

Las 35 pruebas corren **en paralelo**, y todas borraban y reescribían el mismo directorio temporal.
La raíz lleva ahora el selector en el nombre. Dos procesos vaciando la misma carpeta mientras el
otro compila dentro es una carrera que se manifiesta como un fallo de enlace intermitente y no como
lo que es.

---

## 5. Lo medido

| | |
|---|---|
| `./sweep.sh llvm` | **OK=180 FAIL=5** (los cinco negativos; eran 178) |
| `nsc.cc1.integer-evaluator` | 39/39 |
| `nsc.llvm-lowering` | 39/39 |
| equivalencia de los predicados de tipo | idénticas en los 20 tipos |
| extracción de etiquetas por CMake | **35 fixtures**, ejecutando el bloque real |
| casos del corpus | **55 → 110 trabajos** |

**No verificado desde el contenedor:** que `c34` y `c35` pasen ejecutando, y que el reparto de
ctest funcione — eso necesita configurar CMake en Windows. `nsc.llvm-optimization` sigue sin poder
ejecutarse aquí (usa API de LLVM 23).

## 6. La corrida, y lo que el reparto midio de verdad

```
ctest    279 pruebas   reloj 114.14 s      (eran 244 en 274.47 s)
la mas lenta:  nsc.stage56.c8-type-root    113.60 s
```

**De 274 s a 114 s.** El techo es ya la siguiente prueba más lenta, como estaba previsto: a partir
de aquí, repartir más fixtures no gana nada — lo que habría que partir es `stage56.c8-type-root`.

El conteo sube de 244 a 279 porque `nsc.cc1.correctness` pasa de una prueba a treinta y seis. **No
hay más comprobaciones: son las mismas, repartidas.**

Los cuatro rojos de esa corrida fueron los de §2, todos de la primera versión del ID 132, todos
arreglados. Fixtures **110/110**.
