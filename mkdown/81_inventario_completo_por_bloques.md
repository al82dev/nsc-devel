# 81 — Inventario completo por bloques

**Fecha:** 2026-09-01
**Qué es:** la lista **entera** de trabajo pendiente del compilador, repartida en los ocho bloques
del doc 69, con el ID o el SHR de cada ítem y el nivel al que está especificado. Consolida el doc 62
(Listas A/B/C), el doc 69 (los ocho bloques), el registro CC2 del doc 60 §15 y el handover del
auto-hospedado.
**Para qué:** el doc 69 dimensiona los bloques pero no enumera ítem por ítem, y el registro CC2
enumera pero no reparte en bloques. Este documento es la unión de los dos, para poder tachar líneas.
**Alcance:** compilador (`nsc-devel`) y nada más. Bindings, port de Saru y port del tutorial de
OpenGL van por sus propios carriles y no aparecen aquí.

**Columna «Spec»** — cuánto hace falta averiguar antes de escribir código:

* **L** — especificado hasta la línea: archivo, línea, y qué se escribe.
* **E** — enumerado: los casos están contados y nombrados, falta el diseño de la solución.
* **D** — descrito: se sabe qué falla, falta medir y decidir.
* **?** — decisión antes que código.

---

## Bloque 1 — Correctness del frontend

**Depende de:** nada. Es el primero y no hay alternativa: es lo único que puede corromper en
silencio todo lo que se construya encima.
**Terminado cuando:** `c17_constant_promotion` y `c18_stackalloc_hardening` verdes a O0 y O2, y los
242 de la suite siguen verdes.

> ## ✅ CERRADO — 2026-09-02
>
> `242/242` en ctest · `Passed: 80  Failed: 0` en el checklist · `MANIFEST: 1222/1222 PASS`.
> Las siete fixtures del bloque **ejecutadas de verdad** a `-O0` y `-O2`, no sólo compiladas —
> `run_windows_checklist.bat`, que es la única corrida que las ejecuta bajo Windows.
>
> **Cerrados:** 121, 122 (doc 82) · 123, 89 (doc 84) · **127** (doc 86, P1 encontrado de paso) ·
> 82, SHR-005 (doc 87).
>
> **Encontrados durante el bloque, y cerrados después en su propia ronda:**
> ✅ **ID 126 + ID 120** — una sola superficie de 31 pares en `commonNumericType`, medida par a par
> en el doc 89 y **cerrada el 2026-09-02**: mecanismo de C++ —promover primero, reglas ordenadas,
> dos enteros nunca dan `double`— con las respuestas de C# allí donde C++ muerde (`int + uint` →
> `long`, `long + ulong` → error). La corrección no fueron tres parches sino **una inversión de
> orden**: para un par de integrales contesta `integerPromotion` y nadie más. La tabla de 91 pares
> vive en el test y **es la especificación**.

### 1a

| ID / SHR | Qué | Spec | Dónde está la medición |
|---|---|:--:|---|
| ✅ **122** — SHR-062 | `ProjectIntegerConstant` tiene dos convenciones: la struct documenta `value` como **magnitud** + `isNegative`, y el evaluador le mete `bits` en complemento a dos con el signo ya extendido | **L** | `ProjectIntegerEvaluator.cpp:268-269`. 10 ocurrencias de la struct en todo `src/`; el defecto son **2 líneas**. Doc 64, handover §8.1 |
| ✅ **121** — SHR-002 | `int + long` devuelve `double` y el folding de constantes se rinde: falta la regla de promoción entera de C# | **L** | `ProjectTypeConversion.cpp`, `commonNumericType`. Sitio de inserción: entre el final de la regla del ID 120 (línea 321) y el bucle de preferencia (línea 323). Handover §8.2 |
| ✅ **123** — SHR-004 | `stackalloc` hardening | **E** | Cuatro casos en doc 60 §8: (1) contador constante negativo, (2) contador desmesurado sin cota, (3) `alloca` dentro de un bucle — nada se libera hasta salir de la función, (4) desbordamiento de `count * elementSize` en 64 bits. `c10_stackalloc` no cubre ninguno |

