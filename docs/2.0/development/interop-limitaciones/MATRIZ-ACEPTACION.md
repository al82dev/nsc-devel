# Matriz de aceptación — limitaciones de interoperabilidad (V-03, V-06)

Rama `interop-sdk-tandas` (local), HEAD `ver AVANCE.md`, base `1aec52cfd`. Actualizada 2026-11-24 (V-05, NSMeta,
Core 512 bits; cierre de celdas «—» HEAD `451f6d865`). Binario `A:\nsc-devel\build-integracion\bin\nsc.exe`
(Release, clang-cl, LLVM 23.1), NSLink `A:\nslink-devel\bin\release\nslink.exe`, Core
`A:\nsc-devel-fixes\build-integracion\nativesharp-libraries-build`. Fecha: 2026-11-22.

Estados: **A** aprobada (ejecutada y verde) · **D** rechazo con diagnóstico (restricción consciente) · **P** pendiente
(sin implementar en esta rama) · **B** bloqueada (requiere autorización o componente externo) · **N/A** no aplica.
Modos: D = directo, L = biblioteca `.lib` precompilada consumida sin encabezado, DLL = biblioteca compartida; O0–O3.

## C — Interoperabilidad C

| Capacidad | D | L | DLL | Estado | Evidencia |
|---|---|---|---|---|---|
| Calificadores por nivel de puntero (C1) | A | A | A | A | `limitaciones_c1.py` 26, `tanda02` |
| Macros: listas, llaves, dimensión, efectos, `__FILE__`/`__LINE__`, alias (C2) | A | A | A | A | `limitaciones_c2.py` 109, `tanda05`, `tanda16` |
| `_Float16`/Half, `_BitInt(N≤64)`, enums 128 bits, `_Atomic`, bit-fields volatile, `transparent_union`, flexibles de arreglos, sin prototipo, `static sysv_abi`, barreras C11, agregados vectoriales (C3) | A | A | A | A | `limitaciones_c3.py` 198 |
| `_BitInt(N)` 64<N<128 como valor de función (plan de Clang) y como campo de record (offset y alineación de Clang) | A | A | A | A | `limitaciones_c3.py` 222 (HEAD 1d6975ef4) |
| Callbacks y punteros a función C con `_Float16` (XMM) | A | A | A | A | `limitaciones_c3.py` 198 |
| Barreras C11 con orden en ejecución (switch como Clang) | A | A | A | A | `limitaciones_c3.py` 198 |
| Half en `...` promovido a double; puntero `sysv_abi` sin prototipo (C3-01, C3-08) | A | A | A | A | `limitaciones_c3.py` (HEAD ce0aff25f) |
| Callbacks y punteros C con `_BitInt(N)`, N < 128 (estrecho en `iN`, ancho en memoria; C3-02) | A | A | A | A | `limitaciones_c3.py` (`c3-bitint-callback`, `c3-bitint-callback-ancho`) |
| Vectores 512 bits (cdecl/ms_abi por memoria; portadores Float32x16, Float64x8, Int32x16, Int64x8 de Core) | A | A | A | A | `limitaciones_core512.py` (O0–O3, Core aparte `A:\temp\core512`); `__vectorcall` de 512 bits: D (ZMM) |
| Lambdas tipadas por el puntero C, `user_data`, variádicas, excepción en callback (C4) | A | A | A | A | `limitaciones_c4.py` 56, `pruebas-tanda-15-callbacks` |
| Capturas en callbacks C, métodos de instancia como callback | — | — | — | D | `limitaciones_c4.py` |
| Literales, `_Generic` char/signed char, `extern T a[]`, bit-fields en `...`, `Implementation` (C5) | A | A | A | A | `limitaciones_c5.py` 137 |
| Callbacks NativeSharp `sysv_abi`: escalares, records (registros, byval, sret) y enteros estrechos por el plan de Clang | A | A | A | A | `limitaciones_c5.py` 137 |
| Callback/puntero `sysv_abi` con `__int128` (`i128` directo, C5-07); nombre de tipo = typedef ante etiqueta homónima (C5-06) | A | A | A | A | `limitaciones_c5.py` |
| Local `var` de un valor C `signed char` en `_Generic` (C5-02) | A | A | A | A | `limitaciones_c5.py` (`c5-generic-local`) |
| Callback `sysv_abi` con record sin typedef en plantilla precompilada (sesión de fuente NSMeta) | A | A | A | A | `limitaciones_c5.py` (`c5-sysv-lambda-record` lib), `limitaciones_nsmeta.py` (`nsm-sysv-lambda`) |
| `setjmp` (D8) | — | — | — | D | `limitaciones_c5.py` |

