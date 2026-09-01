# CC1 — Fases 4B, 4C, 4D y 4E: `stackalloc`, raw `new T`, aritmética de punteros y cierre de paridad

**Fecha:** 2026-08-31 · **Alcance:** el compilador. Nada más.
**Origen:** hallazgos M-001, M-002, M-005, M-006 y R-003 del documento de auditoría.
**Tabla:** `MILESTONE_1_TASKS.md`, fases 4B–4E. **Cierra el gate G4.**

---

## 0. Por qué las cuatro juntas

`stackalloc`, raw `new T` y la aritmética de punteros son **el mismo hueco tres veces**: el parser
las acepta, el modelo semántico las tipa, y mueren al bajar. Y las tres atraviesan exactamente las
mismas cuatro capas — semántica → HIR → MIR → LLVM — pidiendo lo mismo en la tercera: cuánto mide un
elemento.

Hacerlas por separado habría significado tres barridos de símbolos del backend, tres juegos de
decisiones de plomería y tres corridas de Windows. Juntas son un barrido, un juego y una corrida.

---

## 1. Lo primero: el barrido del backend

Es la tercera fase que toca `ProjectOopLlvmLowerer.cpp`, y el árbol no incluye las cabeceras de
LLVM: `src/LLVM/LlvmCApi.h` declara a mano el subconjunto que se usa, y llamar a algo no declarado
falla en compilación como identificador desconocido.

```
LLVMBuildArrayAlloca       DECLARADO
LLVMBuildGEP2              DECLARADO
LLVMBuildPtrDiff2          DECLARADO
LLVMBuildPtrToInt          DECLARADO
LLVMSetAlignment           DECLARADO
LLVMBuildSub / SDiv / Neg  DECLARADO
LLVMBuildInBoundsGEP2      *** FALTA ***
```

El único que falta es el único que **no debo usar** (§4.3). Tras escribir el código, el barrido
completo: **143 símbolos usados, 0 faltantes.**

---

## 2. Fases 4B y 4C — `stackalloc T[n]` y `new T`

### 2.1 Lo que había, medido

Las dos formas compartían la cola de un mismo `case` en el modelo semántico. Esa cola hacía
exactamente dos cosas: poner `result.typeKey = element + '*'` y marcar válido. Sin binding, sin una
sola validación.

Consecuencias reales, todas comprobadas leyendo:

- `stackalloc int[unaCadena]` **tipaba sin decir una palabra** — el contador nunca se validaba
  (la forma dinámica `new T[n]`, dos ramas más arriba, sí lo exige);
- `stackalloc void[4]` y `new void` tipaban igual;
- un elemento con destructor o con campos `owned` se aceptaba;
- y al bajar, `stackalloc` moría en el `default` del HIR mientras `new T` moría en un error que lo
  decía con todas las letras: *"raw new remains outside this pipeline"*.

### 2.2 La tarea 47 tenía la misma premisa falsa que la 40

Decía: *«portar a `ProjectMethodBodyModel` las validaciones de `stackalloc` **del pipeline
clásico**»*.

Barrido completo del árbol: `stackalloc` sólo aparece en el lexer, el parser, el AST y ese caso.
**No hay pipeline clásico del que portar nada** — igual que no lo había para `sizeof` en la 4A.

Es la segunda vez, y ya no es un descuido puntual del documento de origen: **atribuye
repetidamente a un «pipeline clásico» cosas que no están en este árbol.** Conviene tenerlo presente
al leer las fases que quedan.

Las validaciones de abajo están **escritas**, no portadas, y cada una lleva su razón:

| Validación | Por qué |
|---|---|
| elemento no `void` | sin tamaño no hay reserva posible, ni en pila ni en montón |
| elemento sin destructor y sin campos `owned` | su ciclo de vida necesita que alguien lo cierre, y ni un buffer de pila ni un bloque crudo lo cierran: en la pila, además, la memoria desaparece bajo los pies del objeto |
| contador integral (sólo `stackalloc`) | el hueco literal descrito arriba |