**La trampa del 121, y es la propiedad que se preservó:** la regla nueva va **DESPUÉS** de la del
ID 120, no antes. Delante, `short s; s + 3` pasa a evaluarse en `int` y `short t = s + 3;` vuelve a
no compilar — el defecto exacto que el ID 120 cerró.

> **Nota posterior (doc 89):** esa colocación era correcta **para el par que el 121 tenía delante**,
> y es justo lo que dejó vivos los otros 30. La guarda de `integerPromotion` se rinde calladamente
> y la expresión cae al bucle de preferencia, cuya primera parada es `double`. El ID 121 no falló:
> midió un par y arregló ese par. La superficie entera está en el doc 89.

### 1b

| ID / SHR | Qué | Spec | Dónde está la medición |
|---|---|:--:|---|
| ✅ **82** — resto de SHR-008 | Asignación compuesta (`+=` etc.) sobre propiedad e indexador | **D** | Deferido en la semántica. Necesita secuenciar getter y setter y decidir el orden de evaluación del receptor. Docs 59 §9, 60 §9 |
| ✅ **SHR-005** | Owned destination: residuo de **arreglo fijo** y agregado por valor | **D** | Residuo acotado (doc 68): locals, elementos de arreglo, campos que poseen almacenamiento, propiedades e indexadores **ya están cubiertos** |
| ✅ **89** — SHR-065 | No existe `sbyte`. Una API de C con `signed char` no se puede declarar honestamente | **E** | Once capas enumeradas en doc 60 §10: keyword y `TokenKind`, `isBuiltinTypeKeyword` (**es un rango**), `constantFits`, `isSignedInteger`, `commonNumericType`, tabla de preferencia, `BuiltinLayoutKind`, `llvmType`, ABI de argumentos, switch de conversiones. `grep sbyte src/Lexer/Lexer.cpp` → **cero** |

**Riesgo específico del 89:** las dos funciones `…NodeKindName` no tienen `default` y
`isBuiltinTypeKeyword` compara por rango. Un enumerador mal colocado **no da error de compilación**,
da un tipo que se comporta raro. Regla: compilar con `-Wall -Wextra` antes de entregar.

---

## Bloque 2 — El suelo del lenguaje

**Depende de:** Bloque 1 (la aritmética de constantes que usará el parser).
**Terminado cuando:** un programa NativeSharp lee `0x1F`, `42UL` y `3.14f` de una cadena y escribe
un diagnóstico formateado por stderr.
**Tamaño:** 3 superficies, del orden de 25 funciones.

> ## ✅ CERRADO — 2026-09-02
>
> **LOS NÚMEROS DE LA CORRIDA, que es lo único que cierra un bloque.** `all.bat`, 2026-09-02:
>
> ```
> Passed: 84   Failed: 0
> ctest:  100% tests passed out of 242
> cases:  82 OK, 0 FAIL en 25.5 s con -j 16 (suma 353.6 s -- x13.9)
> build:  limpio, sin un error ni un warning, con /WX
> bug_triage: 16/16, cero vivos
> MANIFEST: 1237/1237 PASS
> cc1_c22_console_and_formatting -O0 -- exit 42
> cc1_c22_console_and_formatting -O2 -- exit 42
> ```
>
> Las dos optimizaciones, no una: `-O2` es donde un formateo que depende de un desbordamiento
> silencioso cambiaría de respuesta.
>
> El criterio de arriba **es** `tests/cc1/c22_console_and_formatting`, ejecutando, salida 42: lee
> `0x1F`, `42UL` y `3.14f` de **una** cadena sin cortar subcadenas y escribe
> `c22.ns(1,1): nota: leidos 0x001F, 42 y 3.14 en 14 caracteres` **por error estándar**.
>
> **Cerrados:** SHR-024 (doc 90) · SHR-022 (doc 91) · SHR-023 (doc 92).
> **Encontrado y cerrado de paso:** **ID 128** — el parser rechazaba
> `wide < (long)G() || wide > (long)G()` porque la búsqueda de una lista de argumentos genéricos
> encontraba el `>` de la *segunda* comparación (doc 93 §2).
> **Abierto y anotado:** **ID 129** (enums por nombre, es del compilador).