## X — Interoperabilidad C++

| Capacidad | D | L | DLL | Estado | Evidencia |
|---|---|---|---|---|---|
| Sobrecargas, `= delete`, `explicit`, acceso, referencias, `const T&` con valor, receptor `&&` sobre temporal (X1) | A | A | A | A | `limitaciones_x1.py` 36, T10; L: `x1-cpp` completo y record solo declarado como tipo opaco (`x1-registro-opaco`) en `f2553d000` |
| Orden NativeSharp ante ambigüedad C++ (T10-009) | — | — | — | D | `limitaciones_x1.py` |
| Vida local, copia/movimiento explícitos, operadores en sitio, por valor no trivial como parámetro, prvalue construido en el parámetro (X2) | A | A | A | A | `limitaciones_x2.py` 52 (HEAD 0b2cee2ae), T11, `pruebas-x2-retorno-valor`; L: modo lib del mismo caso (NSMeta X4-06) |
| Retorno por valor de record no trivial construido en el local (excepción, retorno temprano, move-only) | A | A | A | A | `pruebas-x2-retorno-valor` (estática y DLL), `limitaciones_x2.py`; L y DLL sin encabezado: `cbfb70bcb`; funciones públicas en namespace (`c1a22f627`) |
| Temporal C++ como receptor (`lib.make(42).v`), destruido al final de la expresión completa | A | A | A | A | `limitaciones_x2.py` 44; L: modo lib del mismo caso (NSMeta X4-06) |
| Reasignación de un local C++ con el `operator=` que elige C++ (rvalue y lvalue) | A | A | A | A | `limitaciones_x2.py` 44; L: modo lib del mismo caso (NSMeta X4-06) |
| Operadores miembro (`*`, `()`) sobre un temporal C++ | A | A | A | A | `limitaciones_x2.py` 44; L: modo lib del mismo caso (NSMeta X4-06) |
| Retorno de un record C++ no trivial desde una función NativeSharp, construido en el destino | A | A | A | A | `limitaciones_x2.py` 44; L y DLL: `x2-retorno-ns-publico` |
| Resultado C++ construido de métodos, getters y overrides NativeSharp; receptor temporal de una función NativeSharp (X2-05) | A | A | A | A | `limitaciones_x2.py` (`x2-resultado-metodo`; L y DLL: `x2-resultado-metodo-publico`) |
| Prvalue record C++ hacia `const T&` (funciones y miembros; X2-05) | A | A | A | A | `limitaciones_x2.py` (`x2-constref-prvalue`); L y DLL sin encabezado: `cbfb70bcb`; funciones públicas en namespace (`c1a22f627`) |
| Métodos C++ (virtuales o no) que devuelven un record no trivial por valor (X2-05) | A | A | A | A | `limitaciones_x2.py` (`x2-metodo-resultado`); L: modo lib del mismo caso (NSMeta X4-06) |
| Métodos estáticos y operadores libres C++ que devuelven un record no trivial (X2-05) | A | A | A | A | `limitaciones_x2.py` (`x2-estatico-resultado`, `x2-operador-resultado`); L: modo lib del mismo caso (NSMeta X4-06) |
| Operadores de conversión C++ como cast explícito; cast inválido de record con diagnóstico (X2) | A | A | A | A | `limitaciones_x2.py` (`x2-conversion`, `x2-cast-record`); L: modo lib del mismo caso (NSMeta X4-06) |
| `foreach` sobre rangos C++ (begin()/end(), iteradores puntero o clase; X2) | A | A | A | A | `limitaciones_x2.py` (`x2-foreach-rango`); L: modo lib del mismo caso (NSMeta X4-06) |
| `foreach` sobre contenedores STL; operadores heredados y `!=` reescrito de C++20 (X2) | A | A | A | A | `pruebas-tanda-18-stl` (`array-foreach`, `vector-foreach`), `limitaciones_x2.py` (`x2-operador-heredado`); L: modo lib del mismo caso (NSMeta X4-06) |
| Igualdad implícita de records C++ rechazada (C++ no la define; X2) | A | A | A | A | `limitaciones_x2.py` (`x2-igualdad-implicita`; L: `rechazo_lib`, `23c992035`) |
| Plantillas de función miembro C++ (deducidas y explícitas, instancia y estáticas; X2; también dentro de un cuerpo genérico) | A | A | A | A | `limitaciones_x2.py` (`x2-plantilla-miembro`, `-generica`, `-estatica`, `-explicita`); genérico: `6b5621745`; L: `958a067fe` |
| `std::string` (construcción desde literal, append, +=, índice; X2) | A | A | A | A | `pruebas-tanda-18-stl` (`string`; L: `--lib`, `2ebcd7bd1`), `limitaciones_x2.py` (`x2-constructor-puntero-const`) |
| Operadores plantilla libres C++ y `!=` reescrito desde `==` libre (C++20; X2) | A | A | A | A | `pruebas-tanda-18-stl` (`string-compare`), `limitaciones_x2.py` (`x2-operador-plantilla-libre`); L: sesión de fuente (`5efb9fc6c`) |
| Relacionales C++20 reescritos desde `operator<=>` (X2) | A | A | A | A | `pruebas-tanda-18-stl` (`string-order`), `limitaciones_x2.py` (`x2-relacional-reescrito`); L: sesión de fuente (`5efb9fc6c`) |
| Sobrecargas `const T&`/`T&&` de métodos e indexadores C++ (X2) | A | A | A | A | `limitaciones_x2.py` (`x2-referencias-sobrecarga`); L: sesión de fuente (NSMeta X4-06) |
| `std::list`, `std::map`, `std::unordered_map`; emisión inline y de adaptadores robusta (X2) | A | A | A | A | `pruebas-tanda-18-stl` (`list`, `map`, `unordered-map`; L: `--lib`, `2ebcd7bd1`) |
| `std::set`; `using Base::name`, plantillas miembro con parámetros por defecto, resultado descartado (X2) | A | A | A | A | `pruebas-tanda-18-stl` (`set`), `limitaciones_x2.py` (`x2-using-base-plantilla-defecto`); L: modo lib del mismo caso (NSMeta X4-06) |
| Sobrecargas por calificador de receptor (`&`, `const&`, `&&`) de métodos C++ (X2) | A | A | A | A | `limitaciones_x2.py` (`x2-calificadores-receptor`); L: sesión de fuente (NSMeta X4-06) |
| `std::optional`; un constructor no mapeable no descarta la familia de vida (X2) | A | A | A | A | `pruebas-tanda-18-stl` (`optional`), `limitaciones_x2.py` (`x2-constructor-no-mapeable`); L: modo lib del mismo caso (NSMeta X4-06) |
| `return o;` de un local C++ (movimiento implícito) | A | A | A | A | `limitaciones_x2.py` 44; L y DLL: `x2-retorno-local-publico` |
| `return move(o)` (movimiento implícito) | A | A | A | A | `limitaciones_x2.py` 44; L y DLL: `x2-retorno-move-publico` |
| Struct NativeSharp con campo C++ transportable (X2-06) | A | A | A | A | `limitaciones_x2.py` (`x2-campo-transportable`); L: el campo trivial no le da drop glue y sigue copiable, `ff6dbfd08` |
| Clase NativeSharp con campos de record C++ no trivial (construcción, `operator=`, destrucción; X2-06) | A | A | A | A | `limitaciones_x2.py` (`x2-clase-campo-cpp`); L: `ff6dbfd08` |
| `operator=` borrado; record no trivial en plantilla precompilada; struct, arreglo o tupla NativeSharp con record C++ no trivial (diagnóstico claro) | — | — | — | D/B | `limitaciones_x2.py` (rechazos `x2-struct-campo-cpp`, `x2-arreglo-cpp`, `x2-tupla-cpp`) |
| Herencia, offsets, dispatch virtual, COM consumido (X3) | A | A | A | A | `limitaciones_x3.py` 42, T12 (`--dll`); L: modo lib del mismo caso (NSMeta X4-06) |
| Records con bases virtuales como locales (adaptadores de Clang) | A | A | A | A | `limitaciones_x3.py` 42; L: modo lib del mismo caso (NSMeta X4-06) |
| Clase NativeSharp que implementa una interfaz C++ abstracta / COM entrante (X3-04/X3-07) | A | A | A | A | `limitaciones_x3.py` (`x3-com-entrante`); L: `94af1fc88` (NSMeta X4-06) |
| Derivar de clase C++ polimórfica con datos, destructor trivial o no (X3-01) | A | A | A | A | `limitaciones_x3.py` (HEAD 196f1237e; base construida aunque nunca se convierta: `x3-base-destructor`, `x3-base-destructor-campo`, X3-01 tras `451f6d865`); L: `94af1fc88` (NSMeta X4-06) |
| `base(args)` hacia el constructor de una base C++ (X3-01) | A | A | A | A | `limitaciones_x3.py` (HEAD 0664871a1); L: `94af1fc88` (NSMeta X4-06) |
| Clase NativeSharp con varias interfaces C++ (COM; X3-07) | A | A | A | A | `limitaciones_x3.py` (`x3-com-multiple`); L: `94af1fc88` (NSMeta X4-06) |
| `Rtti="false"` en la referencia C++ (X3-08) | A | A | A | A | `limitaciones_x3.py` (`x3-rtti-false`); L: `94af1fc88` (NSMeta X4-06) |
| `Exceptions="false"` en la referencia C++; campos de clase de plantilla C++ (X4) | A | A | A | A | `limitaciones_x4.py` (`x4-exceptions-false`, `x4-exceptions-false-bib`: lib con la clase del productor) |
| Construcción que falla: base C++ y campos C++ construidos se destruyen (X3-01/X2-06) | A | A | A | A | `pruebas-x3-construccion-excepcion`, `pruebas-tanda-18-constructores`; L: `--lib` de ambos validadores (`ea3f82477`) |
| Punteros a miembro | — | — | — | D | `limitaciones_x3.py` |
| Plantillas: deducción, clases, parciales, NTTP, `requires` (X4) | A | A | A | A | `limitaciones_x4.py` 23, `tanda13`; L: modo lib del mismo caso (NSMeta X4-06) |
| Plantillas de variable `constexpr` enteras, bool, `float` y `double` como valores (X4-04) | A | A | A | A | `limitaciones_x4.py` (HEAD 72b832d93); L: sesión de fuente (NSMeta X4-06) |
| Struct NativeSharp como argumento de plantilla de clase C++ sobre su espejo (X4-05) | A | A | A | A | `limitaciones_x4.py` (`x4-struct-ns-argumento`, `x4-struct-ns-argumento-publicado`), T13 structs-ns; L y DLL: `cc8504a04` (NSMeta X4-06) |
| Plantillas de variable no constantes como su almacenamiento (accesora, X4-04) | A | A | A | A | `limitaciones_x4.py` (HEAD fa6a78a57); L: `x4-plantilla-variable-double` (sesión de fuente) |
| Valor para el `const T&` de un constructor C++ (literal o local sin `in`; `std::pair<int, int>(40, 2)`; X1-04) | A | A | A | A | `limitaciones_x1.py` (`x1-constructor-constref`, `9c0c512bd`) |
| Deducción de argumentos de plantilla de clase con argumentos literales (`new std.pair(40, 2)`, guías; X4) | A | A | A | A | `limitaciones_x4.py` (`x4-deduccion-clase`, `e975d0c50`); locales de tipo builtin explícito (`aece10260`); `var`, parámetros y campos (`x4-deduccion-clase-var`, `-campo`, `451f6d865`; campo de un struct del productor en lib tras `dba7a4485`) |
| Fábrica variádica que devuelve un record no trivial (`std::make_unique<int>(42)`; argumentos literales o `ref`) | A | A | A | A | `limitaciones_x4.py` (`x4-fabrica-variadica`, lib `a9dcf9b2c`, `7a450565a`) |
| Plantilla de función variádica con argumentos explícitos que no fijan el paquete (`sum<int>(40, 1, 1)`; X4) | A | A | A | A | `limitaciones_x4.py` (`x4-paquete-explicito`, `5abc87b39`) |
| Catálogo de especializaciones y records C++ no triviales o polimórficos para consumidores sin encabezado (sesión de fuente, X4-06/R2) | N/A | A | A | A | `limitaciones_nsmeta.py` 56 (lib y dll, O0–O3); D: N/A (directo no usa el catálogo NSMeta) |