`DiagnosticId::InvalidStackalloc` llevaba tiempo declarado y **sin usar en ningún sitio**. Ahora se
usa. Es el segundo diagnóstico fantasma que esta fase despierta, después de `InvalidSizeof`.

### 2.3 El criterio de «puede vivir en memoria cruda»

```cpp
bool ProjectMethodBodyModel::elementTypeHasTrivialLifetime(std::string_view typeKey) const noexcept
```

Pregunta por dos cosas, por separado porque son dos razones distintas: un **destructor declarado**
(alguien escribió código que tiene que correr al final, y en memoria cruda no hay quien lo llame) y
un **campo `owned`** (el objeto posee otra asignación, y abandonar el contenedor la filtra).

Una **clase** como elemento no es problema y se acepta: lo que se reserva es un buffer de
*referencias*, y una referencia no posee nada por sí misma.

---

## 3. Fase 4D — aritmética de punteros

### 3.1 Por qué importa más de lo que parece

`p[i]` funcionaba desde hacía rondas. `*(p + i)` no compilaba. **Son la misma dirección escrita de
dos formas**, y el nivel crudo de este lenguaje existe justamente para expresar formas de C, donde
la segunda es tan común como la primera. Un consumidor externo lo reportó como limitación antes de
que esta fase existiera.

La fixture `c12` abre comprobando exactamente eso, en bucle sobre las ocho posiciones:

```
if (buffer[index] != *(buffer + index)) { return 1; }
```

En el backend las dos salen del **mismo GEP de bytes con el mismo paso**. Eso no es una casualidad
conveniente: si algún día dejan de compartir la forma, dejan de ser lo mismo, y esa línea lo ve.

### 3.2 El orden importa, y por eso la rama va antes

La rama de punteros va **antes** de la rama aritmética general, no después. `commonNumericType` no
conoce los punteros, así que si fuera después, `p + 1` ya habría muerto en *"binary operator
requires compatible numeric operands"*.

### 3.3 Lo que se acepta, y lo que no

| Forma | Resultado |
|---|---|
| `p + i`, `i + p` | `T*` |
| `p - i` | `T*` |
| `p - q`, mismo tipo | `builtin:nint` — **con signo**, porque `p - q` puede ser negativo, y del ancho del puntero, que es el rango que la diferencia alcanza |
| `p + q` | rechazado: sumar dos direcciones no significa nada |
| `i - p` | rechazado: restarle una dirección a un entero tampoco |
| `p - q` de tipos distintos | rechazado: no hay un paso con el que dividir |
| cualquier cosa sobre `void*` | rechazado: un paso de tamaño cero no es un paso |

Los cuatro rechazos tienen mensaje propio y test negativo propio. Sin ellos habrían caído por el
mensaje genérico de operandos numéricos, que para `left + right` entre dos punteros habría sido
desconcertante.

### 3.4 La tarea 62 no se puede hacer, y no por poco

Pedía *«extender `analyzeUnary` y el lowering de `PlusPlus`/`MinusMinus` para que un operando de
tipo puntero use la misma infraestructura»*.

Fui a ver ese lowering antes de extenderlo. **No existe, para ningún tipo.** El HIR contesta
`"Stage 34 OOP lowering does not yet lower ++/--"` también para un `int`.

Hacerlo sólo para punteros dejaría `++p` funcionando y `++i` no — una asimetría difícil de explicar
y peor de mantener. Queda como **tarea 79**, con ID propio, planteada como lo que es: implementar
`++`/`--` en el pipeline OOP, para todos los tipos. En la fixture se escribe `p = p + 1`, que es la
misma operación y sí compila.

---

## 4. Las decisiones del backend que no son obvias

### 4.1 Todo en bytes, una sola regla de tamaño