| SHR | Qué | Spec | Dónde está la medición |
|---|---|:--:|---|
| ✅ **024** | Parsing numérico: signed/unsigned, bases **2–16**, float, detección de overflow | **D** | `lib/NativeSharp.Core/Number.ns` nuevo. Núcleo sobre `(texto, ref índice)` para que un lexer no corte una subcadena por token. **Las bases se ampliaron de {2,8,10,16} a 2–16 al cerrar SHR-023**, para que las dos direcciones admitan el mismo conjunto (doc 92 §8). Fixture `c21_number_parsing`, siete bloques. Doc 90 |
| ✅ **022** | `Console.Error` y `Console.Write` | **D** | Cuatro métodos, **un** nodo, dos bits de máscara (bit 0 salto de línea, bit 1 canal). `Console.Error.WriteLine` rechazado a propósito: pide `TextWriter`, que es SHR-020 / Bloque 5. Doc 91 |
| ✅ **023** | Formatting de float/double, hex, vistas y enums | **D** | Bases 2–16, hexadecimal como patrón de bits (distinto de `ToString(v, base)`, que es signo+magnitud), `ToString(double)` con garantía **medida** de once cifras contra glibc. Vistas → Bloque 3 por orden; enums → **ID 129**, es del compilador. Doc 92 |

**Lo que este bloque enseñó, y no estaba en el plan:**

* **El número que promete una API tiene que medirse, no elegirse.** `ToString(double)` iba a
  prometer quince cifras (el `DBL_DIG` de IEEE-754) hasta que la medición contra `printf` de glibc
  dio **16.6% de desacuerdo** a quince y **0.000%** a once. Quince cifras habrían sido cuatro de
  ruido con la misma cara de seguridad que las once primeras.
* **El defecto grave lo encontró el modelo, no la lectura.** `1e-300` se imprimía como
  `9.2233720368547758E-299` — 2^63 disfrazado — porque escalar por `10^310` desborda el `double` y
  el casteo a `ulong` satura. Leer el código no lo enseña.
* **Un ítem del plan puede contener un ítem de otro bloque.** «Formatting de … vistas y enums» son
  tres cosas de tres capas distintas: biblioteca (float/hex), Bloque 3 (vistas) y **compilador**
  (enums). Cerrar el bloque exigió separarlas, no hacerlas todas.

### Abierto y anotado en este bloque

| ID | Qué | Spec | Dónde |
|---|---|:--:|---|
| **129** — resto de SHR-023 | `ToString` de un **enum** por su NOMBRE. Necesita una tabla nombre/valor **generada por el compilador**: los nombres no sobreviven a la compilación | **D** | El mecanismo tiene precedente (`StructToString`, línea C8 "type root", sintetiza miembros y los resuelve estáticamente) y los datos existen (`isEnumConstant` + `enumConstantValue` en `ProjectMemberModel.h`). **Es del compilador, no de `String.ns`** — por eso no entró aquí. Doc 92 §7 |

---

## Bloque 3 — Vistas y tiempo de vida

**Depende de:** Bloque 1.
**Terminado cuando:** el lexer guarda lexemas como vistas sin copiar y el compilador rechaza
devolver una vista de un local.
**Tamaño:** 4 tipos + 1 regla de análisis en el compilador.

