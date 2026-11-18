# Pendientes de la interoperabilidad C y C++ (T18-021)

Este documento reúne los pendientes de las tandas 8 a 18 que siguen abiertos. Se revisaron los cierres de las
tandas 10 a 17, el [inventario](INVENTARIO-INTEROP-C-CPP.md) y los límites de las pruebas integrales de la
tanda 18. Se quitaron los que una tanda posterior cerró, por ejemplo:

- los constructores que lanzan (T18-011);
- `thread_local` dinámico (T16-006);
- el CRT en el ejecutable (T16-002/003).

Cada fila tiene una tarea específica de la [sección 19 de TRACKING](TRACKING.md#19-pendientes-de-la-interoperabilidad-c-y-c). Ningún
pendiente se cuenta como cubierto: hoy cada uno se rechaza con diagnóstico o deja el nombre sin proyectar.

La [revisión del 2026-11-16](REVISION-CIERRES-INTEROP-C-CPP.md) agregó los pendientes que faltaban (T09-003 y
cuatro traslados perdidos), reabrió las casillas de las tandas 9 a 18 que los contienen y marcó con *(tanda 19)*
lo que esa tanda ya cubrió, con su registro.

## C

| Pendiente | Origen | Tarea |
|---|---|---|
| `_Complex` sin portador NativeSharp *(tanda 19, cubierto: [complejos y VLA](registro-commits/t19-c-complejos-vla.md))* | inventario | T19-001 |
| Aritmética, comparación y conversiones implícitas de C sobre `_Complex` *(tanda 19, cubierto: [complejos aritmética](registro-commits/t19-complejos-aritmetica.md))* | T19-001 | T19-039 |
| Arreglos de longitud variable en parámetros *(tanda 19, cubierto: [complejos y VLA](registro-commits/t19-c-complejos-vla.md))* | inventario | T19-001 |
| `null` a un puntero a función C (campo o argumento) *(tanda 19: [nulos](registro-commits/t19-nulos.md))* | T18 zlib | T19-002 |
| Dirección de un global `_Atomic` TLS *(tanda 19, cubierto: [atómicas](registro-commits/t19-atomicas-direccion.md))* | T08 | T19-002 |
| Campos, globales y tipos de función C con `_Atomic T*`; dirección de globales con punteros a dato calificado y de globales `volatile` *(tanda 19, cubierto: [atómicas punteros](registro-commits/t19-atomicas-punteros.md))* | T19-002 | T19-038 |
| Expandir macros de función; macros que dan cadenas, punteros, records o tipos *(tanda 19, cubierto: [macros](registro-commits/t19-macros.md))* | T14-002, T14-003, T18 zlib | T19-003 |
| Macros de función con `#` o `##` sobre sus argumentos (hecho; quedan `#`/`##` sobre `__VA_ARGS__`) | T19-003 | T19-040 ✔ |
| Macros de objeto de expansión no constante (`errno`, dirección de una global `dllimport`) (hecho; solo C) | T14-002, T19-003 | T19-041 ✔ |
| Argumentos de macro que son tipos (`SIZE(int)`) | T14-003, T19-003 | T19-042 |
| Bit-fields de tipo enum o de más de 64 bits *(tanda 19, cubierto: [bit-fields y uniones](registro-commits/t19-bitfields-uniones.md))* | T14-006 | T19-004 |
| `__int128` en punteros a función, callbacks y variádicas; bit-fields `volatile` de más de 64 bits | T19-004 | T19-044 |
| `new` sobre un struct C; `sizeof` como `int` sin conversión *(tanda 19, cubierto: [valores C](registro-commits/t19-c-valores.md))* | T18 zlib | T19-002 |

## Convenciones y replay (T09-003)

| Pendiente | Origen | Tarea |
|---|---|---|
| Punteros a función, records por valor y variádicas con `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` *(tanda 19, cubierto: [convenciones planificadas](registro-commits/t19-convenciones-planificadas.md); la variádica `intel_ocl_bicc` sigue en T19-032)* | T09-003 | T19-028 |
| Unión C de solo `float` por valor con `regcall` *(tanda 19, cubierto: [unión float regcall](registro-commits/t19-regcall-union-float.md))* | T09-003 | T19-029 |
| Replay sin encabezado de `__unaligned`, atributos distintos de la alineación, `#pragma pack` y alineación con bit-fields *(tanda 19, cubierto: [replay sin encabezado](registro-commits/t19-replay-sin-encabezado.md))* | T09-003 | T19-030 |
| Tamaños de objeto ocultos y flujos de receptor C++ en formas no registradas | T09-003 | T19-031 |
| Bloqueado por Clang 23.1.0: `preserve_all` e `intel_ocl_bicc` en C++; variádicas C++ `preserve_none`; variádicas `intel_ocl_bicc` (verificador de LLVM) | T09-003, T19-028 | T19-032 |

## Nombres, sobrecargas y referencias C++

| Pendiente | Origen | Tarea |
|---|---|---|
| Alias de namespace como prefijo de tipo (`icu.UnicodeString`) y sus enumeradores *(tanda 19, cubierto: [alias](registro-commits/t19-alias.md), [enumeradores](registro-commits/t19-alias-enumeradores.md))* | T18 ICU | T19-005 |
| Alias y `using` de variables y tipos; `using` en el ámbito global; plantilla con un nombre de `using` *(tanda 19, cubierto: [nombres](registro-commits/t19-nombres.md))* | T10-002, T13-006 | T19-005 |
| Sobrecargas que solo difieren en `int` y `long` *(tanda 19, cubierto: [funciones de namespace](registro-commits/t19-sobrecargas.md), [métodos y constructores](registro-commits/t19-sobrecargas-predeterminados.md))* | T17-003 | T19-006 |
| Alternativas en las llamadas a métodos de clases C++ *(tanda 19, cubierto: [métodos y constructores](registro-commits/t19-sobrecargas-predeterminados.md))* | T17-003 | T19-006 |
| Predeterminados de enum, de record o no constantes *(tanda 19, cubierto: [predeterminados](registro-commits/t19-sobrecargas-predeterminados.md); en una plantilla precompilada, los de constructores siguen en T19-016)* | T10-010 | T19-006 |
| Retornos `const T&` y `T&` (`std::min`, `std::array::at`); operandos `T&` en operadores; temporal con `const T&` *(tanda 19, cubierto: [referencias](registro-commits/t19-referencias.md), [referencias completas](registro-commits/t19-referencias-completas.md); los operadores compuestos libres siguen en T19-013)* | T10-003, T10-016, T11-019, T18 STL | T19-007 |
| Parámetros `T&&` distintos del movimiento *(tanda 19, cubierto: [referencias completas](registro-commits/t19-referencias-completas.md); los constructores desde una plantilla precompilada siguen en T19-016)* | T11-016 | T19-007 |
| `T&` y `const T&` en métodos estáticos *(tanda 19, cubierto: [referencias completas](registro-commits/t19-referencias-completas.md))* | T10-005, T10-013 | T19-007 |
| Records y enums anidados privados o protegidos; enumerador no scoped nombrado por el record *(tanda 19, cubierto: [nombres](registro-commits/t19-nombres.md))* | T10-002, T10-007 | T19-005 |
| Global de tipo enum sin que el programa nombre el enum *(tanda 19, cubierto: [nombres](registro-commits/t19-nombres.md))* | T10-004 | T19-005 |
| Funciones `inline` declaradas `dllimport` *(tanda 19, cubierto: [dllimport inline](registro-commits/t19-dllimport-inline.md))* | T10-012 | T19-008 |

## Clases y vida de objetos

| Pendiente | Origen | Tarea |
|---|---|---|
| Destructores que pueden lanzar en locales | T18-011 | T19-009 |
| Copia implícita de un record no trivial; movimiento con constructor de copia | T11-015, T11-016 | T19-009 |
| Tipos movibles no copiables devueltos por valor; `move` de un objeto C++ | T15-016 | T19-009 |
| Locales que Clang no construye directamente (`std::optional<int>`, `std::string_view`) | T18 STL | T19-009 |
| Constructores de `icu::UnicodeString` no importados; métodos que devuelven `UnicodeString&` | T18 ICU | T19-010 |
| Constructor plantilla (`std::unique_ptr`, `std::pair`) | T15-014, T18 STL | T19-010 |
| `new T` de un tipo C++ en el heap desde NativeSharp | T11-018 | T19-011 |
| Campos y métodos privados o protegidos: no se importan, sin diagnóstico de acceso *(tanda 19, campos y métodos de instancia: [acceso](registro-commits/t19-acceso.md))* | T11-004/005 | T19-012 |
| Campos de una `union` C++ *(tanda 19, cubierto: miembros escalares, [uniones](registro-commits/t19-uniones.md); miembros record triviales y uniones anónimas triviales, [bit-fields y uniones](registro-commits/t19-bitfields-uniones.md))* | T11-004 | T19-004 |
| Uniones C++ con miembros de vida no trivial | T11-004, T19-004 | T19-043 |
| Operadores miembro no const, heredados, de asignación, llamada, subíndice y flecha *(tanda 19, no const: [referencias completas](registro-commits/t19-referencias-completas.md))*; operadores compuestos libres | T12-004, T19-007 | T19-013 |
| Campos de una base virtual, conversión a una base virtual y construcción con base virtual | T12-001/003/009 | T19-014 |
| Referencia a la base (`ref`/`in` de `B`) desde un derivado | T12-003 | T19-014 |
| `typeid` y `dynamic_cast` escritos en NativeSharp | T12-010/011 | T19-014 |
| Un portador para punteros a miembro | T14-007 | T19-014 |
| Clases NativeSharp derivadas de clases C++; acceso protegido desde la derivada | T10-007, T12 | T19-035 |

## Plantillas y constantes

| Pendiente | Origen | Tarea |
|---|---|---|
| Plantillas miembro o anidadas en un record, escritas desde NativeSharp | T13-004/005 | T19-015 |
| Plantillas de variable | T13-021 | T19-015 |
| Constantes de tipo record, arreglo o puntero | T13-022 | T19-015 |
| Ordenación parcial entre dos plantillas deducibles con la misma firma | T13-005 | T19-015 |
| Deducción con un struct NativeSharp como argumento; struct NativeSharp con campo C++, arreglo fijo o enum; structs genéricos | T13-005/007 | T19-015 |
| Sobrecargas de `std::midpoint` para punteros (conjunto incompleto) | T18 STL | T19-015 |
| Especialización C++ en la API pública de un módulo precompilado; objetos C++ y métodos heredados en una plantilla precompilada (también constructores con sus predeterminados, T19-006, y `new` de un record C++ trivial, T19-001) | T13-001/020 | T19-016 |

## Excepciones, invocables y corutinas

| Pendiente | Origen | Tarea |
|---|---|---|
| Atrapar una excepción C++ en NativeSharp o convertirla | T15-003 | T19-017 |
| Una excepción NativeSharp lanzada dentro de un callback, con la biblioteca núcleo | T15-013 | T19-017 |
| `std::function` y objetos invocables con estado | T15-010 | T19-018 |
| Lambda escrita directamente como argumento de tipo puntero a función | T15-008 | T19-033 |
| Una corutina devuelta por valor | T15-017 | T19-018 |
| `std::allocator<int>::deallocate` desde NativeSharp | T15-015 | T19-018 |

## Arranque, enlace y distribución

| Pendiente | Origen | Tarea |
|---|---|---|
| Runtime de C++ dinámico (`/MD`) | T15-002, T16-002/009 | T19-019 |
| Ejecutar `.CRT$XCU` desde la entrada de NativeSharp sin el CRT; inspeccionar `.CRT$XDU`; símbolos débiles y `/include` en la selección de miembros | T16-003 | T19-019 |
| Diagnóstico al llamar a NativeSharp desde un constructor global C++ | T16-008 | T19-019 |
| `LoadLibrary`/`FreeLibrary` de una DLL C++ desde NativeSharp | T16-012 | T19-020 |
| Un hilo creado por NativeSharp que usa TLS C++ | T16-007 | T19-020 |
| Diagnóstico propio de TLS sin el CRT; directorio TLS con NSLink | T16-006 | T19-034 |
| LTO entre el programa NativeSharp y la biblioteca C++; bitcode con NSLink | T16-016 | T19-020 |
| Bibliotecas que solo distribuyen `.pch`; interfaces de módulo `.pcm` | T14-009/010 | T19-021 |
| La instalación de NSC no incluye NSLink, `lld-link`, `llvm-lib` ni los encabezados de recursos de Clang | T18-017 | T19-022 |

## Diagnóstico, depuración y caché

| Pendiente | Origen | Tarea |
|---|---|---|
| Ubicación C++ con la línea del encabezado y subrayado | T17-001 | T19-023 |
| Variables y miembros C++ que faltan al enlazar (solo el mensaje del enlazador) | T17-005 | T19-023 |
| `-fdebug-prefix-map` en el código C++ emitido; depuración interactiva | T17-007/008 | T19-023 |
| Costo del inventario de `names` y `members` en encabezados grandes | T17-010 | T19-024 |
| Caché en `nsc build` para proyectos C/C++ | T17-011 | T19-024 |

## Combinaciones y pruebas

| Pendiente | Origen | Tarea |
|---|---|---|
| Combinaciones sin fixture: corutina + excepción + DLL; plantilla + herencia virtual + caché | T18-006 | T19-025 |
| `cpp-tamano-objeto/validar-indice-expresion` falla con cinco procesos; los dos `cpp-variadica-preserve-none` reproducen el bloqueo de T19-032 | T09 | T19-026 |
| Traslados sin evidencia: `class` en el catálogo precompilado, globales y campos estáticos en plantilla precompilada, campos `thread_local`, excepciones a través de un adaptador y de una especialización, records anidados en plantillas, `vectorcall` con agregados vectoriales | T10, T11, T13, T09 | T19-036 |
| `gz*` de zlib y el resto de ICU (`UnicodeString`, `BreakIterator`) | T18 | T19-025 |
| 36 pruebas unitarias de NSC que fallan en el CTest completo sin una construcción anterior con qué compararlas | T18-014 | T19-027 |
| Las otras 153 pruebas de NSC que fallan o no se ejecutan (sin ejecutable, `NativeSharp.Core.IO`, selfhost, salida concisa, depuración, entorno) | T18-014 | T19-037 |

## Componentes que no se modifican

Restricción del usuario (2026-11-17): solo se modifican las fuentes y las pruebas de NSC. Nunca se modifican Clang
23.1.0 (`C:\clang-llvm`), NSLink ni ningún otro componente, y no se despliega en `C:`. Una capacidad que solo se
resolvería así queda pendiente, documentada y sin cerrar:

| Pendiente | Componente | Tarea | Qué se puede hacer en NSC |
|---|---|---|---|
| `preserve_all` e `intel_ocl_bicc` en C++; variádicas C++ `preserve_none`; variádicas `intel_ocl_bicc` | Clang (y su LLVM) | T19-032 | Nada: se conserva la reproducción del fallo de Clang; T09-003 sigue abierta mientras dependa de ella |
| Directorio TLS con NSLink | NSLink | T19-034 | El diagnóstico propio de TLS sin el CRT |
| NSLink, `lld-link`, `llvm-lib` y los encabezados de recursos de Clang en la distribución | NSLink, LLVM, Clang | T19-022 | Documentar su instalación separada |

Si otra tarea resulta necesitar un cambio fuera de NSC, se documenta aquí con la evidencia y la tarea sigue abierta.

Fuera del alcance por la línea base (T18-001, T18-002): C23, C++23, Objective-C, OpenMP, HLSL, CUDA y el ABI
Itanium.
