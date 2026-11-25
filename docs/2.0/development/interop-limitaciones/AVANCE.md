# Avance: limitaciones de interoperabilidad C y C++

Plan: [plan-implementacion-limitaciones-interop-para-claude.md](../plan-implementacion-limitaciones-interop-para-claude.md).
Repositorio `A:\nsc-devel`, rama local `interop-sdk-tandas`. Sin push.

Regla de trabajo acordada con el usuario (2026-11-22): una tanda a la vez, en el orden del plan; no se abre la
siguiente sin cerrar la actual. Claude implementa y compila `nsc`; el usuario valida en la otra máquina. Clang/LLVM,
NSLink, NSMeta, el SDK y las bibliotecas externas no se modifican.

## P0 — Base y manifiesto

### P0-01. Estado y entorno

| Elemento | Valor |
|---|---|
| Repositorio | `A:\nsc-devel`, rama `interop-sdk-tandas`, HEAD `1aec52cfdbfad79596af73d7cf2fc37ad0ed0f71` (= `development` local) |
| Cambios locales al empezar | ninguno (`git status` limpio) |
| Build de trabajo | `A:\nsc-devel\build-integracion`: Ninja, Release, `clang-cl` de `C:\clang-llvm` (LLVM 23.1.0), `NSC_ENABLE_CPP_INTEROP=ON`, `NSC_BUILD_TESTS=OFF`, `NSC_HOST_LINKAGE=STATIC`, `NSC_HOST_OPTIMIZATION=O2` |
| Compilador bajo prueba | `A:\nsc-devel\build-integracion\bin\nsc.exe` |
| Clang de interop y oráculo | `A:\al82dev-clang` (clang 23.1.0, `ea7d852a7`), target `x86_64-pc-windows-msvc` |
| Linker | `A:\nslink-devel\bin\release\nslink.exe` (variable `NSC_LINKER`) |
| Runtime | `NativeSharp.Runtime.Win32.lib` generado por el build (`NSC_RUNTIME_LIB`) |
| Core para fixtures | Se reutiliza la compilación existente de Core (`NSC_CORE_ROOT`) en lugar de reconstruirla: AGENTS.md no autoriza reconstruir Core sin causa demostrada |
| Triple | `x86_64-pc-windows-msvc`, ABI Microsoft x64 |

### P0-02. Manifiesto de casillas

El documento fuente que cita el plan, `limitaciones-interop-nsc-devel.md`, **no existe** en `A:\trabajo-interoperabilidad`
ni en el repositorio. Se usa como fuente el texto vigente de cada casilla en `A:\nsc-devel\nativesharp-interop\TRACKING.md`
(todas siguen `[ ]`). La relación casilla → tareas es la matriz del plan; ninguna casilla queda sin tarea. Las 89
filas y el índice inverso tarea → casillas están en [manifiesto-tabla.md](manifiesto-tabla.md).

- 73 casillas cotejadas y 16 adicionales; todas con al menos una tarea.
- Las 108 tareas del plan existen; la matriz no cita ninguna tarea inexistente.
- Casillas con partes fuera de esta autorización (se registran, no se simulan): T19-032 (Clang 23.1), T19-034
  (directorio TLS de NSLink), T19-022 (empaquetar herramientas ajenas), T16-016 (si exigiera NSLink).
- Casillas de línea base y regresión (T18-014, T19-027, T19-037) se reconcilian en V, no en una tanda funcional.
- El tracking histórico no se modifica; el estado nuevo se registra aquí.

### P0-03. Arnés y fixtures existentes

`nativesharp-interop/interop-sdk/comun.py`:

| Función | Modos | Niveles | Notas |
|---|---|---|---|
| `positivo` | directo; `.lib` (método genérico público, consumidor sin header); DLL (consumidor por import library) | O0–O3 | El cuerpo devuelve 42 |
| `rechazo` | directo | solo O0 | Exige fallo y texto |
| `diagnostico` | directo | O0–O3 | Texto presente y si compila |
| `ir` | directo | uno (O1 por omisión) | IR optimizado para comparar con Clang |

Exclusiones conocidas: las bibliotecas C++ de prueba se compilan con `-fno-exceptions -fno-rtti` y sin CRT salvo
que el caso pida otra cosa; `rechazo` no recorre O1–O3; no hay modo productor/consumidor para `diagnostico`.
Fixtures existentes: `tanda01_diagnosticos.py` … `tanda17_18_runtime.py` (17 archivos). Los resultados de
`implementacion-plan/AVANCE.md` son históricos; no se toman como ejecución del HEAD actual.

Fixtures nuevas de este plan: `nativesharp-interop/interop-sdk/limitaciones_<tanda>.py`, con el mismo arnés.

### P0-04. Hipótesis y controles

Cada tarea marcada `HIPÓTESIS` arranca con una reproducción. Se define al abrir su tanda:

| Tarea | Entrada | Ruta a trazar | Resultado esperado | Control |
|---|---|---|---|---|
| C2-01 | macro que expande a varios argumentos (`#define PAR 1, 2` usado como `f(PAR)`) | `ClangSession.cpp`, expansión de macros de valor y su llamador | rechazo explícito; nunca solo la última expresión | Clang compila `f(PAR)` con dos argumentos |
| C3-01 | `_Float16` en parámetro y retorno | plan ABI de Clang → HIR/MIR → `OopLlvm*` | mismos registros que Clang (XMM) | IR de Clang Win64 de la misma firma |

Las demás tareas son `CONFIRMADO EN CÓDIGO` o `REPORTADO`; antes de cambiar se revisa el HEAD (las líneas del plan
son orientativas).

### P0-05. Dependencias y decisiones abiertas

- Orden del plan: C1 → C2 → C3 → C4 → C5 → X1 → X2 → X3 → X4 → R1 → R2 → R3 → V.
- R1 (contratos de ownership) condiciona C4-02/C4-03, X2-03 y R1-03: mientras no exista un contrato acordado, esas
  rutas conservan el rechazo con diagnóstico, como pide el plan.
- R2: cualquier cambio de formato NSMeta requiere autorización; si una tarea lo necesita, se especifica y se detiene
  solo ese subcaso.
- Rutas compartidas (`ClangSession.cpp`, `CppSemanticFunction.cpp`, `CallAnalyzer.cpp`): antes de cambiarlas se
  inventarían consumidores y se corre la regresión de las tandas afectadas.

**Gate P0:** manifiesto de 89 relaciones rastreables, entorno fijado, harness inventariado. No requiere cambios al
compilador. **Estado: cerrado (2026-11-22).** `nsc` compila en `build-integracion` sobre `1aec52cfd` sin cambios
(`ninja nsc nsc_windows_runtime_import_libraries`, exit 0).

## C1 — Calificadores y referencias C

Decisión de diseño (pedido del usuario, 2026-11-22: "añade ese tipo de dato o feature", "resuelve sin workarounds"):
NativeSharp gana calificadores por nivel de puntero en lugar de descartar o rechazar el calificador.

- **Base de la cadena:** marcas existentes del apuntado (`T const**`, T08-D17) y ahora también `T volatile**`, escrito
  en código fuente (T08-D03 lo limitaba a metadata).
- **Niveles intermedios:** máscara nueva de dos bits por nivel (`const`, `volatile`) para hasta tres punteros
  intermedios (`int* volatile*`, `char const* const*`), en
  `include/nativesharp/Language/PointerLevelQualifiers.h`. Un cuarto nivel calificado da diagnóstico.
- **Conversiones:** regla de C (C11 6.5.16.1): ningún nivel pierde un calificador y solo el nivel 0 puede ganarlo.
- **NSMETA:** el byte de calificadores del apuntado usa los bits 2–7 para los niveles intermedios. El contenido
  existente no cambia (los bits valen cero). Cambio de contenido de NSMeta hecho por la autorización explícita del
  usuario para añadir la capacidad.

| Tarea | Resultado | Commit |
|---|---|---|
| C1-01 | `volatile int**`, `const volatile int**` en parámetros, retornos, campos C, globales C y callbacks C | `1c1fbc479` |
| C1-02 | Máscara por nivel; `int* volatile*` y `int* const*` se conservan; `&p` de un puntero calificado es representable | `1c1fbc479` |
| C1-03 | Captura y proyección en funciones, retornos, campos, globales, callbacks y alias C (`VolatileIntPtr*` → `int volatile**`) | `1c1fbc479`, `2e46dd615` |
| C1-04 | `*q`, `q[i]`, `&p`, `?:`, `??`, casts, locales `var`; HIR emite cargas/escrituras volatile por nivel | `1c1fbc479` |
| C1-05 | Metadata, superficie pública materializada y snapshot precompilado conservan los niveles | `5307b2e97` |
| C1-06 | `limitaciones_c1.py`: 12 positivos (O0–O3 × directo/.lib/DLL), 5 rechazos, 8 de API NativeSharp por metadata (.lib y DLL, O0/O2) e IR | `2e46dd615` |

Defecto previo encontrado y corregido en C1: el decaimiento de arreglos fijos del ítem 4.7 perdía el `const`
de los elementos (`c_mutate(c_fixed_slots)` con `int* const[2]` hacia `int**` se aceptaba también en `1aec52cfd`).
Ahora el arreglo da sus calificadores de elemento al nivel 0.

Pruebas existentes actualizadas porque fijaban la limitación anterior (mismo criterio que la decisión del usuario
sobre `punteros-c` en la tanda 2):

- `tanda02_calificadores.py`: el rechazo 2.7 de `volatile int**` pasa a ser una llamada positiva.
- `pruebas-tanda-08-punteros-c/validar-punteros.py`: `02c-no-representado-volatile` pasa al cuerpo como llamada.
- `pruebas-tanda-08-record-c/validar.cmd`: `&` de una global alias `volatile int*` y `VolatileIntPtr*` compilan; el
  local `int** address = &value` sigue fallando, ahora por la conversión.
- `pruebas-tanda-08-arreglos-c`, `pruebas-tanda-08-punteros-c/validar-puntero-a-puntero-const.py`,
  `pruebas-tanda-08-global-puntero-const`: `&` de un puntero de solo lectura es `int* const*`; el rechazo pasa a la
  escritura o a la conversión a `int**`.
- `tests/unit/CppClangSessionTests.cpp`: `ptr_volatile_nested` se proyecta con la marca volatile de la base (no se
  compiló aquí: `NSC_BUILD_TESTS=OFF`).

**Gate C1 (2026-11-22): cerrado.** Con el binario de `2e46dd615`: `limitaciones_c1.py` 26/26, `tanda02_calificadores.py`
27/27, `tanda04_records.py` 12/12, y los validadores T08 `punteros-c` (punteros y puntero a puntero const),
`arreglos-c`, `record-c` y `global-puntero-const` pasan (copias con rutas al build actual en `A:\temp\c1-t08`).
Bundle: `A:\temp\limitaciones-interop.bundle` (`1aec52cfd..interop-sdk-tandas`).

## C2 — Macros y preprocesador

| Tarea | Resultado | Commit |
|---|---|---|
| C2-01 | Confirmado: una macro lista (`"$clr.diffuse", 0, 0`) se descartaba; nunca se toma solo el último valor | `1b7ff23c5` |
| C2-02 | Cada elemento se evalúa como macro propia (`<M>__nsc_arg<i>`) y un argumento de llamada que nombra la macro equivale a todos, en orden (cadenas, paréntesis anidados, variádicas). Fuera de una llamada: diagnóstico. Una unidad que declara el nombre conserva la búsqueda NativeSharp | `1b7ff23c5` |
| C2-03 | Macros entre llaves (`LZ4F_INIT_PREFERENCES`, `DBUS_ERROR_INIT`): Clang evalúa `static const T v = M;` por cada record del encabezado; un sondeo con `#line` descarta los pares que rechaza. `T v = M;` toma el valor de su tipo; otros usos o tipos rechazados dan diagnóstico con los tipos válidos | `036f1baa2` |
| C2-04 | Macro de función en la dimensión de un arreglo: Clang la evalúa como expresión constante entera (`evaluateIntegerConstantExpression`); una expansión no constante deja el arreglo inválido | `1efe84ddb` |
| C2-05 | Argumentos clasificados por efectos desde el AST: un argumento puro se acepta aunque la macro lo evalúe varias veces o condicionalmente; llamadas, incrementos, asignaciones y lecturas `volatile`/atómicas se rechazan (antes una global `volatile` pasaba) | `c75b63007` |
| C2-06 | `#__VA_ARGS__` y `a##__VA_ARGS__` como tokens; `__FILE__`/`__LINE__` con el sitio de la llamada NativeSharp (`#line` por sitio); `__func__` rechazado con motivo; tipo de resultado de ámbito de bloque deducido por sondeo. Defecto previo corregido: `(int)` sobre `void` llegaba al backend | `21fb05012` |
| C2-07 | La accesora de una macro de valor escribía en una variable `static` compartida entre hilos; ahora escribe en la pila de quien lee | `4fe69c664` |
| C2-08 | Constantes de macro y listas dentro de plantillas precompiladas: el productor publica su fuente de macros cuando importó macros de objeto y la sesión del consumidor las evalúa | `1b7ff23c5` |
| C2-09 | Alias de función (`#define compress LZ4_compress_default`); `IncludeDirectory Kind="User"` importa macros, `Kind="System"` conserva la política de sistema | `53def8d92` |

Fixture: `nativesharp-interop/interop-sdk/limitaciones_c2.py` (O0–O3 × directo/.lib/DLL y rechazos).

**Gate C2 (2026-11-22): cerrado.** Con el binario de `53def8d92`: `limitaciones_c1` 26, `limitaciones_c2` 109,
`tanda01_diagnosticos` 36, `tanda02_calificadores` 27, `tanda03_escalares` 37, `tanda04_records` 12,
`tanda05_macros` 15, `tanda17_18_runtime` 13; ninguna falla.

## C3 — Tipos y ABI C