> ## ✅ CERRADO — 2026-09-02
>
> **PENDIENTE DE LOS NÚMEROS DE WINDOWS.** Lo que está medido, y lo está en el contenedor con el
> compilador real —frontend, MIR y **bajada a LLVM IR verificada por LLVM**—:
>
> ```
> 167 de 171 .nsproj del árbol: OK, hasta IR verificado
>   los 4+1 que faltan son los ejemplos NEGATIVOS que tienen que fallar
> 14/14 pruebas unitarias que sólo piden nsc_frontend -- 372 comprobaciones
> nsc.stage58.view-lifetimes: 31/31
> todos los .cpp tocados con -Wall -Wextra: cero avisos
> ```
>
> El criterio de arriba tiene **dos mitades que no caben en la misma prueba**, y eso no es un
> detalle de forma:
>
> * «el lexer guarda lexemas como vistas sin copiar» → `tests/cc1/c23_views_and_lifetimes`,
>   ejecutando, salida 42. Y **«sin copiar» no se afirma**: cada lexema se compara contra
>   `String.CharacterPointer(fuente, desplazamiento)`, así que una copia caería fuera del buffer
>   original y la fixture lo vería.
> * «y el compilador rechaza devolver una vista de un local» → **no puede medirse ejecutando**: un
>   programa rechazado no produce ejecutable. Vive en `tests/unit/Stage58ViewLifetimesTests.cpp`,
>   que compila 12 formas prohibidas exigiendo el **diagnóstico exacto** de cada una, y 8
>   permitidas que tienen que seguir compilando.
>
> **Cerrados:** SHR-010 · SHR-011/012 · SHR-017 · SHR-013 — todos en el doc 94.
> **Encontrados de paso, medidos y anotados:** **ID 131** (un miembro estático de un genérico
> cerrado no se puede nombrar: `Box<int>.Make(1)` no parsea), **ID 132** (dos métodos genéricos con
> el mismo nombre y aridad no se sobrecargan), **ID 130** (`uwtable` se pide con valor 0, que en
> LLVM significa *sin* tabla de desenrollado).

| SHR | Qué | Spec | Dónde está la medición |
|---|---|:--:|---|
| ✅ **010** | `StringView` | **D** | `borrowed struct { char* start; int count; }`, 16 bytes. Necesitó **una** operación núcleo, `String.CharacterPointer`, porque `&texto[0]` no compila —y no debe: daría un puntero escribible sobre una cadena inmutable. En el backend es la misma llamada que `StringElementLoad` sin el load: **cero aritmética nueva**. Doc 94 §3 |
| ✅ **017** | `Optional<T>` | **D** | No es vista y por eso no es `borrowed struct`: guarda una copia por valor. Mide el caso que ningún centinela cubre —buscar `-1` en un arreglo que **contiene** `-1`—. `Optional<StringView>` no compila, y eso **cae gratis** de la regla R1. Doc 94 §5 |
| ✅ **011 / 012** | `Span<T>` y `ReadOnlySpan<T>` | **D** | Dos tipos y no uno con un `bool readOnly`: el booleano cuesta una comparación **por escritura** y mueve a ejecución lo que el sistema de tipos hace gratis en compilación. Doc 94 §4 |
| ✅ **013** | Escape checking para vistas — regla en el compilador | **D** | `borrowed struct` + tres reglas (campo, retorno, captura) + el análisis de enraizamiento. Doc 94 §2 |

**Por qué el escape checking va aquí y no después:** una vista que sobrevive a su dueño es un
uso-después-de-liberar, y añadir la comprobación más tarde obliga a reescribir a los consumidores.

**Lo que este bloque enseñó, y no estaba en el plan:**

* **La comprobación de escape que ya existía no veía nada de esto, y no por descuido.** Stage 50.7
  rechaza un `borrowed Resource` fugado, pero su regla está detrás de
  `isReferenceTypeKey(t) || t == "builtin:string"`, y una vista es un **struct por valor**. El
  hueco no se ve leyendo la regla vieja —es correcta para todo lo que existía cuando se escribió—:
  se ve preguntándole por un tipo nuevo.
* **Un modificador explícito le ganó a la inferencia, y por una razón medible.** Deducir «un struct
  con un campo puntero es una vista» habría convertido en vista cada struct que hoy lleva un
  puntero sin pedirlo, cambiando el significado de código ya verde.
