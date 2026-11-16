# Pendientes de la interoperabilidad C y C++ (T18-021)

Este documento reúne los pendientes de las tandas 8 a 18 que siguen abiertos. Se revisaron los cierres de las
tandas 10 a 17, el [inventario](INVENTARIO-INTEROP-C-CPP.md) y los límites de las pruebas integrales de la
tanda 18. Se quitaron los que una tanda posterior cerró, por ejemplo:

- los constructores que lanzan (T18-011);
- `thread_local` dinámico (T16-006);
- el CRT en el ejecutable (T16-002/003).

Cada fila tiene una tarea específica de la [sección 19 de TRACKING](TRACKING.md#19-pendientes-de-la-interoperabilidad-c-y-c). Ningún
pendiente se cuenta como cubierto: hoy cada uno se rechaza con diagnóstico o deja el nombre sin proyectar.

## C

| Pendiente | Origen | Tarea |
|---|---|---|
| `_Complex` sin portador NativeSharp | inventario | T19-001 |
| Arreglos de longitud variable en parámetros | inventario | T19-001 |
| `null` a un puntero a función C (campo o argumento) | T18 zlib | T19-002 |
| Dirección de un global `_Atomic` TLS | T08 | T19-002 |
| Expandir macros de función; macros que dan cadenas, punteros, records o tipos | T14-002, T14-003, T18 zlib | T19-003 |
| Bit-fields de tipo enum o de más de 64 bits | T14-006 | T19-004 |
| `new` sobre un struct C; `sizeof` como `int` sin conversión | T18 zlib | T19-002 |

## Nombres, sobrecargas y referencias C++

| Pendiente | Origen | Tarea |
|---|---|---|
| Alias de namespace como prefijo de tipo (`icu.UnicodeString`) | T18 ICU | T19-005 |
| Alias y `using` de variables y tipos; `using` en el ámbito global; plantilla con un nombre de `using` | T10-002, T13-006 | T19-005 |
| Sobrecargas que solo difieren en `int` y `long` | T17-003 | T19-006 |
| Alternativas en las llamadas a métodos de clases C++ | T17-003 | T19-006 |
| Predeterminados de enum, de record o no constantes | T10-010 | T19-006 |
| Retornos `const T&` y `T&` (`std::min`, `std::array::at`); operandos `T&` en operadores; temporal con `const T&` | T10-003, T10-016, T11-019, T18 STL | T19-007 |
| Parámetros `T&&` distintos del movimiento | T11-016 | T19-007 |
| `T&` y `const T&` en métodos estáticos | T10-005, T10-013 | T19-007 |
| Records y enums anidados privados o protegidos; enumerador no scoped nombrado por el record | T10-002, T10-007 | T19-005 |
| Global de tipo enum sin que el programa nombre el enum | T10-004 | T19-005 |
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
| Campos y métodos privados o protegidos: no se importan, sin diagnóstico de acceso | T11-004/005 | T19-012 |
| Campos de una `union` C++ | T11-004 | T19-004 |
| Operadores miembro no const, heredados, de asignación, llamada, subíndice y flecha | T12-004 | T19-013 |
| Campos de una base virtual, conversión a una base virtual y construcción con base virtual | T12-001/003/009 | T19-014 |
| Referencia a la base (`ref`/`in` de `B`) desde un derivado | T12-003 | T19-014 |
| `typeid` y `dynamic_cast` escritos en NativeSharp | T12-010/011 | T19-014 |
| Un portador para punteros a miembro | T14-007 | T19-014 |

## Plantillas y constantes

| Pendiente | Origen | Tarea |
|---|---|---|
| Plantillas miembro o anidadas en un record, escritas desde NativeSharp | T13-004/005 | T19-015 |
| Plantillas de variable | T13-021 | T19-015 |
| Constantes de tipo record, arreglo o puntero | T13-022 | T19-015 |
| Ordenación parcial entre dos plantillas deducibles con la misma firma | T13-005 | T19-015 |
| Deducción con un struct NativeSharp como argumento; struct NativeSharp con campo C++, arreglo fijo o enum; structs genéricos | T13-005/007 | T19-015 |
| Sobrecargas de `std::midpoint` para punteros (conjunto incompleto) | T18 STL | T19-015 |
| Especialización C++ en la API pública de un módulo precompilado; objetos C++ y métodos heredados en una plantilla precompilada | T13-001/020 | T19-016 |

## Excepciones, invocables y corutinas

| Pendiente | Origen | Tarea |
|---|---|---|
| Atrapar una excepción C++ en NativeSharp o convertirla | T15-003 | T19-017 |
| Una excepción NativeSharp lanzada dentro de un callback, con la biblioteca núcleo | T15-013 | T19-017 |
| `std::function` y objetos invocables con estado | T15-010 | T19-018 |
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
| Tres validadores de la tanda 9 que fallan de forma conocida: `cpp-variadica-preserve-none` (dos) y `cpp-tamano-objeto` con carga | T09 | T19-026 |
| `gz*` de zlib y el resto de ICU (`UnicodeString`, `BreakIterator`) | T18 | T19-025 |

Fuera del alcance: heredar un tipo C++ desde NativeSharp (T12); C23, C++23, Objective-C, OpenMP, HLSL, CUDA y
el ABI Itanium (línea base).