| Tarea | Resultado | Commit |
|---|---|---|
| C3-01 | Hipótesis confirmada: `_Float16` no se mapeaba. Ahora es `NativeSharp.Core.Numerics.Half` (binary16); por valor viaja como `half` de LLVM en XMM, como Clang. Un cast explícito convierte entre Half y números (antes fallaba en el backend, también en NativeSharp puro). Límites: callback con `_Float16` sin mapear; Half no pasa a `...` implícitamente | `7215dd3a8`, `b575dc4a0` |
| C3-02 | `_BitInt(N)` conserva N (también en el catálogo precompilado) y viaja como `iN`: argumento truncado, resultado extendido por el signo del portador (`u12_add(4000,100)` leía 4100). Límites: 64 < N < 128 (Clang los pasa en memoria) y callbacks con `_BitInt` estrecho sin mapear, con diagnóstico | `f6441bc03` |
| C3-03 | Enums C23 de base fija 8–64 bits ya cubiertos. Un enum sobre `__int128` invalidaba todo el proveedor de tipos; ahora sus posiciones son el entero de 128 bits (plan de `__int128`) y sus enumeradores llegan como valores de macro exactos | `05051c3dc` |
| C3-04 | Campos `_Atomic T`: cargas `seq_cst`, escrituras atómicas, `+=`… por las rutas RMW existentes; `&s->n` solo enlaza `_Atomic T*`; sin `ref/in/out` | `23d9f46ab` |
| C3-05 | Un bit-field accedido por almacenamiento `volatile` (`Regs volatile*`, MMIO) usa la unidad de Microsoft (32 bits) como Clang; los demás accesos conservan la ventana mínima. La ruta de 128 bits no cambia | `26df067cf` |
| C3-06 | Parámetros `transparent_union` de punteros: se pasan como el primer miembro y aceptan argumentos de cualquier miembro; otro tipo se rechaza. Los miembros viajan en un grupo final de la metadata precompilada | `13a5115b7` |
| C3-07 | Arreglos flexibles de arreglos (`int rows[][4]`): filas con su propio límite (`rows[0][4]` rechazado) y decaimiento a `int (*)[4]`. `T x[0]` final sigue flexible; `T x[1]` final sigue siendo arreglo de un elemento | `34d2360cd` |
| C3-08 | Punteros a función sin prototipo (`int (*)()`, `FARPROC`): toman cualquier argumento con las promociones de C (validador FARPROC O0–O3 pasa). Funciones C `static inline` `sysv_abi` con records conservan el plan de Clang. Defecto previo corregido: la metadata precompilada rechazaba un record formado por escalares (`P make(int,int)`), también en funciones externas. Otra convención sin prototipo sigue sin mapear | `53cf3cff4` |
| C3-09 | `atomic_thread_fence`/`atomic_signal_fence` de `<stdatomic.h>` (sin definición en biblioteca alguna) pasan a ser `AtomicFence`: `consume`→`acquire`, `relaxed`→barrera del compilador, señal→`singlethread`. Orden constante obligatorio; ninguna otra macro de sistema cambia. Alternativa `AtomicFence.Thread/Compiler` verificada | `9805a8e47` |
| C3-10 | Campos vector en records C (agregados vectoriales): vistas de carriles con layout de Clang; HVA por XMM0-3 con `__vectorcall` y por dirección con cdecl, forma de IR igual a Clang. Punteros `__vectorcall`/cdecl con records ya correctos. 256 bits verificado con AVX (`validar-vectores-c.py --avx --dll`, O0–O3). 512 bits conserva el rechazo (sin portador ni ABI demostrado) | `d414c804b` |

Fixture: `nativesharp-interop/interop-sdk/limitaciones_c3.py` (O0–O3 × directo/.lib/DLL, rechazos y comprobaciones de IR).

Pruebas existentes actualizadas porque fijaban la limitación anterior:

- `tanda01_diagnosticos.py`: `t1-campo-omitido` usaba `_Float16` como tipo sin mapeo; ahora es lectura positiva con
  cast, y el diagnóstico de campo omitido se conserva con `_BitInt(100)`.

Observación sin cambio: en la copia del validador `pruebas-tanda-08-arreglos-c/validar-arreglos.py`, el rechazo
`04c-elemento-subalineado` espera el texto "addressable storage"; desde `90aeb4e5e` (anterior a la rama) el mensaje
de `&` sobre un elemento indexado sin dirección es "has no address". No es efecto de C3 y la prueba no se modificó.

**Gate C3 (2026-11-22): cerrado.** Con el binario de `d414c804b`: `limitaciones_c1` 26, `limitaciones_c2` 109,
`limitaciones_c3` 164, `tanda01_diagnosticos` 48, `tanda02_calificadores` 27, `tanda03_escalares` 37,
`tanda04_records` 12, `tanda05_macros` 15, `tanda06_enlace` 36, `tanda07_callbacks` 13, `tanda08_contratos` 15,
`tanda09_captura_cpp` 12, `tanda17_18_runtime` 13; ninguna falla. Además: validadores T08 `record-c`, `bitfields-c`
(6), `funciones-c` (3), `arreglos-punteros-campo`, FARPROC O0–O3 y `validar-vectores-c.py --avx --dll` O0–O3.
Límites registrados: `_BitInt` 64<N<128, callbacks con `_Float16`/`_BitInt` estrecho, vectores de 512 bits, punteros
sin prototipo de convención no C, orden no constante en barreras C11; todos con diagnóstico.

## C4 — Callbacks y variádicas C

| Tarea | Resultado | Commit |
|---|---|---|
| C4-01 | Una lambda escrita como argumento de una función C/C++ importada (un candidato) en una posición puntero a función toma ese tipo: parámetros y resultado de la firma C, también con cuerpo de bloque, y llega al puntero por su thunk. Antes exigía tipos explícitos y cuerpo de expresión. Funciones y métodos estáticos NativeSharp también son callbacks. Un estado capturado sigue rechazado; el mensaje nombra la regla y el canal `user_data`. El puntero C nunca es la representación de delegado | `677578670` |
| C4-02 | Par `callback + user_data` verificado: registro, invocación posterior, desregistro y pares asignador/liberador con `opaque`. La vida del estado es la del almacenamiento que da el programa; capturas implícitas quedan para R1 | `677578670` |
| C4-03 | Un método de instancia se rechaza ("a C function pointer has no receiver"); el receptor se pasa explícitamente por `user_data`. Sin estrategia de vida aprobada (R1) no se habilita otra conversión | `677578670` |
| C4-04 | Inventario: un cuerpo NativeSharp no puede ser variádico (diagnóstico); `va_list` es `char*` en Win64 y pasa de un callback C a otra función C; las llamadas variádicas aplican las promociones de C y pasan records por valor como Clang (8 bytes por valor, 24 por copia, también en DLL); un callback NativeSharp detrás de un puntero variádico recibe sus fijos. Crear un `va_list` desde NativeSharp (`va_start`/`va_copy`) no existe: requiere un cuerpo variádico | `677578670` |
| C4-05 | Asignador/liberador de ida y vuelta con contexto retenido. Una excepción no cruza la frontera: lanzarla en un callback sin capturarla es error de compilación; un `try` dentro del callback la contiene. El contrato de memoria de la biblioteca no se vuelve propiedad NativeSharp | `677578670` |

Fixture: `nativesharp-interop/interop-sdk/limitaciones_c4.py` (56 casos, O0–O3 × directo/.lib/DLL y rechazos).
Además: `pruebas-tanda-15-callbacks/validar.py --dll` pasa (O0–O3); su texto "must not capture state" y el de
`tanda07` "cannot capture state" se conservan en el mensaje nuevo.

**Gate C4 (2026-11-22): cerrado.** Con el binario de `677578670`: `limitaciones_c1` 26, `limitaciones_c2` 109,
`limitaciones_c3` 164, `limitaciones_c4` 56, `tanda01` 48, `tanda02` 27, `tanda03` 37, `tanda04` 12, `tanda05` 15,
`tanda06` 36, `tanda07` 13, `tanda08` 15, `tanda09` 12, `tanda17_18` 13; ninguna falla. Firma (C4-01) y vida del
callback (C4-02/03, R1) se verifican por separado; ninguna captura implícita se habilita.

## C5 — Casos C adicionales

| Tarea | Resultado | Commit |
|---|---|---|
| C5-01 | Literales hacia `char*`, `signed char*` y `unsigned char*`: primer byte, vacío, nulos embebidos y puntero retenido por C (literal estático) correctos; sin defecto reproducido | `dab7f49c4` (fixture) |
| C5-02 | `_Generic`: un `sbyte` devuelto por C como `signed char` conserva ese tipo; un `sbyte` NativeSharp sin origen C sigue siendo `char` (item 5.5). Defecto corregido: dos envoltorios de la misma macro con igual firma NativeSharp se sombreaban; cada llamada enlaza el suyo. Límite: un local `sbyte` pierde el origen C (NativeSharp tiene un solo tipo de 8 bits con signo) | `dab7f49c4` |
| C5-03 | `extern int a[]; extern int a[4];` importaba `int[*]`; ahora el tipo compuesto C11 `int[4]` en ambos órdenes (longitud y límites) | `8c4076125` |
| C5-04 | Bit-fields en `...`: `unsigned :3`/`int :5` → `i32` (zext/sext), `unsigned long long :40` → `i64`; comprobado en el IR | `dab7f49c4` (fixture) |
| C5-05 | `setjmp`/`returns_twice` sigue rechazado con su motivo; `va_list` como paso transparente (C4-04); `va_start`/`va_copy` desde NativeSharp no existen (sin cuerpos variádicos) | sin cambio |
| C5-06 | Nombres de etiqueta y ordinarios iguales (`enum mode` y `typedef int mode`): las llamadas funcionan; nombrar el tipo desde NativeSharp da diagnóstico sin ambigüedad ("conflicts with a native type"). `_Complex` y enum C23 con nombres de records vecinos funcionan | `dab7f49c4` (fixture) |
| C5-07 | Funciones `static inline` `sysv_abi` con records: llamada (C3-08) y uso como callback C, directo y precompilado. Una lambda o método NativeSharp hacia un puntero `sysv_abi` sigue rechazado con diagnóstico (requiere thunk SysV con la inversa de cada clase de plan; sin ABI demostrado) | `dab7f49c4` (fixture) |
| C5-08 | `Implementation="MACRO"` con encabezados de sistema (`stddef.h`, `stdint.h`) y otro encabezado propio, en ese orden; sin cambios al mecanismo ni detección de SDK | `dab7f49c4` (fixture) |

Fixture: `nativesharp-interop/interop-sdk/limitaciones_c5.py` (101 casos).

Nota: el encabezado de `tanda06_enlace.py` aún lista 6.4/6.5 (`static` `sysv_abi` con records) y 6.11 (puntero sin
prototipo con argumentos) como limitaciones; C3-08 las resolvió (cubiertas en `limitaciones_c3.py`). La prueba no se
modificó.

**Gate C5 (2026-11-22): cerrado.** Con el binario de `dab7f49c4`: `limitaciones_c1` 26, `c2` 109, `c3` 164, `c4` 56,
`c5` 101, `tanda01` 48, `tanda02` 27, `tanda03` 37, `tanda04` 12, `tanda05` 15, `tanda06` 36, `tanda07` 13,
`tanda08` 15, `tanda09` 12, `tanda17_18` 13; ninguna falla. Cada caso tiene control frente a Clang o exclusión con
diagnóstico (sbyte local sin origen C, lambda hacia `sysv_abi`, `setjmp`, colisión etiqueta/nombre ordinario).

## X1 — Identidad, sobrecargas y referencias C++

| Tarea | Resultado | Commit |
|---|---|---|
| X1-01 | Alias `using`/`typedef` nombran el mismo tipo (construcción y paso por `in`); una declaración adelantada es tipo opaco por puntero. Sin defecto | `a6118d5f6` (fixture) |
| X1-02 | Familias por aridad y tipo correctas. Restricción consciente registrada: con dos conversiones se conserva el orden NativeSharp (T10-009, revertido el ranking en `536baf9e4`); la constante `g(5)` elige `g(unsigned)` donde C++ sería ambigua | `a6118d5f6` (fixture) |
| X1-03 | Defecto corregido: una sobrecarga `= delete` no participaba (métodos: `s.m(4)` llamaba `m(double)` en silencio) o llegaba al enlazador (funciones libres: `h(4)` era una función `void`). Ahora participa y la llamada que la elige se rechaza; viaja en la metadata precompilada. `explicit`, acceso privado y miembros privados con diagnóstico | `a6118d5f6` |
| X1-04 | `T&` (ref), `const T&` con `in` o con un valor sin `in` (convertido y materializado para la llamada, como C++ enlaza un temporal), `T&&`, retorno por referencia y referencia a campo. Un record C++ para `const T&` sigue enlazando su objeto con `in` (copiarlo es X2-04) | `a6118d5f6` |
| X1-05 | Receptores `&` y `const &` correctos; `&&` proyectado y llamado sobre un temporal (`(new lib.Acc()).take()`), rechazado con motivo sobre un objeto con nombre, como en C++ | `a6118d5f6`, `762b31fcb` |
| X1-06 | Funciones libres, `= delete` y referencias iguales en directo, biblioteca precompilada sin encabezado y DLL. Construir un record C++ con `new` dentro de una plantilla precompilada sigue exigiendo vida de objeto (X2) | `a6118d5f6` (fixture) |

Fixture: `nativesharp-interop/interop-sdk/limitaciones_x1.py` (28 casos). Validadores además verdes:
`pruebas-tanda-10-integral`, `-acceso`, `-operadores`, `-referencias`, `pruebas-tanda-11-metodos`,
`pruebas-tanda-12-operadores-miembro`, `tanda09_captura_cpp`.

**Gate X1 (2026-11-22): cerrado.** Con el binario de `a6118d5f6`: `limitaciones_c1..c5` 26/109/164/56/101,
`limitaciones_x1` 28, `tanda01..09` 48/27/37/12/15/36/13/15/12, `tanda17_18` 13; ninguna falla. Selección positiva,
ambigua (orden NativeSharp, T10-009), eliminada e inaccesible con control; alias y mutabilidad de referencias
coinciden con Clang.

## X2 — Clases, vida de objetos y operadores C++