* **Una regla de rechazo sin su mitad positiva no dice nada.** La primera versión habría pasado los
  cinco casos negativos prohibiendo las vistas por completo. Por eso la prueba mide también las
  ocho formas que **tienen** que seguir compilando.
* **Y el Bloque 2 y el Bloque 3 componen sin una línea duplicada.** La tentación era dar a `Number`
  sobrecargas sobre `StringView` — unas 350 líneas del núcleo de parsing en dos copias que pueden
  separarse. No hace falta: el núcleo del SHR-024 ya trabaja sobre `(texto, ref índice)`, así que un
  lexer lee los números **en sitio** y guarda los demás lexemas como vistas.

### Abierto y anotado en este bloque

| ID | Qué | Spec | Dónde |
|---|---|:--:|---|
| **130** | `applyUwtableAttribute` pide `uwtable` con valor **0**. En LLVM el valor es un `UWTableKind`: 0 = None, 1 = Sync, 2 = Async. **Cero significa «sin tabla»**, lo contrario de lo que el nombre promete | **E** | Medido con la propia `LlvmCApi.h` sobre LLVM 18: con 0 el módulo impreso sale truncado a 65 bytes; con 2, correcto. En Windows x64 la tabla de desenrollado es lo que SEH necesita. **No se tocó**: cambiar los atributos de todas las funciones es una decisión con su propia medición en Windows. Doc 94 §6 |
| **131** | Un miembro **estático** de un tipo genérico **cerrado** no se puede nombrar en una expresión | **D** | `Box<int>.Make(1)` → *expected expression*; `Helper.Wrap<int>(1)` compila. Pide que el modelo de nombres resuelva una instanciación genérica en posición de **expresión**. Por eso las fábricas viven en `Spans`/`Optionals`. Doc 94 §6 |
| **132** | Dos métodos genéricos con el mismo nombre y la misma aridad de tipos **no se sobrecargan** | **E** | `duplicate generic template identity '…Spans.Of``1'`. La identidad es nombre + número de parámetros de tipo, sin la lista de parámetros. Por eso hay `OfRange`. Doc 94 §6 |

---

## Bloque 4 — Sistema operativo y binarios

**Depende de:** Bloque 3 (`Span<T>` para los buffers).
**Terminado cuando:** un programa NativeSharp encuentra fuentes, las lee, escribe un binario y
**llama a `lld-link.exe`**.

> **Estado 2026-09-02:** hechas 031, 028/029/030, 025/026/027 y 032; medidas en verde con los
> dos enlazadores (docs 100, 101, 103). Falta **solo el programa del criterio de cierre**.
> `035` ya no cuenta para cerrar el bloque: salio del roadmap, ver doc 99 §1.

| SHR | Qué | Spec | Estado medido |
|---|---|:--:|---|
| **031** | Filesystem — **siete operaciones**, no «filesystem compiler-grade» en abstracto | **E** | `path` 108 · `remove` 31 · `is_regular_file` 9 · `weakly_canonical` 2 · `absolute` 2 · `file_size` 1 · `current_path` 1. **El 80 % es manipulación de rutas: texto, no sistema de archivos** |
| **028 / 029 / 030** | `File.ReadAllBytes`, `WriteAllBytes`, `GetSize` | **E** | La mitad binaria junto a las de texto que ya funcionan. Pequeño |
| **025 / 026 / 027** | `ByteBuffer`, `BinaryReader`, `BinaryWriter` | **D** | Ausentes. COFF/PE/metadata. ~15 funciones de E/S binaria |
| **032** | **`Process.Run` — crear procesos hijos** | **D** | `Process.ns` existe y su cabecera dice *«crear procesos hijos (CreateProcess) está deliberadamente fuera de alcance de esta ronda»*. Tiene `CurrentProcessId`, `WorkingSetSize`, `KernelTime`, `UserTime`, `PriorityClass`; **ninguna crea un proceso**. `src/Linker/LldLinker.cpp` invoca `lld-link.exe` con `CreateProcessW` + `WaitForSingleObject`. **Sin esto `nsc-next` no enlaza** |
| ~~**035**~~ | ~~`NativeLibrary`~~ | — | **FUERA DEL ROADMAP, no aplazada.** Decidido el 2026-09-02: el flag de enlazador tiene `nslink-inproc`, que enlaza contra `NativeLinkCore.lib` dentro de `nsc`, asi que no hay nada que cargar. **Motivo, los cuatro casos que la justificarian y las condiciones exactas que la reabren: doc 99 §1.** No volver a listarla como pendiente sin mirar esa seccion |