## R — Contratos, catálogos y runtime

| Capacidad | D | L | DLL | Estado | Evidencia |
|---|---|---|---|---|---|
| `<Contract>`: Retained, Transfer, Owned, NonNull/Nullable/Out (R1) | A | A | A | A | `limitaciones_r1.py` 44; L: contratos en la metadata (R1-05), `rechazo_lib`; L y DLL: `r1-contratos-bib` (`5502d326e`) |
| Liberación obligatoria de resultados `Owned` en todo camino (R1-04) | A | A | A | A | `limitaciones_r1.py` 44; L y DLL: `r1-liberacion-publico` (`5502d326e`) |
| Contratos en NSMeta (R1-05): el consumidor precompilado aplica Retained, Transfer, Out, NonNull/Nullable y Owned | N/A | A | N/A | A | `limitaciones_r1.py` (`rechazo_lib`, 6); D: N/A (directo no usa el catálogo); DLL: N/A (el consumidor de una DLL no instancia plantillas publicadas: no hay sitio de rechazo en él) |
| Invalidación y reutilización de caché (`--artifact-cache`), prefijo de depuración, diagnóstico de globales (R2) | A | A | A | A | `limitaciones_r2.py` 5, T17 caché/diagnósticos; L y DLL: `r2-cache-lib`, `r2-cache-dll` (productor en caché) |
| Excepciones, arranque, TLS, enlace/LTO, recursos, constructores que lanzan (R3) | A | A | A | A | validadores T15/T16/T17/T18; L: `--lib` de excepciones (`312c491e9`), TLS (`7149aefd5`), recursos (`02fdc7f54`), arranque (`a3cb6ec56`), constructores (`ea3f82477`); enlace ya con módulo precompilado |

## V — Integración

| Tarea | Estado | Evidencia |
|---|---|---|
| V-01 T19-026 | A | `validar-indice-expresion.py` y `validar-indice-llamada.py`, 5 ejecuciones concurrentes cada uno, verdes con `NSC_LINKER` (HEAD `9dda76763`) |
| V-02 fixtures históricos T19-027/037 | A | los mismos validadores, presentes y verdes |
| V-04 encabezados reales | A | `pruebas-tanda-18-zlib`, `-icu`, `-stl`, `-paquete` verdes |
| V-05 regresión integral | A | ctest 1606 (241 fallas: 187 previas/entorno con la base, 54 sensibles al entorno, 0 de la rama) e interop 1363 scripts (fallas de la rama corregidas: `35265db94`, `ee3bc3aae`); `A:\temp\v05\informe-ctest.md` |
| V-07 depuración interactiva | B | pendiente: requiere un entorno de depuración interactiva; no se ejecutó ni se presenta como aprobado |

No se declara interoperabilidad universal completa: las filas P/B/D enumeran los huecos conscientes y lo no verificado.