| Tarea | Resultado | Commit |
|---|---|---|
| X2-01 | Inventario: construcción local con destructor registrado (Item 10.1, guarda de locales C++ en `ProjectCompilation`); heap/raw `new` de objetos C++ por fábricas de la biblioteca (T11 propiedad); por valor y copias, ver X2-04/05 | — |
| X2-02 | Construcción local y destrucción exactamente una vez al salir del bloque y en retorno temprano (contadores) | `86973c777` (fixture) |
| X2-03 | Sin contrato de propiedad para `new` heap de NativeSharp sobre tipos C++: se conserva el rechazo; la memoria de la biblioteca la libera la biblioteca (T11 propiedad). Decisión para R1 | — |
| X2-04 | Copia explícita `new T(in o)` (constructor de copia, contadores); move-only `new T(ref m)` con el origen movido destruido. Defecto corregido: `new T(in m)` enlazaba el argumento de solo lectura al `T&&` del constructor de movimiento. Se mantiene T11-016 (con copia presente el de movimiento no se proyecta: NativeSharp no sobrecarga ref frente a in) | `86973c777` |
| X2-05 | Un record C++ no trivial por valor como parámetro pasa por un adaptador generado por Clang (`const T&`, una copia que destruye el llamado; `T&&` si solo se mueve). Devolverlo por valor queda rechazado con motivo: requiere construcción en el destino (la guarda de locales C++ solo acepta constructores); diseño: adaptador `void a(T* out, …) { ::new (out, place) T(f(…)); }` + construcción en sitio del local con su destrucción | `90cf2a11c` |
| X2-06 | Un struct NativeSharp con campo de tipo C++ no trivial se rechaza con diagnóstico ("has no imported object layout yet"); no se trata como `memcpy` | — (sondeo) |
| X2-07 | `operator[]` lectura/escritura sobre el objeto; receptor const según item 11.1 | — (sondeo y validadores T10/T12) |
| X2-08 | `+=` con resultado `T&`, `++`/`--` prefijos, `*o` (`T&` asignable), `o(args)` (`operator()` con receptor por referencia) y `==`, sobre el objeto en su lugar sin copias | `ec841649e` |
| X2-09/X2-11 | Constructores de plantillas y tamaños ocultos: sin cambio en esta tanda; dependen de X4/R2 (diagnóstico existente) | — |
| X2-10 | Una unión con miembro no trivial y constructor/destructor propios se construye y usa (la unión gestiona su miembro activo) | — (sondeo) |

Fixture: `nativesharp-interop/interop-sdk/limitaciones_x2.py` (contadores de constructor, copia, movimiento y destructor;
directo y DLL; una plantilla precompilada aún no transporta un record C++ no trivial — catálogo, R2).

**Gate X2 (2026-11-22): cerrado.** Con el binario de `90cf2a11c`: `limitaciones_c1..c5` 26/109/164/56/101, `x1` 28,
`x2` 15, `tanda01..09` 48/27/37/12/15/36/13/15/12, `tanda17_18` 13; además `tanda10..16` 13/12/12/8/9/12/17, `x3` 27 y
`x4` 10, y los validadores T10/T11/T12 citados; ninguna falla. Contadores de constructor, copia, movimiento y destructor
verifican cada secuencia aceptada; los casos no triviales no soportados tienen diagnóstico.

## X3 — Herencia, dispatch, RTTI y COM

| Tarea | Resultado | Commit |
|---|---|---|
| X3-01/X3-02 | Bases, offsets distintos de cero, ajuste de `this`, conversión derivado→base (puntero y referencia): correctos; derivar una clase NativeSharp de una C++ sigue rechazado con diagnóstico | `f8ca18546` (fixture) |
| X3-03 | Un record con bases virtuales construye y destruye por adaptadores que compila Clang (`new (self) T(...)` con el indicador de más derivado de Microsoft, `self->~T()` con el destructor de bases virtuales), explícitos o implícitos | `27ac43d3b` |
| X3-04 | Dispatch virtual desde NativeSharp correcto, también con la base polimórfica no primaria; override implementado en NativeSharp (dirección entrante) sin contrato de vtable/propiedad: diagnóstico | `f8ca18546` (fixture) |
| X3-05 | `typeid`/`dynamic_cast` en la biblioteca que enlaza su CRT (T12 `--dll`); NativeSharp no tiene sintaxis de cast dinámico sobre tipos C++ y no se sustituye por un ajuste estático | — |
| X3-06 | Punteros a miembro rechazados con motivo (T14-007: su representación Win64 depende del modelo de herencia) | `f8ca18546` (fixture) |
| X3-07 | COM consumido por su vtable (`__stdcall`, AddRef/Release/QueryInterface, conteos correctos, directo y DLL). Interfaz entrante implementada en NativeSharp: pendiente con diagnóstico (mismo motivo que X3-04) | `f8ca18546` (fixture) |

Fixture: `nativesharp-interop/interop-sdk/limitaciones_x3.py` (27 casos). `pruebas-tanda-12-herencia` (estática y `--dll`)
y `pruebas-tanda-12-valores` pasan. Nota de entorno: el arnés enlaza la biblioteca C++ sin CRT; las clases con vtable
emitida por NSC desde el encabezado requieren el runtime C++ (RTTI de `type_info`, `operator delete` dimensionado,
estáticos locales seguros entre hilos), lo que corresponde a R3.

**Gate X3 (2026-11-22): cerrado** con la corrida de X2 (mismo binario): layout, offsets, dispatch y COM frente a Clang;
RTTI/dispatch entrante/punteros a miembro con diagnóstico.

## X4 — Plantillas y entidades C++ persistentes