---

## Bloque 5 — Contenedores, algoritmos y memoria

**Depende de:** Bloque 3. Toda esta capa toma `Span<T>` como entrada.
**Terminado cuando:** AST, símbolos y MIR pueden vivir en una arena por compilación.

| SHR | Qué | Spec | Estado medido |
|---|---|:--:|---|
| **052** | `algorithm` — **nueve funciones**, no dieciocho | **E** | `find` 13 · `max` 7 · `min` 5 · `sort` 2 · `any_of` 2 · y una cada uno `reverse`, `remove_if`, `find_if`, `all_of`. **No se usan operaciones de heap, ni `partition`, ni `unique`** |
| **019 / 020** | `StringBuilder` y `TextWriter` | **D** | Ausentes · 6 usos de `<sstream>` |
| **038 / 039** | Allocator abstraction y `ArenaAllocator` | **D** | Ausentes |

> *Cautela heredada:* un primer conteo dio 66 `unique`; son `std::unique_ptr` capturados por el
> patrón. Queda anotado para que nadie lo herede como cifra.

---

## Bloque 6 — Extracción del runtime

**Depende de:** nada de los bloques 2-5. **Es C++ sobre el compilador actual y va en paralelo.**
**Terminado cuando:** el compilador sólo sintetiza EH y el arranque; el resto se enlaza.

| SHR | Qué | Spec | Tamaño medido |
|---|---|:--:|---|
| **089** | Externalizar `WindowsRuntimeBuilder` | **E** | `src/Runtime/WindowsRuntimeBuilder.cpp` = **4957 líneas**, pero **75 funciones `build*`** en ~18 familias, ~66 líneas de media. **~69 salen; EH y `Start` se quedan** |
| **088** | ABI de runtime estable | **D** | — |

**Las 18 familias:** memoria (2) · reloj (3) · consola (2) · debug (3) · directorio (5) · EH (6) ·
entorno (7) · evento (4) · archivo (7) · atómicos (4) · monitor (5) · mutex (4) · rutas (1) ·
proceso (6) · semáforo (3) · info de sistema (6) · hilos (5) · otros (2).

**Cómo se hace:** familia por familia, no de una vez. **Cada familia movida deja la suite verde o no
se mueve.** No es un monolito: son 75 funciones independientes con la suite existente como red.

---

## Bloque 7 — Frontera con LLVM

**Depende de:** nada de 2-5. **También paralelizable.**
**Terminado cuando:** existe un binding NativeSharp versionado de ese subconjunto, y ningún modo de
usuario pasa por el frontend clásico.

| SHR | Qué | Spec | Tamaño medido |
|---|---|:--:|---|
| **090** | Congelar el subconjunto de la API C de LLVM | **E** | `src/LLVM/LlvmCApi.h` = **667 líneas**, **146 símbolos declarados a mano**; ~150 distintos usados en `src/`. **La frontera ya está prácticamente enumerada** |
| **092** | Frontend moderno canónico | **D** | — |

---

## Bloque 8 — Bootstrap

**Depende de:** todos.

| SHR | Qué | Spec |
|---|---|:--:|
| **094** | `nsc0 → nsc1 → nsc2 → nsc3` automatizado | **D** |
| **095** | Reproducibilidad | **D** |