`stackalloc` reserva `alloca i8, n * elementSize` en vez de `alloca <T>, n`, y la aritmética usa un
GEP de `i8`. La razón es la misma que ya seguía `RawElementAddress`: **una sola regla de tamaño para
todo el nivel crudo**. El tamaño lo pone el MIR desde `elementLayout`, que es el mismo helper del
que salen el stride de `p[i]` y el offset de un campo. Ni una tabla nueva.

### 4.2 La alineación, que es fácil de olvidar

Un `alloca i8` sale **alineado a 1**. Un `double` necesita 8. Sin `LLVMSetAlignment` con el valor
del layout, `stackalloc double[3]` produciría accesos desalineados. Por eso la fixture `c10` mide
los tres pasos —`byte` (1), `int` (4), `double` (8)— y no sólo uno: con un paso fijo o una
alineación fija, alguno de los tres falla.

### 4.3 GEP **sin** `inbounds`, a propósito

`inbounds` promete al optimizador que el resultado se queda dentro de la misma asignación. **Este
nivel del lenguaje no hace esa promesa**: no hay comprobación de rango, la semántica es la de C, y
el propio comentario de `RawElementAddress` lo dice. Prometerlo convertiría en comportamiento
indefinido código que el lenguaje declara legal.

Que `LLVMBuildInBoundsGEP2` sea el único símbolo no declarado del barrido es una coincidencia
afortunada: el árbol nunca lo ha necesitado.

### 4.4 El `alloca` va donde está la expresión

No en el bloque de entrada. Es lo que `stackalloc` significa: dentro de un bucle, cada vuelta
reserva. LLVM no promociona un alloca fuera del bloque de entrada, y aquí eso es el comportamiento
correcto, no una limitación. La fixture `c10` lo ejerce.

### 4.5 La diferencia se emite explícita

`ptrtoint` − `ptrtoint` → `sdiv` por el paso, en vez de `LLVMBuildPtrDiff2`. Dos razones: el paso
queda **visible en el IR** en vez de escondido en un helper, y `sdiv` deja escrito que la operación
tiene signo. Una división sin signo daría un número enorme donde debe salir `-7`.

---

## 5. Fase 4E — el inventario del `default`, medido

La tarea 65 pedía dar caso explícito a lo que quedara cayendo en el `default` del HIR. Para saber
qué queda hay que contarlo, así que crucé las 30 formas de expresión del AST contra los `case` de
`lowerExpression`:

```
expresiones en el AST: 30
con case en lowerExpression: 28
SIN case: 2   -> PointerMemberExpression, LambdaExpression
```

**Y una de las dos era un falso positivo de mi propio barrido.** Las lambdas *sí* se bajan — por
binding, antes del `switch`, no por un `case`. Lo comprobé antes de escribirlo en ningún sitio. El
barrido mide lo que mide, no lo que uno quiere que mida.

Así que tras estas fases queda **exactamente una** forma sin bajar: `p->campo`. El parser la acepta,
el modelo semántico la tipa, y el HIR no la tenía. Ahora tiene mensaje propio:

```
pointer member access 'p->member' is not lowered by the OOP pipeline yet;
write '(*p).member', which is the same access and is supported
```

En vez de `"unsupported Stage 34 OOP expression kind: PointerMemberExpression"`, que suena a fallo
interno del compilador y no a lo que es. Arreglarla es la **tarea 80**, con ID propio.

Es, por cierto, la **cuarta** forma con la mitad delantera presente y la trasera ausente que aparece
en este milestone, después de `sizeof`, `stackalloc` y raw `new`. El patrón no era casualidad.

---

## 6. Archivos tocados