| Tarea | Resultado | Commit |
|---|---|---|
| X4-01/X4-02 | Deducción de funciones plantilla con distintos tipos, plantilla de clase con constructor y método, argumentos no tipo explícitos (incluido `unsigned long long` > INT64_MAX, tanda13): cada especialización es la de Clang | `0e8a5839a` (fixture) |
| X4-03 | Especialización parcial elegida por Clang; una restricción `requires` incumplida da el diagnóstico de Clang ("cannot deduce"), sin candidato reaparecido | `0e8a5839a` (fixture) |
| X4-04 | NTTP enteros y constantes `constexpr` (T13-021/022). Plantillas de variable (13.7): `name<args>` sin llamada es valor si el token tras `>` no continúa una comparación (conjunto de C#) o si un argumento es un tipo predefinido; Clang evalúa la especialización y un literal entero o bool la reemplaza. Decisión: solo plantillas `constexpr` enteras/bool; las demás (double, `inline` no constante) se rechazan con motivo. Sintaxis verificada sin cambios en todos los proyectos de `tests/` | `ffa7672a3` |
| X4-05 | Un struct NativeSharp como argumento de una plantilla de clase C++ se rechaza con diagnóstico; un espejo con layout, constructores y destructor requiere acuerdo de representación (13.9) | `0e8a5839a` (fixture) |
| X4-06 | Desde una DLL las especializaciones funcionan. Una plantilla NativeSharp precompilada cuyo cuerpo usa plantillas C++ no puede consumirse sin el encabezado (el productor no cataloga sus especializaciones; 13.10/13.11): diagnóstico explícito, sin recurrir al encabezado original. Es trabajo de catálogo (R2) | — |
| X4-07/X4-08 | `std::string`, `unique_ptr`, rangos y corrutinas combinan X2-05 (retorno por valor no trivial), X3 (runtime C++) y R1 (propiedad); se registran como prerequisitos, sin reglas por biblioteca | — |

Fixture: `nativesharp-interop/interop-sdk/limitaciones_x4.py` (10 casos). Además `tanda13_plantillas_cpp.py` pasa.

Nota (C2): `tanda16_efectos_cpp.py` 16.8 esperaba el rechazo de una macro de función con `__FILE__`; desde C2-06 se expande
en el sitio NativeSharp, así que pasó a llamada positiva con comentario (`24ba273a7`). Las fixtures `tanda10`–`tanda16`
se agregan a las compuertas desde X3.

**Gate X4 (2026-11-22): cerrado** con la misma corrida: especializaciones y restricciones frente a Clang en directo y DLL;
plantillas de variable y catálogo de especializaciones para consumidores sin encabezado registrados como pendientes.

## R1 — Ownership y contratos de frontera

| Tarea | Resultado | Commit |
|---|---|---|
| R1-01 | Política por frontera: sin contrato, prestado durante la llamada y sin comprobaciones nuevas (un `T*` aislado no determina propiedad) | `6088d221d` |
| R1-02 | Rutas: el contrato se declara en `CppLibraryReference` (ProjectLoader), viaja a la referencia cargada, se aplica al símbolo proyectado en el inventario (`CppSemanticInventory`) y se comprueba en la llamada (`CallAnalyzer::checkCppContracts`) y en el descarte (`StatementAnalyzer`) | `6088d221d` |
| R1-03 | `Retained Until="Call:<f>"` (registro/desregistro): se acepta un local cuyo bloque llama a f después; sin desregistro o desde un bloque interno que termina antes, se rechaza. Callbacks retenidos con lambdas sin capturas y estado por `user_data` (C4) | `6088d221d` |
| R1-04 | `Transfer` rechaza locales y memoria de pila; `Result="Owned" Release="f"` rechaza descartar el resultado y exige consumir el local en todo camino (liberar con f, transferir, devolver, guardar o capturar): ramas de `if`, secciones de `switch`, `try`/`catch`, bucles que pueden no ejecutarse, `return` temprano. `throw` sale por excepción; `break`/`continue` detienen la comprobación de ese local | `6088d221d`, `f4f33c7c4` |
| R1-05 | Los contratos viajan en el catálogo C de NSMeta (grupo de contratos) y el consumidor precompilado sin encabezado los aplica como la importación directa | ver «V-05, NSMeta y Core 512 bits» |

Fixture: `nativesharp-interop/interop-sdk/limitaciones_r1.py` (11 casos, O0–O3; contratos con `<Contract>` en la referencia).

**Gate R1 (2026-11-22): cerrado.** Con el binario de `6088d221d`, las 27 fixtures (`limitaciones_c1..c5`, `x1..x4`, `r1`,
`tanda01`–`tanda17_18`) pasan sin fallas. Cada frontera aceptada tiene política explícita o conserva el rechazo; las
comprobaciones de uso posterior y desregistro no dependen de inferencias por tipo.

## R2 — Caché, catálogos y diagnósticos

| Tarea | Resultado | Commit |
|---|---|---|
| R2-01/R2-02 | Productor/consumidor: las fixtures `limitaciones_c*` y `x*` comparan directo, `.lib` precompilada sin encabezado y DLL. Pérdidas registradas: records C++ no triviales y especializaciones de plantillas C++ en plantillas precompiladas (X2, X4-06) | — |
| R2-03 | Con `--artifact-cache=<dir>` (la caché es opcional): fría sin reutilizar, caliente reutilizada (`compilation-bundle-cache: hit`), cambiar solo un encabezado transitivo o solo un `Define` no reutiliza y cambia el programa (42→41→43); `pruebas-tanda-17-cache` pasa. Corrección: la primera versión del caso compilaba sin `--artifact-cache` y no probaba la caché (corregido en `R2-03` posterior) | `9dda76763`, corrección posterior |
| R2-04 | Diagnósticos de Clang con nota de inclusión, una sola vez, fix-its (`tanda01`); `pruebas-tanda-17-diagnosticos` pasa | — |
| R2-05 | `-fdebug-prefix-map` reescribe la ruta en el PDB también para las unidades C del encabezado | `9dda76763` (fixture) |
| R2-06 | Fallos de catálogo explícitos (p. ej. "imported record metadata origin is unsupported", "has no visible ... template") sin recurrir al encabezado original | — |
| R2-07 | La "falta de aciertos en caliente" registrada antes era una lectura errónea: sin `--artifact-cache` no hay caché; con ella la recompilación sin cambios se reutiliza (con y sin referencias C/C++). Sin medición que justifique optimizar | — |
| R2-08 | Defecto corregido (T17-005): una global C/C++ declarada y no definida ahora se explica como una función | `9dda76763` |
| R2-09 | `.pch`/`.pcm` exclusivos no caracterizados en esta rama (sin cambio) | — |

## R3 — Runtime, excepciones y distribución

Con el binario actual pasan: `pruebas-tanda-15-excepciones` (excepciones C++ que atraviesan marcos NativeSharp, R3-04),
`pruebas-tanda-15-recursos` (punteros inteligentes, allocators, no copiables y corrutinas, X4-08), `pruebas-tanda-16-arranque`
(constructores y destructores globales, R3-01/R3-02), `pruebas-tanda-16-tls` (TLS dinámico desde dos hilos, R3-03),
`pruebas-tanda-16-enlace` (enlace separado, duplicados y LTO, R3-11), `pruebas-tanda-17-cache`, `pruebas-tanda-17-diagnosticos`
y `pruebas-tanda-18-constructores` (constructores que lanzan en locales, X2-02). Tres de ellos requieren `nslink.exe` en el
`PATH` (fijan `NSC_LINKER=nslink.exe`). Una excepción NativeSharp no puede salir de un callback C (error de compilación,
C4-05/R3-06). El arnés `interop-sdk` enlaza C++ sin CRT: RTTI, `operator delete` dimensionado y estáticos locales
seguros requieren el runtime C++ (R3-01). Sin cambios de compilador en R3; R3-08/R3-09/R3-10/R3-12 quedan como
documentación de dependencias externas sin modificar NSLink ni herramientas.

## V — Integración y cierre

| Tarea | Resultado |
|---|---|
| V-01 | T19-026 conciliado: HEAD `9dda76763`, `NSC_LINKER=A:\nslink-devel\bin\release\nslink.exe`; `validar-indice-expresion.py` y `validar-indice-llamada.py` ejecutados 5 veces cada uno en concurrencia (10 procesos): todos verdes. Coincide con la atribución de la falla histórica a la ausencia de `NSC_LINKER` |
| V-02 | Los validadores de T19-027/037 están presentes y verdes; no se modificaron resultados históricos |
| V-03/V-06 | Matriz de aceptación en `MATRIZ-ACEPTACION.md` (directo/.lib/DLL, estados A/D/P/B) |
| V-04 | Encabezados reales: zlib 1.3.2, ICU4C icuuc, STL y paquete instalado en entorno limpio, verdes |
| V-05 | Ejecutada el 2026-11-24 a petición del usuario (ver «V-05, NSMeta y Core 512 bits»): sin regresiones abiertas de la rama |
| V-07 | Pendiente: requiere un entorno de depuración interactiva; no se ejecutó y no se presenta como aprobado |

### X2-05 (continuación): retorno por valor (`2837e59b2`)

`var o = lib.make(42)` con un record no trivial devuelto por valor se construye en el local: la función se publica como
adaptador en sitio generado por Clang (`T* a(T* self, ...) { return ::new (self, tag) T(::f(...)); }`, elisión garantizada,
etiqueta sin placement delete) y además como constructor de fábrica de su record, que `new T(...)` nunca elige; la llamada
registra un sitio de construcción y el HIR la baja como la construcción del local, con la ruta existente (guarda de
locales C++, construcción en sitio, destructor del local, indicador de destrucción solo tras volver la llamada, T18-011).
Verificado frente a Clang con CRT en `pruebas-x2-retorno-valor/validar.py` (estática y `--dll`, O0–O3): una fábrica que
lanza deja el destino sin construir ni destruir, retorno temprano destruye una vez, sin copias ni movimientos, move-only.
`limitaciones_x2.py` (17) convierte el rechazo anterior en caso positivo y conserva la cobertura de rechazo en las formas
aún no admitidas: devolverlo desde una función NativeSharp, un temporal sin local y reasignar un local construido. Una
plantilla precompilada que lo devuelve sigue fallando de forma explícita en el productor (catálogo, R2): no cubierta.
Regresión dirigida verde: `x1`, `x3`, `x4`, `r1`, `tanda09`–`tanda17_18`, T10 integral, T11 constructores/objetos/propiedad,
T12 herencia, T18 constructores.

### C5-07 (continuación): callbacks `sysv_abi` escalares (`2d20a05dc`)

Una lambda o función NativeSharp hacia un puntero `sysv_abi` cuyos valores son enteros de 32/64 bits, flotantes, `bool` o
punteros es un thunk `x86_64_sysvcc` (`__sysv_boundary`, `bool` zeroext); LLVM los clasifica igual que Clang. Records,
enteros estrechos y 128 bits siguen rechazados con el motivo. `limitaciones_c5.py` 113, `c3`/`c4`, `tanda07` y
`pruebas-tanda-15-callbacks --dll` verdes. Observación previa a la rama: `pruebas-tanda-09-sysv-indirecta/validar.py` falla
en su caso `BadWide` (espera "C callback signature contains an unavailable type" y obtiene el rechazo del retorno) también
con el binario base `1aec52cfd`; su caso `BadConversion` sigue rechazado con el texto esperado.

### C5-07 (continuación): callbacks `sysv_abi` con records (`4e9327536`)

Tras construir el HIR, cada callback NativeSharp hacia un puntero `sysv_abi` con records por valor o enteros estrechos
recibe el plan de Clang de un typedef C del mismo tipo de puntero (el proveedor que ya usa la llamada indirecta). LLVM
emite `<destino>__sysv_boundary` (`x86_64_sysvcc`) con la firma física del plan (piezas INTEGER/SSE, `<2 x float>`,
byval, sret, signext/zeroext) y llama al límite C existente, que sigue guardando las excepciones. Sin typedef, el
rechazo explica el motivo; un cuerpo de plantilla precompilada nombra el typedef (`scb f = lambda;`) para que su
catálogo lo lleve. 128 bits: el tipo de puntero no se mapea (rechazo de importación). El rechazo anterior de la lambda
con record en `limitaciones_c5.py` pasó a positivo con comentario. Evidencia: `limitaciones_c5.py` 137 (O0–O3, directo/
lib/DLL), validadores T09 `sysv-indirecta-*`, T15 callbacks, `c3`, `c4`, `tanda07`, `tanda09`, `tanda15`, `x2`, `r2`.

### X2-05 (continuación): temporal receptor

`lib.make(42).v`: el objeto C++ devuelto por valor se construye en la ranura de un dueño oculto
(`OwnedTemporaryBorrow`), se presta por dirección y se destruye al final de la expresión completa, como un temporal
C++ (un constructor y un destructor por temporal, sin copias ni movimientos; O0–O3 directo y DLL). El rechazo anterior
en `limitaciones_x2.py` pasó a positivo con contadores. Siguen rechazados con motivo: operadores aplicados a un
temporal (`*lib.make(0)`), un temporal que sale de la expresión, reasignar un local construido y devolver el record
desde una función NativeSharp.

### X2-05 (continuación): reasignación (`d5e5a182d`)

`o = lib.mk(40)` llama al `operator=` que C++ elige para un rvalue (búsqueda de miembros especiales de Sema: movimiento,
o copia si no hay) y `o = p` al de un lvalue const; implícitos incluidos, borrados o no públicos no se publican. Un
prvalue C++ prestado por dirección recibe en general un dueño oculto destruido al final de la expresión completa. Para
`Obj` (C++ borra su asignación de copia implícita porque declara un constructor de movimiento) la reasignación sigue
rechazada, ahora con ese motivo. `limitaciones_x2.py` 32; validadores T10–T12 y de retorno por valor verdes.

### X2-05 (continuación): operadores miembro sobre temporales (`51dc0f58b`)

`*lib.make(0)` y `lib.make(1)(2)` ejecutan el operador miembro sobre el temporal, como C++, con dueño oculto destruido al
final de la expresión completa. Un prvalue a un parámetro por valor no se posee (el adaptador por valor lo copiaría donde
C++ construye el parámetro en su lugar): sigue rechazado. `limitaciones_x2.py` 33.

### X2-05 (continuación): retorno desde funciones NativeSharp (`7b28fd881`)

Una función NativeSharp que devuelve un record C++ no trivial lo construye en el almacenamiento del llamador (puntero
oculto, como el ABI de Microsoft): `return lib.make(k);` construye allí y `return Otra(k);` le pasa el mismo destino;
`var o = Make(42)` pasa la dirección del local y activa su indicador de destrucción al volver la llamada. Sin copias ni
movimientos (contadores, O0–O3). Devolver un local de la función (NRVO) sigue rechazado. `limitaciones_x2.py` 38.

### X2-05 (continuación): `return o;` como movimiento implícito (`dc9f0114d`)

Devolver un local C++ no trivial construye el resultado en el destino del llamador con el constructor de movimiento
(o el de copia si no hay) y el local se destruye al salir de su ámbito, como C++ sin elisión. El constructor de
movimiento que oculta uno de copia (T11-016) se proyecta marcado y fuera de la resolución de sobrecarga. `return move(o)`
sigue rechazado. `limitaciones_x2.py` 41.

### X2-05 (continuación): `return move(o);` (`0e0d4a1f5`)

Es el mismo movimiento que `return o;`; el local movido sigue destruyéndose en su ámbito, como en C++.
`limitaciones_x2.py` 44.

### X2-06: decisión

Un struct NativeSharp con un campo de tipo C++ no trivial queda como restricción consciente con diagnóstico: los structs
NativeSharp se copian por bytes, y el campo exigiría construcción, copia y destrucción con la semántica C++ (la misma
representación espejo que X4-05). No se emula con `memcpy`.

### C3-02 (continuación): `_BitInt(N)` con 64 < N < 128 (`cf11ef5d9`)

Como valor de función se lleva como el entero de 128 bits y sigue el plan de Clang: argumento truncado a N bits y
extendido, pasado por la dirección de una copia; resultado por puntero oculto. Los campos de record siguen sin mapear
(alineación 8 frente a 16) y los callbacks conservan su rechazo. `limitaciones_c3.py` 175 (directo/lib/DLL).

### C3-01 (continuación): callbacks con `_Float16` (`4ef7a3449`)

Con la convención C de Win64, un puntero a función C con `_Float16` pasa el valor en XMM como `half` de LLVM, igual que
Clang: el thunk del callback y la llamada por el puntero reinterpretan los bits del portador Half. Un typedef C de ese
puntero expande `_Float16` a `NativeSharp.Core.Numerics.Half`. `limitaciones_c3.py` 186. Los callbacks con `_BitInt`
estrecho siguen rechazados: el ancho N no forma parte de la identidad del tipo función NativeSharp y añadirlo cambiaría
el lenguaje.

### C3-09 (continuación): orden en ejecución (`a4ef76dfb`)

Una barrera C11 con `memory_order` conocido solo en ejecución recorre el orden con un switch como Clang (consume/acquire
→ acquire, release, acq_rel, seq_cst; otro valor, ninguna; la de señal conserva su ámbito de un hilo). `limitaciones_c3.py` 198.

## Estado de los pendientes (HEAD `27ac43d3b`)

Lo implementable sin decisión de lenguaje ni autorización está hecho. Lo restante:

Bloqueado por autorización o componente externo:
- R1-05 contratos en NSMeta; X4-06/R2 catálogo de especializaciones y de records C++ no triviales para consumidores
  sin encabezado (columna L de X2/X4); typedefs `sysv_abi` sin nombrar en plantillas precompiladas: requieren cambiar
  el formato NSMeta (autorización).
- Vectores de 512 bits: requieren portadores nuevos en Core.
- V-05 regresión integral y V-07 depuración interactiva: a petición del usuario / entorno autorizado.

Restricciones conscientes con diagnóstico (requieren diseño de lenguaje o de representación):
- Derivar/implementar una clase C++ (o una interfaz COM entrante) desde NativeSharp (X3-01/X3-04/X3-07): contrato de
  vtable y de propiedad del objeto.
- Struct NativeSharp con campo C++ no trivial (X2-06) y struct NativeSharp como argumento de plantilla C++ (X4-05):
  representación espejo con construcción, copia y destrucción C++.
- Punteros a miembro (X3-06): su representación Win64 depende del modelo de herencia.
- Callbacks con `_BitInt` estrecho: el ancho no forma parte de la identidad del tipo función.
- Capturas en callbacks C y métodos de instancia como callback (C4): sin canal de entorno en un puntero C.
- `sbyte` sin origen C en `_Generic` (ítem 5.5), `setjmp`, orden NativeSharp ante ambigüedad C++ (T10-009, decisión
  probada por `536baf9e4`).

## X2-05: prvalue a parámetro por valor (HEAD `359c7d39a`)

- `lib.take_val(lib.make(42))` y `lib.take_val(new lib.Obj(1))` (antes rechazo "no viable overload"): en la ABI
  Microsoft x64 un record que no viaja en registros se pasa como la dirección del temporal del llamador y lo destruye
  el llamado. ClangSession publica junto al adaptador por valor el símbolo original (`cppByValueDirectSymbol`, solo si
  cada record no trivial es `!canPassInRegisters()` e `isParamDestroyedInCallee()`); la proyección agrega una variante
  oculta `__nsc_cpp_byvalue_direct_<f>`. El analizador acepta el prvalue (`cppByValueRecord`), el HIR
  (`ownCppTemporaries`) redirige la llamada cuando todos los argumentos record por valor son construcciones
  materializadas y marca el dueño `cppCalleeDestroyed` (sin destructor del llamador, tampoco en el desenrollado).
- Contadores: un constructor, ninguna copia ni movimiento, un destructor (el del llamado); también move-only
  (`take_mo(new lib.MoveOnly(42))`) y una copia explícita como prvalue (`new T(in o)`).
- Decisión: mezclar en una llamada un prvalue con un objeto prestado con `in`/`ref` se rechaza (el prestado
  necesitaría su propia copia en el parámetro); el diagnóstico sugiere `new T(in o)`.
- `limitaciones_x2.py` 46 (directo y dll, O0–O3); regresión dirigida: x1, x3, x4, `pruebas-x2-retorno-valor`,
  tanda 11 (constructores, destructores, métodos, objetos, propiedad).

## C3-02: campos `_BitInt` anchos y dos regresiones de X2-05 (HEAD `138a7aafc`)

- `6326237e5` (regresiones halladas con pruebas-tanda-19): `ownCppTemporaries` tomaba la dirección de cualquier
  construcción C++ pasada directamente a una llamada; ahora solo si el parámetro toma una dirección (`const T&`, `T&&`,
  `in`/`ref`, el self de un adaptador), así una union o struct C por valor vuelve a viajar como bytes
  (`pruebas-tanda-19-c-valores`, `ui(new U())`). Cada `=` de un cuerpo de plantilla demanda `operator=`: el adaptador
  de asignación de un record C++ no transportable dejó de ser dependencia del catálogo de una biblioteca estática
  (`pruebas-tanda-19-dllimport-inline`). Ambos validadores pasan otra vez, igual que con el binario base.
- `138a7aafc`: un campo `_BitInt(N)` con 64 < N < 128 (antes 'NativeSharp has no mapping for that field type') se
  lee y escribe como el entero de 128 bits en el offset y la alineación de Clang (8 en Windows x64; el struct de
  prueba mide 48 como en Clang). `limitaciones_c3.py` 210 (c3-bitint-campo en directo, lib y dll, O0–O3).
- `1d6975ef4` (defecto previo del núcleo, también en el binario base): `sizeof(T) != k` con `k` `int` se aceptaba
  pero se bajaba como `icmp i64, i32` y el verificador de LLVM lo rechazaba; el HIR compara ahora en el tipo al que un
  operando convierte implícitamente, como lo aceptó el analizador. `c3-sizeof-comparacion` (directo, lib, dll).

## X2-05: varios argumentos por valor y objetos prestados (HEAD `0b2cee2ae`)

- `84fec59ff`: con varios prvalues por valor cada temporal conserva su indicador y su limpieza como un local hasta
  la llamada, que apaga (tras evaluar todos los argumentos) los indicadores de los parámetros que destruye el llamado
  (`cppCalleeDestroyedParameters` de la variante directa): una excepción al construir un argumento posterior destruye
  los anteriores, como en Clang. El adaptador por valor solo se genera si una llamada lo alcanza, así que
  `take_two(make(1), make(2))` enlaza sin CRT. Los contratos de construcción C++ de una función
  (`cppFactoryConstruction`, `cppConstructedResult`, parámetros destruidos por el llamado) se restauran desde la
  declaración HIR al leer una función MIR de la caché (antes se perdían con `--artifact-cache`).
  `pruebas-x2-retorno-valor` compara `take_two(make(3), make(4))` con Clang (estático y DLL).
- `0b2cee2ae`: un objeto prestado con `in` (`ref m` si solo se mueve) a un parámetro por valor es la copia (el
  movimiento) que C++ construye en ese parámetro: el HIR lo vuelve esa construcción en el temporal que destruye el
  llamado y la llamada va a la función original. La mezcla prvalue + `in` (antes rechazo 'is lent with') funciona, y
  `take_two(in o, in p)`, que no enlazaba sin CRT (`__CxxFrameHandler3` del adaptador), también.
- Nota de orden: en la ABI Microsoft Clang evalúa de derecha a izquierda los argumentos que destruye el llamado;
  NativeSharp conserva su orden de izquierda a derecha (C++ no lo fija). Solo se observa si un argumento lanza.
- `limitaciones_x2.py` 52.

## C5-07 / C5-06 (HEAD tras `1ed0ba0d6`)

- `1ed0ba0d6`: puntero y callback `sysv_abi` con `__int128` (antes "parameter 0 ('wide_cb') is not yet mapped"): con
  esa convención Clang lo pasa y devuelve como `i128` directo (dos registros enteros, RAX:RDX), igual que
  x86_64_sysvcc; el tipo se mapea, el callback no necesita plan, la llamada por el puntero lo pasa directo y el
  thunk de frontera sysv ya no le aplica la forma Win64 (dirección y XMM0, T19-044). `c5-sysv-ancho` en directo, lib
  y dll. Muestra dirigida: T15 callbacks, T19-int128 (ruta Win64 intacta), T09 sysv de punteros/records.
- C5-06: con `enum mode` y `typedef int mode`, el nombre `mode` escrito como tipo es el typedef, como en C (antes
  'C/C++ type alias conflicts with a native type'); la etiqueta sigue alcanzable por su identidad en las firmas.
  `c5-nombres-etiqueta-typedef` en directo, lib y dll.
- Nota: se detuvo un barrido de 304 validadores de T09/T15 (equivalente a una batería) tras 90 en verde, para
  respetar la indicación de no correr baterías completas sin petición.

## C5-02, X2-06, X4-05, X4-04 (continuación)

- C5-02 (ítem 5.5): un local declarado con `var` desde un valor C `signed char` tiene ese tipo C (`_Generic` elige
  `signed char`), como una variable C tiene su tipo declarado; un local escrito `sbyte` sigue siendo `char`.
  `c5-generic-local`.
- X2-06 (parcial): un struct NativeSharp con un campo de record C++ transportable (trivialmente copiable y
  destructible) se rechazaba ('has no imported object layout yet'); sus bytes son su valor, como un record C, así que
  el struct lo contiene y lo copia. Un campo C++ no trivial sigue como restricción consciente.
  `x2-campo-transportable`.
- X4-05: un struct NativeSharp como argumento de una plantilla de clase C++ (`new lib.Box<P>(p)`) se rechazaba ('is
  not imported in its project'); Box<P> se instancia sobre el espejo C++ de P (T13-007) y el constructor y la vida
  C++ aceptan el struct reflejado como ya lo hacían sus métodos. `x4-struct-ns-argumento`.
- X4-04: una plantilla de variable `constexpr` `float`/`double` se rechazaba; se leen sus bits con
  `__builtin_bit_cast` (expresión constante entera) y se vuelve el literal decimal que convierte al mismo valor
  (`%.17g`, `%.9g`+`f`). Una no constante (`inline T zero`) sigue rechazada. `x4-plantilla-variable-flotante`.
- Restricciones conscientes revisadas y mantenidas: `setjmp` (ítem 8.7, D8: un segundo retorno invalida los
  supuestos de análisis de NativeSharp), T10-009 (decisión probada).

## C3-08 y C3-01 (continuación)

- `5cf0e21be`: un puntero `sysv_abi` a función C sin prototipo se mapea como el cdecl (C3-08): la llamada aplica
  las promociones por defecto y pasa los argumentos como una llamada variádica sysv (mismos registros; AL con el
  número de vectores, que un destino variádico necesita). `c3-sin-prototipo-sysv-puntero` (directo, lib, dll).
- C3-01: un Half en `...` de una llamada C (antes rechazo 'vread') llega como el double al que C promueve
  `_Float16`. `c3-float16-variadica-implicita`.
- Decisión: callbacks C con `_BitInt` estrecho siguen rechazados: la identidad NativeSharp del tipo función no lleva
  N (Win64 pasa `iN` sin extender y los bits altos del registro son indefinidos); un puntero estrecho sin typedef se
  compilaría como su portador sin aviso. Requiere llevar N en la identidad del tipo función.

## X4-04: plantillas de variable no constantes (HEAD `fa6a78a57`)

- `lib.zero<double>`, `lib.counter<int> = 40` (antes rechazo 'not an integral or bool constant'): la sesión declara
  una accesora `extern "C" inline auto* __nsc_macro_ref___nsc_vt_<h>() { return &(lib::counter<int>); }`, la
  reanaliza (reinstanciando lo que Sema había instanciado) y la informa como global, igual que una macro no
  constante (T19-041); el pre-pase sustituye la expresión por esa global. NativeSharp y C++ comparten el mismo
  objeto (`read_counter()`), con asignación compuesta incluida. `x4-plantilla-variable-double` (directo, dll).
  Regresión: X4, C2, T19 macros y macros no constantes, T13 constexpr.

## X3-04/X3-07: implementar una interfaz C++ desde NativeSharp

Alcance (primera entrega): `class MyCalc : ICalc` donde ICalc es un record C++ completo, polimórfico, sin campos
(tamaño = un puntero de vtable), sin bases virtuales, cuyos métodos virtuales son todos puros (también los de sus
bases, cadena simple, p. ej. COM `IUnknown`). La clase NativeSharp declara un método público de instancia por cada
virtual puro, con el mismo nombre y la firma NativeSharp mapeada; sobrecargas virtuales con el mismo nombre se
rechazan. Valores escalares y punteros (referencias como `in`/`ref`).
Representación: Clang genera un adaptador `struct __nsc_iface_<h> final : ICalc { void* self; R m(P...) override
{ return B(self, p...); } }` cuyas sobreescrituras llaman al límite C (`<método>__c_boundary`, que guarda las
excepciones de NativeSharp) del método NativeSharp, y `extern "C" void* __nsc_iface_<h>_bind(void* storage, void*
self)` que lo construye en `storage`. El objeto NativeSharp reserva dos campos ocultos (vptr, self) contiguos; la
conversión explícita `(ICalc*)obj` llama a bind con su dirección y devuelve el puntero a la interfaz. La vida del
objeto es de NativeSharp (C++ lo toma prestado, como un callback). Lib/DLL: se rechaza con diagnóstico al inicio.

Implementado como se diseñó (arriba):
- `CppSemanticRecord`: `cppInterfaceMethods` marca la interfaz elegible. `MemberCollection`: dos campos ocultos
  `__nsc_cpp_interface_vptr/self` (tamaño puntero, sin sintaxis; el layout de clase los mide). La herencia acepta la
  base sin tomarla como clase base NativeSharp; `buildInheritanceModel` exige un método público de instancia por
  virtual puro con la firma NativeSharp del método importado (diagnóstico con la firma esperada).
- `(ICalc*)obj`: el analizador acepta el cast; MIR marca el `Convert` con el campo `vptr` y su offset; LLVM llama a
  `__nsc_cpp_iface_<h>_bind(&campos, obj)`. Los métodos implementadores reciben su límite C (`__c_boundary`).
- El driver pide a la sesión de Clang el adaptador (`emitCppInterfaceAdapterIR`): clase final que deriva de la
  interfaz, sobreescrituras que llaman a los límites por `__asm__`, bind con placement new propio; sin RTTI (sin
  `type_info` del CRT; un `dynamic_cast` C++ sobre ese objeto no está disponible) y con una trampa privada en lugar
  de `_purecall` para las vtables de las bases abstractas.
- `limitaciones_x3.py`: `x3-com-entrante` (COM: QueryInterface devuelve `(ICalc*)this`, AddRef/Release, Add), O0–O3,
  directo y dll; rechazos: clase incompleta (falta QueryInterface) y base C++ que no es interfaz (`lib.Shape`).
- `e4e1a6443`: también interfaces con virtuales con cuerpo C++ y destructor virtual (patrón observador): la clase
  implementa los puros y puede sobrescribir los demás (`x3-observador`); el `operator delete` del destructor de
  borrado es una trampa privada (C++ no debe borrar un objeto NativeSharp).
- `caec66435` (X3-01): también clases C++ polimórficas con datos (alineación ≤ 8, construibles por defecto, destructor
  trivial y entonces no virtual): los campos ocultos miden el adaptador; bind lo construye una vez (vptr nulo en el
  objeto en cero) y nunca lo destruye, lo que el destructor trivial hace exacto (`x3-base-con-datos`; `lib.Shape`
  pasa de rechazo a positivo con ida y vuelta NativeSharp → despacho C++ → método NativeSharp).
- `196f1237e`: bases con destructor no trivial (también virtual): la destrucción de la clase termina con un paso que
  llama a `<adaptador>_destroy(objeto)` tras los campos propios (como un subobjeto base C++), solo si una conversión
  lo construyó; `x3-base-destructor` cuenta exactamente una destrucción (y ninguna si nunca se convirtió).
- `0664871a1`: `base(args)` elige un constructor público de la base C++; el adaptador hereda los constructores
  (`using Base::Base`) y expone una entrada por constructor, nombrada por su símbolo y con su ABI, que el
  inicializador llama sobre el almacenamiento oculto (`x3-base-argumentos`). Sin constructor por defecto accesible,
  solo `base(args)` construye el objeto (enlazar uno sin construir atrapa).
- Nota: una clase polimórfica definida completamente `inline` en el encabezado hace que NSC emita su vtable con RTTI
  (la sesión de NSC no recibe `-fno-rtti`), que exige `type_info` del CRT; es previo al adaptador.
- Fuera de alcance: modo lib (R2/NSMeta), sobrecargas virtuales del mismo nombre, acceso desde NativeSharp a los
  campos de la base (se alcanzan por el puntero C++).

## Estado de los pendientes (HEAD `d7070f244`)

Implementado en esta continuación (además de lo anterior): prvalues y objetos prestados a parámetros por valor
(construidos en el parámetro), campos `_BitInt` anchos, `sizeof` frente a `int`, callbacks/punteros `sysv_abi` con
`__int128` y sin prototipo, Half en `...`, typedef frente a etiqueta homónima, `signed char` de locales `var` en
`_Generic`, campos C++ transportables en structs NativeSharp, structs NativeSharp en plantillas de clase C++,
plantillas de variable flotantes y no constantes, y clases NativeSharp que implementan interfaces C++ (COM
entrante, observador) o derivan de clases C++ con datos (destructor y `base(args)` incluidos).

Bloqueado por autorización o componente externo:
- NSMeta: contratos (R1-05), catálogo de especializaciones y de records C++ no triviales o polimórficos para
  consumidores sin encabezado (columna L de X2/X3/X4), typedefs `sysv_abi` sin nombrar en plantillas.
- Core: vectores de 512 bits. V-05 regresión integral y V-07 depuración interactiva.

Restricciones conscientes (requieren una decisión de diseño de lenguaje con riesgo de semántica silenciosa):
- Punteros a miembro (X3-06): NativeSharp no tiene sintaxis para formarlos ni un tipo que los represente.
- Struct NativeSharp con campo C++ no trivial (X2-06): exige estado construido/no construido por campo, copia C++ o
  move-only y destrucción en el struct.
- Capturas y métodos de instancia como callback C (C4): un puntero C no lleva entorno; requeriría trampolines con
  memoria ejecutable y una vida ligada al cierre.
- (Resuelto en `85d910029`: callbacks con `_BitInt` estrecho, N <= 64.)
- `setjmp` (D8), T10-009 (decisión probada).
- X2-06 (clases): un campo de record C++ no trivial en una clase NativeSharp se construye con el objeto como un
  miembro C++ (en su lugar por su inicializador `new T(...)`/fábrica, o por defecto, en orden de declaración, también
  en la construcción en línea del constructor implícito), `h.b = valor` es su `operator=` y la destrucción del objeto
  ejecuta el destructor de cada campo (`x2-clase-campo-cpp`). Un struct con ese campo sigue fuera (copia C++).

`2f624a2f4` (X3-04): sobrecargas virtuales del mismo nombre al implementar una interfaz, emparejadas por firma
NativeSharp y por las escrituras C++ de sus parámetros (`x3-sobrecargas-virtuales`).

- X3-07: una clase implementa varias interfaces C++ (COM), con un adaptador y campos ocultos por interfaz
  (`x3-com-multiple`).
- X3-08: `Rtti="false"` en `CppLibraryReference` compila la sesión de Clang con `-fno-rtti`; una clase polimórfica
  definida entera `inline` ya no exige `type_info` del CRT (`x3-rtti-false`, `x3-rtti-invalido`).
- C3-02 (callbacks): un callback C con `_BitInt(N)` estrecho (N <= 64, convención C de Win64) ya no se rechaza. N
  forma parte de la identidad del tipo función (sufijo opcional `:bitint:`, escrito solo si hay alguno, así las demás
  identidades conservan sus bytes) y llega también por el typedef C. Una llamada por el puntero pasa `iN` y extiende
  el resultado según el signo del portador; una lambda llega al puntero por un adaptador que extiende cada `iN` (bits
  altos indefinidos) y trunca su resultado. N > 64 sigue rechazado (`c3-bitint-callback`, `c3-bitint-lambda`,
  `c3-bitint-callback-ancho`; directo, lib y dll).
- C3-02 (callbacks anchos): `_BitInt(N)` con 64 < N < 128 en callbacks y punteros C viaja como lo pasa Clang (dirección
  de una copia extendida desde N; resultado en memoria), con el entero de 128 bits como portador y N en la identidad
  (`c3-bitint-callback-ancho`, antes rechazo; directo, lib y dll).
- X2-06 (struct, decisión revisada): se intentó un struct "fijo" (campos C++ construidos en su lugar, sin copias ni
  movimientos). Al aceptar su layout todo compilaba en silencio (arreglos, genéricos, campos de clase, temporales) sin
  construir los campos: el rechazo de layout es el guardián central y cerrar cada vía exige muchos controles dispersos
  con riesgo de relocación por bytes de objetos C++. Se mantiene la restricción con un diagnóstico claro (antes 'has no
  imported object layout yet') y se corrige un defecto previo: `new T[n]` de un record C++ no trivial llegaba al backend
  ('cannot map named value type to LLVM carrier'); ahora es un rechazo con el mismo diagnóstico
  (`x2-struct-campo-cpp`, `x2-arreglo-cpp`).
- X2-06 (defecto previo): `(lib.Obj, int) t;` compilaba, se iniciaba en cero y al salir del ámbito ejecutaba el
  destructor C++ sobre un objeto nunca construido. Una tupla o arreglo fijo local con un record C++ no trivial por
  valor se rechaza con el mismo diagnóstico (`x2-tupla-cpp`).

## Estado de los pendientes (HEAD `b996edc89`, 79 commits locales)

Hecho en esta continuación: X2-06 en clases (campos C++ no triviales construidos y destruidos con el objeto), X3-07
(varias interfaces C++ por clase), X3-08 (`Rtti="false"`), C3-02 en callbacks y punteros C (`_BitInt(N)` estrecho y
ancho), y tres defectos previos de records C++ no triviales retenidos por valores NativeSharp (diagnóstico de struct,
`new T[n]` que llegaba al backend, tupla local que destruía un objeto nunca construido).

Bloqueado por autorización o componente externo:
- NSMeta: contratos (R1-05), catálogo de especializaciones y de records C++ no triviales o polimórficos para
  consumidores sin encabezado (columna L de X2/X3/X4), typedefs `sysv_abi` sin nombrar en plantillas.
- Core: vectores de 512 bits. V-05 regresión integral y V-07 depuración interactiva.

Restricciones conscientes, con diagnóstico:
- Struct NativeSharp (y arreglo, tupla, argumento genérico) con un record C++ no trivial por valor: se copiaría o
  reubicaría por bytes. Se intentó un struct fijo; ver la entrada X2-06 (struct, decisión revisada).
- Punteros a miembro (sin sintaxis ni tipo NativeSharp); capturas y métodos de instancia como callback C (un puntero C
  no lleva entorno; el patrón `void* user` funciona); `setjmp` (D8); T10-009.
- X2-06 (excepciones): si el constructor C++ de un campo lanzaba durante la construcción de una clase NativeSharp, los
  campos C++ ya construidos no se destruían (C++ sí lo hace). En un constructor, la construcción en su lugar activa el
  indicador de limpieza del campo; en la construcción en línea (constructor implícito) cada campo C++ construido abre
  una región de limpieza hasta terminar. `pruebas-tanda-18-constructores` agrega el caso, frente a Clang, O0–O3,
  estática y DLL.
- X3-01 (excepciones): una clase derivada de una clase C++ cuyo constructor lanzaba después de `base(args)` (p. ej.
  en un campo C++) dejaba viva la base C++ (NativeSharp `134`, Clang `1342`). El constructor registra una limpieza de la
  base C++, viva al volver su construcción, que llama a la entrada de destrucción del adaptador al desenrollar.
  Validador nuevo `pruebas-x3-construccion-excepcion` frente a Clang (fallo y éxito), O0–O3, estática y DLL.
- X3-01 (construcción por defecto de la base): sin `base(args)` (constructor implícito o explícito sin él) la base C++
  nunca ejecutaba su constructor por defecto: el adaptador la construía de forma perezosa en la primera conversión y su
  destructor no corría (NativeSharp `3344`, Clang `133442`). Ahora se construye por defecto antes de los campos, en el
  constructor y en la construcción en línea, con la misma limpieza ante un fallo posterior
  (`pruebas-x3-construccion-excepcion`: `base(5)`, implícito y explícito sin `base(...)`).
- X3-01 (cadenas): en `E : D : L.Base` la base C++ de D no se construía si la inicialización de D era implícita, y una
  construcción en línea de E que llamaba al constructor de D dejaba viva la D completa si un campo de E lanzaba. Se
  construyen las bases C++ de toda la cadena implícita, se registra la limpieza de la base completada y la limpieza de
  la base C++ cede ante la de su clase completa (una sola destrucción, después de sus campos). Tres variantes de cadena
  en `pruebas-x3-construccion-excepcion`.
- X2-06 (estáticos): un campo estático de un record C++ no trivial llegaba al backend ('cannot map named value type to
  LLVM carrier'). Nada lo destruiría al salir del proceso: se rechaza como un estático `owned`
  (`x2-estatico-cpp`).
- X4 (`Exceptions="false"`): el código C++ inline que emite NSC (p. ej. constructor y destructor de una plantilla
  usada como campo de clase) se compilaba con excepciones y exigía `__CxxFrameHandler3` del CRT frente a una biblioteca
  `-fno-exceptions`. `Exceptions="false"` compila la sesión con `-fno-exceptions` (`x4-exceptions-false`,
  `x4-exceptions-invalido`). Confirma además campos de clase de plantilla C++ no trivial.
- X2-05 (métodos): el resultado C++ construido de un método de instancia, un getter o un override virtual NativeSharp
  (por una referencia base, y devuelto de nuevo por otra función) se construye en el local del llamador, y el de una
  función usada como receptor (`Make(4).v`) en un dueño oculto destruido al final de la expresión completa; antes solo
  las funciones de namespace hacia un local (`x2-resultado-metodo`, conteos exactos O0–O3). Una propiedad automática de
  ese record se rechaza en la propiedad (`x2-propiedad-auto-cpp`; antes fallaba más tarde sin ubicación).
- X2-05 (asignación): `x = h.G()` / `x = Make(5)` con un record C++ no trivial fallaba ('call result is not
  addressable'); la llamada en contexto de dirección es ahora un temporal dueño de su resultado, y la asignación es el
  `operator=` del record desde él (`x2-resultado-asignacion`).
- X2-05 (diagnósticos): los rechazos del validador de vida C++ llevan ahora `fichero:línea:columna`; un parámetro C++
  por valor de una función NativeSharp tiene mensaje propio que sugiere `in`/`ref`.
- X2-05 (`const T&`): un prvalue record C++ (`lib.make(3)`, `new lib.Obj(4)`, `Make(5)` NativeSharp) enlaza un parámetro
  `const T&` como en C++, materializado sin copia y destruido al final de la expresión completa, en funciones de
  namespace y miembros (directo y dll); un lugar sigue con `in` (`x2-constref-prvalue`, `x2-constref-miembro`,
  `x2-constref-lugar`).
- X2-05 (métodos C++): un método C++ que devuelve un record no trivial por valor no se importaba. Ahora construye el
  resultado en el destino que NativeSharp pasa tras `this` (ABI de MSVC): directo si no es virtual; si es virtual, su
  adaptador construye el prvalue en ese almacenamiento (placement new, elisión garantizada) en vez de copiar bytes.
  Local, receptor temporal y despacho a un override C++ por puntero base, sin copias (`x2-metodo-resultado`,
  directo y dll, O0–O3). STL, ICU, zlib y paquete siguen verdes.
- X3 (resultados por valor): una clase NativeSharp que implementa una interfaz C++ cuyo método devuelve un record por
  valor (no trivial, ya proyectado; o trivial grande) se caía al llamarla C++: el límite pasaba el puntero de resultado
  antes de `this` al método de instancia, que lo toma después. Corregido (`x3-interfaz-resultado-record`).
- X2-05 (estáticos C++): un método estático C++ que devuelve un record no trivial por valor tampoco se importaba; ahora
  construye el resultado en el destino (primer puntero oculto, como función libre) (`x2-estatico-resultado`).
- X2-05 (operadores): un operador C++ libre que devuelve un record no trivial (`Obj operator+(const Obj&, const Obj&)`,
  como el de std::string) fallaba en el backend; ahora es la construcción del record con su constructor de fábrica
  (local y receptor temporal, sin copias; `x2-operador-resultado`). (El operador miembro se resolvió
  después; ver la entrada siguiente.)
- X2-05 (operadores miembro): `w * 2` con `Obj Vec::operator*(int) const` no se encontraba; el adaptador del operador
  miembro se trata como fábrica (devuelve el prvalue en el almacenamiento del llamador) y la llamada es la construcción
  del record (`x2-operador-miembro-resultado`, directo y dll, O0–O3).
- X4/X2-05 (plantillas de función): `lib.wrap<int>(42)` con `Box<T> wrap(T)` fallaba al enlazar: el adaptador de retorno
  de la especialización no se pedía y la especialización que llama quedaba sin definir. El emisor de adaptadores recorre
  las especializaciones y ofrece las definiciones de enlace vago alcanzables, como T15-014 (`x4-plantilla-resultado`).
- X2 (casts): `(int)h` de un record C/C++ importado sin conversión llegaba al backend; ahora es un diagnóstico del
  frontend (`x2-cast-record`).
- X2 (operadores de conversión): `explicit operator bool()` / `operator int()` de C++ se ignoraban; ahora son la
  conversión explícita `(T)o` (un `op_Explicit(in Self)` estático implementado por el adaptador de miembro), sobre
  lugares y temporales, también de records no triviales. Las conversiones implícitas de C++ siguen rechazadas
  (`x2-conversion`, `x2-conversion-implicita`).
- X2-05 (sobrecargas): con prvalues enlazando `const T&`, `take(const Obj&)`/`take(Obj&&)` quedaba ambiguo para
  `take(new Obj(2))`; como en C++, el prvalue prefiere `T&&` (`x2-rvalue-preferido`).
- X2 (foreach): `foreach` sobre un rango C++ (record con begin()/end(), como std::vector) se recorre como un range-for
  de C++, con iterador puntero o iterador clase (`!=`, `*`, `++` C++); `foreach ref mut` sobre él se rechaza porque
  escribiría una copia (`x2-foreach-rango`, `x2-foreach-rango-ref-mut`).
- X2 (foreach STL): `foreach` sobre `std::array` y `std::vector` (frente a Clang en `pruebas-tanda-18-stl`):
  operadores/adaptadores de especializaciones implícitas, operadores heredados de una base pública (`D == D`),
  `!=` reescrito desde `==` en C++20, y la fuente de un rango C++ que es un lugar se recorre en su sitio
  (`x2-operador-heredado`, `array-foreach`, `vector-foreach`).
  El control negativo T12 `operador-heredado` pasó a positivo (D usa el `operator%` de B).
- X2 (igualdad): `==`/`!=` sobre un record C++ sin operador propio ya no compara bytes ni falla internamente; se
  rechaza con diagnóstico, como en C++ (`x2-igualdad-implicita`). Los structs C conservan su comportamiento.
- X2 (plantillas de función miembro): `m.add(40)` deduce e instancia la plantilla miembro en Clang (como T13-005)
  y la especialización se llama como extensión del record por su adaptador de miembro (`x2-plantilla-miembro`).
- X2 (plantillas miembro estáticas): `M.twice(21)` se deduce igual y se llama por su símbolo como extensión
  estática solo de su propio record (`x2-plantilla-miembro-estatica`, `x2-plantilla-miembro-estatica-otro`).
- X2 (plantillas miembro, argumentos explícitos): `m.add<double>(1)`, `m.size<double>()`, `M.half<double>(5)`; la
  fase genérica deja al análisis de cuerpos una llamada a miembro con `<T>` sin método genérico NativeSharp de ese
  nombre. Cada llamada toma exactamente la especialización que Clang dedujo para ella (antes `half(10)` podía tomar
  `half<double>` por conversión, según el orden de análisis) (`x2-plantilla-miembro-explicita`).
- X2 (std::string): `std.string` se nombra (palabra clave tras un punto), se construye desde un literal (constructor
  `const char* const`: el `const` de nivel superior de un puntero por valor ya no impide importar un constructor), los
  literales llegan al `const char*` de constructores y operadores C++, `s.append(...)` encadenado como sentencia ya no
  copia el objeto y `s += "x"` funciona; constructores plantilla con todos sus parámetros por defecto se instancian
  al importar (`pruebas-tanda-18-stl` `string`, `x2-constructor-puntero-const`).
- X2 (operadores plantilla libres): `s == "x"`, `s != t` sobre std::string; las plantillas de operador del namespace
  del record se deducen de los operandos y en C++20 un `operator==` libre publica el `!=` reescrito
  (`string-compare` en `pruebas-tanda-18-stl`, `x2-operador-plantilla-libre`).
- X2 (relacionales C++20): `<`, `<=`, `>`, `>=` reescritos desde un `operator<=>` libre (`string-order`,
  `x2-relacional-reescrito`).
- X2 (sobrecargas `const T&`/`T&&`): en métodos e indexadores C++ un lvalue toma `const T&` y un prvalue prefiere
  `T&&` para cualquier T; el indexador C++ con referencias es viable (`x2-referencias-sobrecarga`).
  Pendientes STL anotados: `make_unique<T>(args)` (plantilla variádica con argumento explícito); constructores plantilla deducibles
  (`pair(40, 2)`).
- X2 (contenedores STL): `std::list`, `std::map`, `std::unordered_map` frente a Clang (`pruebas-tanda-18-stl`).
  La emisión inline instancia cada miembro por separado y deja fuera los que C++ nunca instanciaría; un cuerpo que
  llama a una especialización nunca definida queda como declaración; los adaptadores rechazados se ubican por las
  líneas de sus errores (antes: un reanálisis por adaptador, minutos con unordered_map). `std.set` se nombra.
- X2 (std::set): `using Base::name;` expone los miembros de la base; las plantillas miembro con todos sus parámetros
  por defecto se instancian al importar; se completan los tipos de firma solo declarados; una sobrecarga no
  proyectable solo bloquea a las hermanas para las que Clang la considera viable (Item 9.1 refinado); un resultado
  C++ no trivial descartado se destruye al final de la sentencia (`set` en `pruebas-tanda-18-stl`,
  `x2-using-base-plantilla-defecto`).
- X2 (calificadores de receptor): sobrecargas `&` / `const&` / `&&` de métodos C++ (`optional::value()`) se distinguen
  y se eligen como en C++ según el receptor (`x2-calificadores-receptor`).
- X2 (std::optional): un constructor no mapeable (`optional(nullopt_t)`) ya no descarta toda la familia de vida; solo
  bloquea si Clang lo considera viable para un constructor proyectado (`optional` en `pruebas-tanda-18-stl`,
  `x2-constructor-no-mapeable`).

## V-05, NSMeta y Core 512 bits (2026-11-24)

Autorizado por el usuario el 2026-11-24: V-05, NSMeta (un solo contrato vigente, ampliado) y vectores de 512 bits en
Core. V-07 queda pendiente. Se mantienen las decisiones sobre punteros a miembro, capturas como callback C, `setjmp` y
structs NativeSharp con records C++ no triviales.

### V-05: regresión integral

- ctest de `build-v05` (compilador `6c25eb592`, clang-cl Release, pruebas y sesiones C++ activas): 1606 pruebas,
  1365 verdes, 241 fallas. Cada falla se repitió aislada con la base `1aec52cfd` y con HEAD: 187 fallan también con
  la base (previas o de entorno: Core `NativeSharpCore.lib` buscado junto al proyecto, raíz de fixtures de sesión
  ausente, rutas largas, selfhost); 54 pasan aisladas con ambos (sensibles a la corrida paralela). 0 atribuibles.
  Evidencia: `A:\temp\v05\informe-ctest.md`, `ctest.log`, `ctest-rerun\`.
- Interop (validadores `pruebas-*` e `interop-sdk`, 1363 scripts, agrupados por tanda en
  `A:\temp\v05\interop2\resumen.md`): 1200 verdes, 137 de entorno (nombran `A:\nsc-interop-build\tanda1-ON`, que no
  existe; 6 de la tanda 08 lo construyen al ejecutarse), 2 sin argumento conocido, 24 fallas. Atribución
  (`A:\temp\v05\atrib-interop*`):
  - de la rama, corregidas: `consumidor-campo-c` (`35265db94`); `tanda12_referencias_cpp` lib (sobrecarga `&&`
    eliminada en el catálogo, `ee3bc3aae`); `tanda14_herencia_cpp` (rechazo que X3-04 levantó, ahora positivo);
    `regcall-cpp-retorno-double-scalar` precompilado (la comprobación buscaba bytes que C2-08 hace viajar en la
    fuente de macros; ahora mira identidades del catálogo); `sysv-cpp-record-retornos-escalares` (verde tras NSMeta);
  - previas (fallan igual con la base): `tamano-objeto` (3), `variadica-preserve-none` (2), `sysv-indirecta`,
    diagnóstico `_BitInt` de `tanda01`;
  - entorno: `tanda-19-traslados` (3) buscan el paquete de Core en `build-clang/packages`; con
    `NSC_STDLIB_PACKAGE_ROOT=A:\temp\pkg` (copia de la instalación congelada) pasan;
  - casos `lib` agregados durante la corrida a C5/X2/X3/X4 (requieren el compilador nuevo; verdes con él).

### NSMeta X4-06/R2: sesión de fuente (`4d5ae8d4e`, etiqueta corregida en `7446e5d5a`)

Un consumidor precompilado sin encabezado recibe records C++ no triviales y polimórficos, especializaciones de
plantillas de clase y typedefs `sysv_abi` no nombrados por la plantilla. Representación (documentada en
`docs/NSMETA-CONTRATO-ACTUAL.md`): la entrada existente de fuente de macros del catálogo C se marca
`__nsc_source_session` y nombra `type:`, `template:`, `name:` y `option:`; el consumidor abre una sesión Clang completa
sobre esa fuente, propia del proyecto precompilado. Los records, funciones, globales, alias y miembros de esa
referencia salen del catálogo de valores. Validado: `limitaciones_nsmeta.py` 56 (lib, dll, O0–O3), 26 casos lib nuevos
de X2/X3/X4/C5/R1 (O0–O3), todas las fixtures `interop-sdk`, validadores T19, `build-clang` `/WX`.

Operadores plantilla libres y relacionales reescritos en L: `5efb9fc6c` (una referencia con plantillas de operador o
con records que declaran plantillas miembro viaja como sesión de fuente). Contratos R1-05: `b35e172bb`.

Pendiente en la columna L (consumidor sin encabezado): plantillas miembro C++ (la sesión de fuente ya viaja; falla
la bajada: "cannot map identifier/member expression"); indexadores y sobrecargas por calificador de receptor ya
pasan (un record con operadores miembro o métodos sobrecargados viaja como sesión de fuente), prvalue hacia `const T&`, clases
NativeSharp que implementan o derivan de clases C++ (X3-01/04/07/08) y `Exceptions="false"` con campos de plantilla.
Los casos con un `preludio` que declara funciones o tipos NativeSharp del productor no aplican a lib.

### R1-05: contratos en NSMeta

Los `<Contract>` del productor viajan en un grupo de contratos del catálogo C (después de las fuentes de macros, solo
si hay) y el consumidor precompilado los aplica con la misma correspondencia que la importación directa, también a
funciones de una sesión de fuente. Validado: `limitaciones_r1.py` `rechazo_lib` (Retained Static, Until Call,
Transfer, Owned descartado, NonNull, liberación por camino): los seis rechazos pasan en lib y fallan con el
compilador previo.

### Core: vectores de 512 bits (`20e26aecf`; Core `9e3a9b2` en `A:\nativesharp-libraries`)

Archivos: Core `stdlib/NativeSharp.Core/{Float32x16,Float64x8,Int32x16,Int64x8}.ns`; NSC `CppSemanticFunction.cpp`
(portadores), `ClangAbiPlan.cpp` (plan del vector suelto), `OopLlvmDeclarations.cpp` (nivel de CPU),
`OopLlvmTypesAbiAttributes.cpp`, `OopLlvmImplementation.h`, `OopLlvmDebugInfo.cpp`. Artefactos: Core construido
aparte en `A:\temp\core512\O0..O3` (la instalación congelada no se sustituye). Con cdecl/ms_abi Win64 el vector
viaja por memoria y no requiere AVX-512 (esta máquina no lo tiene); `__vectorcall` de 512 bits sigue rechazado (ZMM).
Validado: `limitaciones_core512.py` (O0–O3, directo/lib/dll), `limitaciones_c3.py`, validadores de vectores T19
(`--avx`, `--dll`), T09 vectorcall, `build-clang` `/WX`.

### Pendiente

- V-07 (entorno de depuración interactiva): no ejecutado.
- Columna L restante (arriba). STL: `new std.pair<int, int>(40, 2)` ya construye (`9c0c512bd`: un valor enlaza el
  `const T&` de un constructor C++, `x1-constructor-constref` directo/dll); pendiente la deducción de argumentos de
  plantilla de clase (`new std.pair(40, 2)`). El modo lib ya pasa: un record C++ con constructores declarados viaja
  como sesión de fuente (`9fbf15422`).
- Regresión amplia de precompilados (364 `validar-precompilado`): dos defectos de `4d5ae8d4e` en T09 (enums de una
  referencia de sesión de fuente duplicados con el catálogo; punteros `sysv_abi` variádicos que tomaban el plan en
  vivo y dejaban de rechazar la variante no verificada) corregidos en `9e835557a`; las 60 vuelven a pasar.
- `make_unique<T>(args)`: construye su resultado en el destino (`7a450565a`, `x4-fabrica-variadica` directo/dll) con
  argumentos literales o locales tipados (`ref x`); lib en `a9dcf9b2c`; `in x` (lvalue const) en `6532443b8`; expresiones aritméticas sobre literales y locales
  tipados (prvalue del tipo de las conversiones aritméticas usuales) en `7e1271585` (`x4-fabrica-expresiones`
  directo/dll/lib); llamadas a funciones de la unidad con resultado builtin en `597f69a45` (`x4-fabrica-llamadas`);
  siguen fuera miembros, nombres calificados y cadenas como argumentos.
- Columna L, avances posteriores: indexadores y sobrecargas por calificador de receptor (`cf8d58224`), plantillas de
  variable (`ee3aeb602`); cada uno con la batería de 364 `validar-precompilado`, `interop-sdk` y T18 en verde salvo
  las tres fallas conocidas (core512 sin su Core, diagnóstico `_BitInt` previo de tanda01, preserve-none previa).
- `e496a36fd`: plantilla publicada que llama a una plantilla de función C++ variádica (`x4-paquete-explicito` en lib).
  `fb00727cc`: corrige duplicados de símbolos de métodos C++ entre un consumidor con encabezado y la sesión de fuente
  (T10 acceso/integral, T19 dllimport-inline/referencias). Batería: interop-sdk, 364 `validar-precompilado` y todas las
  tandas 10–19, verdes salvo las fallas conocidas.
- Plantillas miembro C++ en lib: la causa no es NSMeta. Una llamada a una plantilla de función miembro dentro de un
  cuerpo genérico NativeSharp falla también en directo ("Stage 34 cannot map identifier/member expression to Stage 27
  binding"; las plantillas de función libres sí funcionan en un genérico). Falta el enlace de la llamada de extensión
  en la especialización genérica tras la ronda de deducción `@member`. Pendiente como limitación general de X2.
  Diagnóstico afinado: el análisis del cuerpo sí enlaza la extensión en la ronda 1 (en la ronda 0 aún no existe); la
  bajada de la especialización genérica baja otro nodo de sintaxis (no el analizado), cuyo acceso miembro nunca
  pasa por el análisis de extensión; en un método no genérico la misma llamada funciona (`lookupSource_` no es la
  causa: solo reparte el trabajo entre hilos).
  Traza (2026-11-24): en `G.Run<T>` el análisis enlaza la extensión en los nodos de la llamada (ronda 0 sin
  extensión, ronda 1 con extensión y selección), siempre los mismos punteros entre rondas; HIR baja otro nodo en la
  misma posición del fuente que nunca pasa por `bindExtensionMember`. Indexar la selección por posición no lo
  corrige: el nodo bajado no se analiza por esa vía. Falta encontrar qué copia del cuerpo baja la especialización.
- `e975d0c50`: deducción de argumentos de plantilla de clase con argumentos literales (`new std.pair(40, 2)`,
  `x4-deduccion-clase` directo/dll/lib, guía de deducción incluida); con locales de tipo builtin explícito en `aece10260`. Pendiente con otros argumentos
  (`var`, campos, parámetros).
- Casos X2 de retorno (`x2-retorno-*`, `x2-constref-prvalue`) en lib: su `preludio` declara funciones globales del
  productor; aun públicas, publicar una función NativeSharp que devuelve por valor un record C++ no trivial a otro
  módulo no estaba soportado. Resuelto en lib en `c1a22f627`: la causa era que la verificación no declaraba los records de
  una sesión de fuente (y las funciones de la prueba deben vivir en un namespace); nuevos `x2-retorno-ns-publico` y
  `x2-constref-prvalue-publico`. Pendiente en dll (el consumidor no enlaza un operador C++ del productor).
  Traza: el programa del consumidor dll referencia los adaptadores de todos los operadores miembro de los records de
  la sesión (`operator()`, `operator*`, `operator==`), no solo los que usa; en dll la biblioteca C++ del productor no
  se enlaza, así que un operador no inline queda sin resolver. Resuelto en `cbfb70bcb`: la sesión de fuente de un proyecto
  precompilado genera solo los adaptadores que alcanza una llamada; los dos casos pasan también en dll.
- X3 en lib (clases NativeSharp del productor que derivan de clases C++ o implementan interfaces C++): el consumidor no
  sabe que la clase deriva de su base C++ (`invalid explicit reference conversion from Leaf to lib.Node*`); la
  metadata tiene `baseTypeKey`, pero publicar la base C++ exige también el subobjeto, la tabla virtual y los
  adaptadores de overrides que X3-01 genera en el productor. Capacidad nueva de NSMeta, pendiente.
  Notas de diseño: en directo la base C++ es un campo oculto `__nsc_cpp_interface_vptr` con `cppInterfaceTypeOrdinal`
  (todas las interfaces comparten ese nombre; de ahí el choque de `x3-com-multiple` en lib), y la conversión al puntero
  C++ busca ese campo. El consumidor reconstruye los miembros de una clase precompilada desde su superficie publicada,
  que no incluye la base C++; incluirla haría que el consumidor regenere los adaptadores y la vtable que el productor
  ya compiló. Falta decidir cómo se comparten esos adaptadores (exportarlos del productor o marcarlos externos).
  Resuelto en `94af1fc88`: el productor publica el puntero oculto como puntero al tipo C++ (con nombre distinto
  por interfaz adicional) y el consumidor restaura el enlace; los adaptadores, la vtable y los límites C de una
  clase precompilada se toman de la biblioteca del productor (externos). El constructor implícito exportado no
  construye la base C++ (como la construcción implícita en directo) y un record C++ con bases viaja como sesión
  de fuente. Los 11 casos positivos de `limitaciones_x3.py` corren también en lib (182 resultados en verde).
- `cc8504a04`: X4-05 en lib. En una plantilla publicada, `lib.Box<P>` con un struct NativeSharp del productor se refleja en el
  consumidor sin encabezado desde la superficie publicada del productor (que escribe los campos con `at(n) align(n)`);
  `x4-struct-ns-argumento-publicado` en directo/dll/lib. `x4-exceptions-false` ya pasaba en lib (prueba forzada).
- `ff6dbfd08`: X2-06 en lib. Un struct NativeSharp con un campo C++ trivial (`Holder { lib.P p; }`) recibía drop glue porque
  la familia de vida del record declara un destructor aunque sea trivial; la superficie precompilada lo publicaba como
  destructor y el consumidor lo volvía move-only. Un record con copia, asignación y destrucción triviales ya no cuenta
  como destructor cuando se alcanza como campo (el propio record conserva su contrato). `x2-campo-transportable` y
  `x2-clase-campo-cpp` también en lib.
- `f2553d000`: X1 en lib. Un record C++ solo declarado en el encabezado (`struct Fwd;`) hacía fallar la metadata del
  productor ("C++ record requires one definition"); ahora es un tipo opaco, como en C. `x1-registro-opaco` en
  directo/dll/lib y `x1-cpp` completo en lib.
- `b27e9a9ce`: X1-05 en lib. `(new lib.Acc()).take()` con `take() &&` fallaba en el consumidor sin encabezado: el record
  quedaba en el catálogo de valores, que no lleva la vida del temporal receptor. Un record con un método `&&` viaja ahora
  como sesión de fuente, como ya lo hacían las sobrecargas por calificador de receptor. `x1-receptor-rvalue` en lib.
- `1bd1ec222`: con `lib` forzado en todas las fixtures de interop-sdk, los casos de las tandas 2 y 10–16 que ya pasaban en lib
  (o corrían allí un cuerpo reducido por el ítem 13.11) corren ahora el cuerpo completo en lib. Las demás fallas
  forzadas son artefactos del arnés (funciones del preludio que el consumidor no ve; `__FILE__`/`__LINE__` exactos).
- `ebf2b4a0a`: ítem 5.9 en lib. `SIZE(Pt)` en una plantilla publicada fallaba en el consumidor sin encabezado (no
  importa `Pt`); en el cuerpo de un tipo precompilado un nombre que no es valor en el ámbito se pasa como tokens (el
  productor ya lo comprobó). La tanda 5 corre el cuerpo completo en lib, con las constantes de macro.

## X4: `Exceptions="false"` en lib (`3c74105e8`)

La casilla L de la fila `Exceptions="false"` / campos de clase de plantilla C++ pasa de P a A sin cambio de
compilador. Nuevo caso `x4-exceptions-false-bib` (solo lib): la clase NativeSharp con campos `lib.Box<int>` es del
productor (pública) y sus campos de plantilla C++ llegan al consumidor sin encabezado por la sesión de fuente; verde
O0–O3. El caso original conserva directo/dll con la clase privada: hacerla pública convierte dll en un consumidor que
llama a `lib.ct` no exportada (límite del arnés, no del compilador).

## R1: liberación y contratos en lib y DLL (`5502d326e`)

Las celdas L y DLL de «Liberación obligatoria de resultados `Owned`» (R1-04) y la DLL de `<Contract>` pasan de «—» a A
sin cambio de compilador: `r1-liberacion-publico` (funciones del preludio en un namespace público) y
`r1-contratos-bib` corren en lib y dll, O0–O3; `limitaciones_r1.py` completa: 44 casos verdes.

Auditoría de celdas «—» de la matriz (2026-11-24): varias estaban desactualizadas. `x2-retorno-ns-publico` ya
cubre L y DLL del retorno desde una función NativeSharp (fila 45) y `x4-plantilla-variable-double` L y DLL de las
plantillas de variable no constantes (fila 81). Los retornos `return o;`/`return move(o)` y los resultados de
métodos, getters y overrides pasan en lib y dll con funciones y clases públicas (validado; se registran tras los
commits de plantillas miembro, que comparten `limitaciones_x2.py`). El párrafo «Pendiente en la columna L» de la
sección NSMeta quedó superado: indexadores, prvalue hacia `const T&`, X3-01/04/07/08 y `Exceptions="false"` ya
están en A en L.

## X2: plantillas miembro C++ en cuerpos genéricos y en lib (`6b5621745`, `958a067fe`)

- `6b5621745`: una plantilla de función miembro llamada en un cuerpo genérico fallaba incluso en directo ("Stage 34
  cannot map identifier/member expression to Stage 27 binding"). La referencia de extensión se busca por la posición
  de fuente del miembro, que el clon del cuerpo de una especialización comparte con la plantilla; la llamada tomaba
  el miembro y el receptor de la plantilla. Ahora toma los suyos (`CallAnalyzer`). Caso
  `x2-plantilla-miembro-generica` (directo, dll, lib) y `x2-plantilla-miembro` también en lib.
- `958a067fe`: en lib, `lib.M.twice(20)` (estática) no sembraba el record (ningún valor lo nombra), así que su sesión
  de fuente no viajaba; ahora se siembra un record que declara plantillas miembro. `m.add<double>(1)` se tomaba por un
  método genérico faltante de la plantilla publicada; la sesión enumera los nombres de sus plantillas miembro
  (`memberTemplateNames`) y el recolector de dependencias los omite como a las plantillas de función C++.
- Validado: casos `x2-plantilla-miembro*` (todos los modos, O0–O3); batería amplia de cada cambio (interop-sdk, 364
  `validar-precompilado`, tandas 10–19): solo las fallas conocidas (`core512` y `tanda01_diagnosticos` de entorno o
  previas, `preserve-none` previa, 3 de `tanda-19-traslados` por el paquete de Core del entorno); `build-clang` `/WX`.
- La fila de plantillas miembro de la matriz pasa a A en L: no queda ninguna celda P en la matriz.

## X2-05: retornos y resultados de métodos en lib y DLL (`f5ff7f691`)

`x2-retorno-local-publico`, `x2-retorno-move-publico` y `x2-resultado-metodo-publico` (lib, dll, O0–O3; 24 verdes)
llevan a A las celdas L y DLL de `return o;`, `return move(o)` y de los resultados construidos de métodos, getters y
overrides. Solo fixture.

## STL en lib (`2ebcd7bd1`)

Defecto encontrado al auditar las celdas «—»: una biblioteca precompilada cuya plantilla publicada usaba la STL
fallaba su propia verificación de metadata ("selected C function has multiple matching observations"). El adaptador
de miembro de `std::tuple<>` (asignación) se observa dos veces, por el record y por un `using` de su base, con el
mismo símbolo, identidad, firma canónica y proyección: es una sola función. Se aceptan esos duplicados; los que
difieren siguen siendo error, que ahora nombra la función. `pruebas-tanda-18-stl` tiene modo `--lib` (cada caso es
una plantilla publicada; el consumidor sin encabezado la instancia), igual a Clang O0–O3. Batería amplia: solo las
fallas conocidas (una primera corrida de interop-sdk se invalidó porque `comun.py` estuvo roto durante ella; se
repitió completa). Filas `std::string` y `list`/`map`/`unordered_map` de la matriz: L en A.

## X2: igualdad implícita rechazada también en lib (`23c992035`)

`comun.rechazo_lib` (el arnés compartido; `limitaciones_r1` conserva su copia) compila el cuerpo como plantilla
publicada y exige el rechazo en el consumidor sin encabezado. `x2-igualdad-implicita` lo usa: `a == b` sobre un record
C++ sin `operator==` se rechaza igual en lib. Solo fixture.

## R3: excepciones en lib (`312c491e9`)

`pruebas-tanda-15-excepciones --lib`: cada cuerpo es una plantilla publicada por una biblioteca precompilada; las
excepciones C++ que atraviesan callbacks NativeSharp, `finally`, destrucción de locales C++ durante el desenrollado,
`noexcept`/sin `catch` (terminación), `catch` NativeSharp que no atrapa C++ y `panic` en callback coinciden con Clang
O0–O3. Sin cambio de compilador. La fila R3 sigue «—» en L hasta cubrir arranque, TLS, enlace y recursos.

## R3 en lib: TLS, recursos y arranque (`7149aefd5`, `02fdc7f54`, `a3cb6ec56`)

Modo `--lib` en `pruebas-tanda-16-tls` (thread_local dinámico desde dos hilos), `pruebas-tanda-15-recursos` (puntero
propietario no copiable, allocator con estado, `std::unique_ptr`/`std::allocator`, generador `co_yield`; los rechazos
NS3020 siguen en directo) y `pruebas-tanda-16-arranque` (constructor global, Main y destructor en orden, `cmd`; los
negativos siguen en directo): el cuerpo es una plantilla publicada por una biblioteca precompilada y el consumidor sin
encabezado la instancia; iguales a Clang O0–O3, sin cambio de compilador. `pruebas-tanda-16-enlace` ya consumía un
módulo NativeSharp precompilado. Falta en L, para cerrar la fila R3: los constructores que lanzan
(`pruebas-tanda-18-constructores --lib`), que dependen de la corrección de construcción en curso.

## R2: caché de un productor lib (`e187e18de`)

Un productor `lib` compilado con `--artifact-cache` reutiliza su artefacto solo cuando nada cambió (frío, caliente
reutilizado, encabezado transitivo y `Define` cambiados no reutilizados) y su consumidor ve cada valor (42, 42, 41,
43). Solo fixture. La fila R2 pasa a A en L. `r2-cache-dll` (siguiente commit) repite los cuatro pasos con el
productor como DLL: la fila pasa a A también en DLL.

## Construcción de clases precompiladas con campos o base C++ (`ea3f82477`)

Defectos encontrados al llevar a lib `pruebas-x3-construccion-excepcion` y `pruebas-tanda-18-constructores`:

- Un consumidor sin encabezado rechazaba `new D()` de una clase precompilada con un campo C++ no trivial inicializado
  ("has no accessible default constructor"): veía el campo sin su inicializador. Los campos de una clase precompilada
  ya no se exigen ni se emiten en el consumidor (los construye el constructor del productor).
- El constructor implícito que exporta una biblioteca para una clase derivada de C++ no construía la base (sin su
  constructor ni su destructor). Ahora la construye como la construcción en línea de directo: de inmediato cuando la
  clase o su cadena implícita inicializa un campo; sin inicializadores, la conversión la enlaza como antes
  (`x3-base-destructor` conserva lo que espera en los tres modos).

Validado: `limitaciones_x3` 182; ambos validadores `--lib` iguales a Clang O0–O3; batería amplia (interop-sdk, 364
precompilados, tandas 10–19) solo con las fallas conocidas; `build-clang` `/WX`. Las filas 76 y R3 de la matriz pasan
a A en L: no queda ninguna celda «—» que no sea una decisión consciente (D) o N/A.

## X4: deducción de argumentos de plantilla de clase para `std::pair` (`f9b11abaf`)

El pendiente «deducción de argumentos de plantilla de clase (`new std.pair(40, 2)`)» ya estaba resuelto (la deducción
de clase funcionaba para las plantillas de la unidad, `x4-deduccion-clase`); queda cubierto con el caso
`pair-deduccion` de `pruebas-tanda-18-stl`, igual a Clang O0–O3 en directo y `--lib`. Solo validador.

## STL: argumentos de fábrica locales, campos y campos estáticos (`276f64ddc`)

El pendiente «siguen fuera miembros, nombres calificados y cadenas como argumentos» de `make<T>(args)` queda cerrado:

- La prededucción lee `owner.member` (owner es un local de un tipo de la unidad o el nombre del tipo) con el tipo
  builtin que declara el campo.
- Un local o campo builtin escrito sin `ref`/`in`, solo (`j`, `s.v`, `K.n`), se deducía como valor (`int&&`) pero la
  llamada rechazaba el lugar para ese `T&&`; para una especialización de plantilla C++ deducida de la llamada (y su
  constructor de fábrica) se enlaza ahora como ese valor (una copia), como ya lo hacía `j + 0`. Cualquier otro lvalue
  para un `T&&` sigue rechazado, como en C++.
- Las cadenas ya se deducían como `const char*` (`StringLiteral`).

`x4-fabrica-miembros` (directo, dll, lib; O0–O3). Batería amplia: solo las fallas conocidas; `build-clang` `/WX`.

## X4: deducción de clase desde locales `var`, parámetros y campos (`451f6d865`)

El pendiente «deducción de argumentos de plantilla de clase con otros argumentos (`var`, campos, parámetros)» queda
cerrado: la prededucción lee un parámetro `<builtin> name`, un `var name = <expresión aritmética>;` y un campo builtin
`s.v` de un tipo de la unidad. `x4-deduccion-clase-var` (directo, dll, lib) y `x4-deduccion-clase-campo` (directo,
dll). Limitación registrada: en lib la plantilla publicada se lee sin las demás declaraciones del productor, así que
un campo de un tipo del productor no se tipa ahí. Batería amplia: solo las fallas conocidas; `build-clang` `/WX`.

## Estado de cierre (HEAD `451f6d865`, 2026-11-24)

- Matriz: todas las celdas en A, N/A (con motivo) o D (decisiones conscientes conservadas: punteros a miembro,
  capturas y métodos de instancia como callback C, `setjmp`, T10-009, struct/arreglo/tupla NativeSharp con record C++
  no trivial). Ninguna celda P ni «—» sin estado.
- Pendientes de AVANCE resueltos en esta continuación: plantillas miembro en genéricos y en lib, STL en lib,
  construcción de clases precompiladas con campos o base C++, R1/R2/R3 en lib y DLL, retornos X2 en lib/DLL, igualdad
  implícita en lib, argumentos de fábrica (locales, campos, estáticos), deducción de clase (`std::pair`, `var`,
  parámetros, campos).
- V-07: pendiente; requiere un entorno de depuración interactiva; no se ejecutó y no se presenta como aprobado.
- Limitaciones menores registradas (no son celdas de la matriz):
  - (resuelto después, ver «X4: deducción desde un campo de un struct del productor en lib») la deducción desde un
    campo de un tipo del productor dentro de una plantilla publicada (lib);
  - (resuelto después, ver «X3-01: base C++ de una clase sin constructores ni campos») la construcción perezosa
    de la base C++ en `new X()` sin campos ni constructor.

## X3-01: base C++ de una clase sin constructores ni campos (decisión delegada por el usuario)

Decisión: alinear con C++. `new X()` de una clase derivada de C++ sin constructores ni inicializadores llamaba
directamente al constructor de `Object` y saltaba la construcción por defecto de la base C++ (X3-01): la base solo se
enlazaba en la primera conversión y, si nunca se convertía, su constructor y su destructor no corrían. Ahora la
construcción en línea construye las bases C++ de las clases por debajo del ancestro cuyo constructor llama (con su
limpieza si algo posterior lanza), y el constructor implícito que exporta una biblioteca las construye siempre (sin el
criterio de «inicializa un campo»).

- `x3-base-destructor` cambia lo que espera (con comentario): el objeto nunca convertido también destruye su base
  (2 y 12 en vez de 1 y 11), en directo, dll y lib.
- Nuevo `x3-base-destructor-campo` (directo, dll, lib): con un campo inicializado la base se construye una sola vez.

Validado: `limitaciones_x3` completo; `pruebas-x3-construccion-excepcion` y `pruebas-tanda-18-constructores` en
estática, DLL y lib frente a Clang; batería amplia (interop-sdk, 364 precompilados, tanda-1) solo con las fallas
conocidas; `build-clang` `/WX`.

## X4: deducción desde un campo de un struct del productor en lib

Una plantilla publicada que el consumidor instancia se lee sin las demás declaraciones del productor, así que
`new lib.Two(s.v, s.d)` con `S` del productor no tipaba el campo. La prededucción ahora toma, para un tipo que la
unidad no declara, el campo builtin (no puntero) del struct del mismo proyecto según su superficie publicada (los
mismos tipos que ya se escaneaban antes del análisis para los argumentos de plantilla NativeSharp).
`x4-deduccion-clase-campo` pasa también en lib.

Validado: `limitaciones_x4` completo; batería amplia (interop-sdk, 364 precompilados, tanda-1) solo con las fallas
conocidas; `build-clang` `/WX`.

## Estado de cierre (HEAD `0a3f66fe8`, 2026-11-24)

- `dba7a4485`: X3-01, base C++ de una clase sin constructores ni campos (decisión delegada: como C++).
- `0a3f66fe8`: NSMeta X4-06, deducción de plantilla de clase desde un campo de un struct del productor (lib).
- Las dos limitaciones menores registradas en el estado anterior quedan resueltas. Matriz sin celdas P.
- Único pendiente: V-07, que requiere un entorno de depuración interactiva; no se ejecutó y no se presenta como
  aprobado.