**Métrica de terminado del proyecto entero:** `nsc0` (C++) compila `nsc1`; `nsc1` compila `nsc2`;
`nsc2` compila `nsc3`; **`nsc2` y `nsc3` producen binarios equivalentes** (COFF/PE, IR, conteo de
instrucciones) y la suite sigue verde. No hace falta que `nsc-next` sea más rápido ni más pequeño
que `nsc0`: hace falta que sea **correcto** y **determinista**.

---

## Orden y paralelismo

```
Bloque 1 ──► Bloque 2 ──► Bloque 3 ──► Bloque 4 ──► Bloque 8
                              └─────► Bloque 5 ──►    ▲
Bloque 6 ─────────────────────────────────────────────┤
Bloque 7 ─────────────────────────────────────────────┘
```

**Camino crítico:** 1 → 2 → 3 → 4 → 8. **Independientes desde el primer día:** 6 y 7.

**Un bloque a la vez hasta tener base verde.** La suite es la única red de seguridad — 242 ctest +
71 del checklist de Windows + 1201 del manifiesto — y con dos bloques en vuelo un rojo tiene dos
causas posibles y la suite deja de decir cuál. Los bloques 6 y 7 son la excepción sólo porque no
tocan la biblioteca ni el frontend.

---

## Fuera de los ocho bloques

Está en el registro CC2 (doc 60 §15) y **no lo recoge ningún bloque**. Aquí para que no se pierda,
no para meterlo en el camino crítico.

### Decisiones, no tareas