| Archivo | Qué |
|---|---|
| `include/…/ProjectMethodBodyModel.h` | `StackAllocation`, `RawAllocation`, `PointerArithmetic`; `elementTypeHasTrivialLifetime` |
| `src/Frontend/ProjectMethodBodyModel.cpp` | validaciones y bindings de `stackalloc`/raw `new`; rama de punteros en `analyzeBinary`; el helper; los tres nombres del switch exhaustivo |
| `include/…/ProjectOopHirModel.h` + `.cpp` | tres nodos, sus nombres, tres lowerings, y el caso explícito de `p->campo` |
| `include/…/ProjectOopMirModel.h` + `.cpp` | tres nodos, sus nombres, y una sola conversión compartida que resuelve tamaño y alineación por `elementLayout` |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | `StackAllocation`, `RawAllocation`, `PointerArithmetic` |
| `tests/cc1/c10_stackalloc/`, `c11_raw_new/`, `c12_pointer_arith/` | tres fixtures nuevas |
| `tests/unit/Cc1CorrectnessTests.cpp` | las tres fixtures más **ocho** negativos |
| `run_windows_checklist.bat` | tres `run_case` nuevos |

Sin cambios en `CMakeLists.txt`. Los cinco `.cpp` compilan con **cero avisos** bajo `-Wall
-Wextra` — que no es cosmético: el build de Windows usa `/W4` con `NSC_WARNINGS_AS_ERRORS=ON`.

---

## 7. G4 cerrado — la corrida

**Corrida de Windows del 2026-08-31 18:42.**

```
Passed: 63
Failed: 0

[ OK ] cc1_c10_stackalloc    -O0 / -O2 -- exit 42
[ OK ] cc1_c11_raw_new       -O0 / -O2 -- exit 42
[ OK ] cc1_c12_pointer_arith -O0 / -O2 -- exit 42
```

`ctest` **242/242, 100%**. Build con **cero avisos** —lo que importa con `/W4` y
`NSC_WARNINGS_AS_ERRORS=ON`—. `nsc.cc1.correctness` sube de 29.88 s a **33.27 s**, consistente con
tres fixtures y ocho negativos nuevos. Triage 16/16.

Veinte tareas, cuatro capas, cinco archivos de compilador, a la primera.

### Los cuatro riesgos, medidos

Los anoté aquí antes de conocer el resultado, que es la única forma de que una predicción valga
algo. Ninguno se materializó:

1. **`(*record).First = 7` sobre un bloque del montón** (c11). El comentario del backend prometía
   esta forma sobre una variable local; funciona igual sobre memoria recién reservada.
2. **`nint` comparado con literales negativos** (c12). `buffer - last != -7` se evalúa como debe:
   la conversión constante a `nint` acepta el negativo y la división con signo da −7 y no un número
   enorme.
3. **`stackalloc` de un struct con relleno** (c10). El paso es 16 y no 13, así que los dos
   elementos no se pisan — la misma respuesta que da `sizeof(Padded)` en c9, desde el mismo
   `elementLayout`.
4. **La alineación del `double`** (c10). Era el más traicionero: un acceso desalineado en x64 puede
   salir como valor incorrecto en vez de como crash, así que un fallo aquí podría haber pasado por
   otra cosa. `LLVMSetAlignment` con el valor del layout basta.

### La propiedad central, ejercida

```
if (buffer[index] != *(buffer + index)) { return 1; }
```

Ocho posiciones, a `-O0` y a `-O2`. `p[i]` y `*(p + i)` son la misma dirección, y ahora el
compilador está de acuerdo. Era la limitación que un consumidor externo había reportado.

### Estado

Fases 4B **6/6**, 4C **4/4**, 4D **7/7** (la 62 salió como tarea 79), 4E **2/2**. **G4 cerrado.**
Milestone **70/80**. Queda la fase 5 — reproducibilidad y cierre, tareas 67–73.

---

## 8. Tareas nuevas, todas con ID propio

| ID | Qué | Dónde |
|---|---|---|
| 79 | `++`/`--` no se baja en el pipeline OOP **para ningún tipo** (§3.4) | fase propia, fuera de CC1 |
| 80 | `p->campo` no se baja; `(*p).campo` sí (§5) | fase propia, fuera de CC1 |
| 78 | `valueLayoutForTypeKey` dice 8 para un enum y `layoutNamedType` dice 4 | fase 4E, sin medir |
| 77 | el conteo de checks no se ve en los logs cuando la suite pasa | fase 5 |
