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
| `_Complex` sin portador NativeSharp | inventario | T19-001 |
| Arreglos de longitud variable en parámetros | inventario | T19-001 |
| `null` a un puntero a función C (campo o argumento) *(tanda 19: [nulos](registro-commits/t19-nulos.md))* | T18 zlib | T19-002 |
| Dirección de un global `_Atomic` TLS | T08 | T19-002 |
| Expandir macros de función; macros que dan cadenas, punteros, records o tipos | T14-002, T14-003, T18 zlib | T19-003 |
| Bit-fields de tipo enum o de más de 64 bits | T14-006 | T19-004 |
| `new` sobre un struct C; `sizeof` como `int` sin conversión | T18 zlib | T19-002 |

## Convenciones y replay (T09-003)

| Pendiente | Origen | Tarea |
|---|---|---|
| Punteros a función, records por valor y variádicas con `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` | T09-003 | T19-028 |
| Unión C de solo `float` por valor con `regcall` | T09-003 | T19-029 |
| Replay sin encabezado de `__unaligned`, atributos distintos de la alineación, `#pragma pack` y alineación con bit-fields | T09-003 | T19-030 |
| Tamaños de objeto ocultos y flujos de receptor C++ en formas no registradas | T09-003 | T19-031 |
| Bloqueado por Clang 23.1.0: `preserve_all` e `intel_ocl_bicc` en C++; variádicas C++ `preserve_none` | T09-003 | T19-032 |

## Nombres, sobrecargas y referencias C++

| Pendiente | Origen | Tarea |
|---|---|---|
| Alias de namespace como prefijo de tipo (`icu.UnicodeString`) y sus enumeradores *(tanda 19, cubierto: [alias](registro-commits/t19-alias.md), [enumeradores](registro-commits/t19-alias-enumeradores.md))* | T18 ICU | T19-005 |
| Alias y `using` de variables y tipos; `using` en el ámbito global; plantilla con un nombre de `using` *(tanda 19, cubierto: [nombres](registro-commits/t19-nombres.md))* | T10-002, T13-006 | T19-005 |
| Sobrecargas que solo difieren en `int` y `long` *(tanda 19, cubierto: [funciones de namespace](registro-commits/t19-sobrecargas.md), [métodos y constructores](registro-commits/t19-sobrecargas-predeterminados.md))* | T17-003 | T19-006 |
| Alternativas en las llamadas a métodos de clases C++ *(tanda 19, cubierto: [métodos y constructores](registro-commits/t19-sobrecargas-predeterminados.md))* | T17-003 | T19-006 |
| Predeterminados de enum, de record o no constantes *(tanda 19, cubierto: [predeterminados](registro-commits/t19-sobrecargas-predeterminados.md); en una plantilla precompilada, los de constructores siguen en T19-016)* | T10-010 | T19-006 |
| Retornos `const T&` y `T&` (`std::min`, `std::array::at`); operandos `T&` en operadores; temporal con `const T&` *(tanda 19, retornos `const T&` de funciones y métodos: [referencias](registro-commits/t19-referencias.md))* | T10-003, T10-016, T11-019, T18 STL | T19-007 |
| Parámetros `T&&` distintos del movimiento | T11-016 | T19-007 |
| `T&` y `const T&` en métodos estáticos | T10-005, T10-013 | T19-007 |
| Records y enums anidados privados o protegidos; enumerador no scoped nombrado por el record *(tanda 19, cubierto: [nombres](registro-commits/t19-nombres.md))* | T10-002, T10-007 | T19-005 |
| Global de tipo enum sin que el programa nombre el enum *(tanda 19, cubierto: [nombres](registro-commits/t19-nombres.md))* | T10-004 | T19-005 |
| Funciones `inline` declaradas `dllimport` | T10-012 | T19-008 |

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
| Campos de una `union` C++ *(tanda 19, miembros escalares: [uniones](registro-commits/t19-uniones.md))* | T11-004 | T19-004 |
| Operadores miembro no const, heredados, de asignación, llamada, subíndice y flecha | T12-004 | T19-013 |
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
| Especialización C++ en la API pública de un módulo precompilado; objetos C++ y métodos heredados en una plantilla precompilada (también constructores con sus predeterminados, T19-006) | T13-001/020 | T19-016 |

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

Fuera del alcance por la línea base (T18-001, T18-002): C23, C++23, Objective-C, OpenMP, HLSL, CUDA y el ABI
Itanium.