| ID / SHR | Qué | Qué bloquea |
|---|---|---|
| **SHR-063** | Mecanismo de condicionales de compilación (Debug/Release/arch). **No falta el mecanismo: falta elegir cuál.** El `.nsproj` sólo tiene `Name`/`Compile`/`ProjectReference` y el lexer nunca mira `#`. Candidatos sin elegir: propiedad del `.nsproj` que inyecte constantes · `const bool` de biblioteca más eliminación de rama muerta · atributos (que no existen) | **Bloque 4**, si el driver necesita distinguir configuraciones. Hasta entonces, nada |
| **125** | UTF-8: decidir y escribirlo. Recomendación en pie: `char` es **unidad de código de un byte**, `string` es UTF-8. La alternativa (`char` de 16 bits como C#) **rompe la ABI de todo lo que ya cruza a C**. Si se elige (a), entran los escapes `\u`/`\U` | Nada hoy. `char` mide un byte en el lexer, en `constantFits`, en el layout, en `llvmType` y en la ABI — CC1-100 lo unificó a propósito |
| **122 (a) o (b)** | (a) convertir en el evaluador, 2 líneas — recomendada; (b) cambiar la struct a bits + `typeKey` y reescribir `constantFits`, toca los 10 sitios | **Bloque 1a.** Hay que elegir antes de escribir |

### Aplazado con orden fijo

| ID / SHR | Qué |
|---|---|
| **87** — SHR-075 | `volatile`. **Primero** el contrato del modelo de memoria y la semántica de atómicos (SHR-073/074), **después** la palabra. El propio SHR-075 lo dice: *«no implementar keyword sin contrato»*. Hoy `Atomics.ns` documenta su propio rodeo (`CompareExchange` consigo mismo para leer con garantías); ese rodeo es correcto y caro, y no se quita hasta que exista el contrato |

> **Contradicción a resolver, y no es de ingeniería.** El doc 60 §16 pone el ID 87 como **Fase 1,
> sola y primero**, con este argumento: es la única del registro donde no hay síntoma — el programa
> compila, corre, y da el resultado equivocado un día de cada mil. El doc 62 y el handover lo sacan
> del perfil y lo aplazan a post-bootstrap. **El handover es posterior, pero el cambio de criterio
> no está justificado por escrito en ningún lado.** Decidir antes de cerrar el Bloque 1.

### Abiertos y sin bloque

| ID | Qué | Prioridad CC2 |
|---|---|---|
| **88** | No hay `checked`/`unchecked` | P2 |
| **124** | Fixture de `switch` anidado dentro de `catch` con `continue` | P2 — **pero ver abajo** |
| **91** | Las interfaces no toman parámetros de tipo | P2 |
| **90** | Restricciones genéricas `where` | P2 |
| **77** | Los logs de test no dicen cuántas comprobaciones corrieron | P2 |
| **92** | Tipos anidados | P3 |
| **93** | Constructores estáticos | P3 |
| **84** | Lambdas anidadas | — |
| **85** | Delegado a método virtual | — |
| **86** | Delegado con receptor struct | — |
| **A-002** | Frontera backend-neutral, por extracción | P3 |
| **P-001** | Retirar el mini-parser de type seeds | P3 |

**El ID 124 probablemente hay que cerrarlo como falso.** El doc 67 lo midió:
`tests/cc1/c8_loop_catch_switch` **existe**, se titula «CC1 EH-002 — `bucle -> catch -> switch` con
`break` local y `continue` al bucle exterior», y explica en su cabecera por qué mide lo que sigue
*después* del switch. Más `c4_catch_continue`, `c5_catch_break`, `c6_catch_return`,
`c7_finally_switch`. El error nació en el doc 61 y se propagó a los docs 63, 65 y 66.

**Los IDs 84/85/86, 90/91, 88 y 92 están en la Lista C** — fuera del bootstrap por decisión. Siguen
en el registro porque son huecos reales del lenguaje, no porque haya que cerrarlos antes de `nsc3`.

---

## Cómo se mide el Bloque 1a

Fixture `tests/cc1/c17_constant_promotion`, en la forma de las demás: `namespace cc1<nombre>`,
`class Program`, `public static int Main()`, un `if (…) { return N; }` por propiedad, `return 42` al
final. Verde a `-O0` y `-O2`.

| Caso | Espera | Qué separa |
|---|---|---|
| `const long a = 1 + 1L;` | se pliega, `long` | el fallo del 121, directo |
| `const ulong b = 3L + 5UL;` | `ulong` | la parte sin signo de la regla |
| `short s = 3; short t = s + 3;` | compila, `short` | **regresión del ID 120** — si rompe, la regla está en el orden equivocado |
| `const long c = (-1) + 1L;` | `long`, vale 0 | que un literal escrito ancho no se estreche |
| `byte b1, b2; b1 + b2` | `int` | promoción de estrechos |
| `short s2 = -3;` cabe | compila | **regresión del ID 119**, que es lo que el 122 puede romper |
| shift con contador estrecho | tipo del operando izquierdo | **regresión de CC1-103**, que ya estaba bien |

**Doble medición:** el valor en ejecución **y** el conteo en el IR de que cada `const` es una carga
inmediata y no un cálculo.

**Regresión obligatoria:** `tests/cc1/c1_constants` y `tests/cc1/c9_sizeof` verdes a O0 y O2, y los
242 de la suite.

**Cuidado con el mínimo con signo en el 122:** la magnitud se calcula sobre el gemelo sin signo
(`0 - bits`), porque negar `INT64_MIN` con signo es comportamiento indefinido. El unario `-` del
evaluador ya lo hace así (`ProjectIntegerEvaluator.cpp:202`) y su comentario lo dice.

---

## La regla que decide si este plan vale algo

> **Ninguna verificación cuenta si el modelo contra el que verifico lo construí yo.**

Cada afirmación de este documento nombra la capa que decide y la línea que lo dice. Un inventario
hecho leyendo etiquetas `case` en vez del cuerpo del `case` ya salió falso dos veces. Un plan
escrito sobre una descripción en vez de sobre una medición ya salió falso una vez.

**El defecto recurrente del árbol tiene nombre: la mitad delantera existe y la trasera no.** Una
forma parsea, tipa, y muere en el lowering con un mensaje que habla de etapas internas. Se ha
cerrado cuatro veces: `sizeof`, `stackalloc`, `new T`, aritmética de punteros.
