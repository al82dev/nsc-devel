# Informe de la interoperabilidad C y C++ de NativeSharp (T18-022)

Este informe cubre las tandas 8 a 18 de la rama `development-cpp-clang`. Dice qué consume hoy NativeSharp de C y
de C++ con evidencia, con qué pruebas integrales y qué sigue pendiente. El detalle por característica está en
el [inventario](INVENTARIO-INTEROP-C-CPP.md), los pendientes en
[PENDIENTES-INTEROP-C-CPP.md](PENDIENTES-INTEROP-C-CPP.md) y la distribución en
[DISTRIBUCION-INTEROP-C-CPP.md](DISTRIBUCION-INTEROP-C-CPP.md).

## Base

- Compilador de referencia: Clang oficial 23.1.0 (`x86_64-pc-windows-msvc`), dentro del proceso de NSC, sin
  modificar.
- Línea base: C17 (`Standard="c17"`) y C++20 (`Standard="c++20"`; `c++17` también se prueba).
- Runtime de C y C++: MSVC estático (`/MT`), toolset 14.52.36815, SDK 10.0.28000.0.
- Enlazadores:
  - NSLink 3.0.0, sin modificar;
  - `lld-link` 23.1.0 para el arranque del CRT, el bitcode y el TLS.
- NSMeta: un único contrato vigente.

Cada capacidad se comprobó frente a un control compilado por Clang que hace lo mismo, en estática y DLL, de
O0 a O3. Donde no aplica, el registro de la tanda dice por qué.

## Capacidades demostradas

**C** (tandas 8 y 9):

- funciones con y sin prototipo, variádicas y globales;
- `_Thread_local` y los tipos escalares, incluido `long double`;
- `enum`, `struct` y `union` con el layout de Clang;
- bit-fields, miembros flexibles y tipos incompletos;
- punteros con calificadores, arreglos y punteros a función como callbacks;
- `_Atomic`, `_Alignas` y `#pragma pack`;
- `static inline` en el encabezado y las convenciones de llamada que Clang acepta en x64 (`regcall`, `sysv_abi`, `preserve_*`).

**C++** (tandas 10 a 16):

- namespaces, sobrecargas, argumentos predeterminados constantes, referencias `T&` y `const T&`;
- operadores libres, `inline` y `enum class`;
- clases con campos, métodos y constructores, incluidos los que pueden lanzar (T18), y destructores al salir
  del ámbito; copia y movimiento explícitos;
- herencia simple y múltiple, despacho virtual, clases abstractas y métodos de bases virtuales;
- RTTI dentro de la biblioteca;
- plantillas de clase y de función, parámetros no tipo, paquetes, conceptos y especializaciones;
- `constexpr` y `consteval` escalares;
- excepciones que atraviesan NativeSharp con el modelo de Microsoft, lambdas sin capturas como callbacks,
  punteros inteligentes y allocators propiedad de la biblioteca, y corutinas;
- macros de objeto, bit-fields y atributos de layout;
- constructores globales con el CRT, `thread_local` dinámico desde varios hilos y `std::atomic` entre hilos.

**Enlace y herramientas** (tandas 16 y 17):

- estática, DLL y varias DLL; enlace separado con símbolos duplicados detectados; ThinLTO con `lld-link`;
- rechazo de los inicializadores dinámicos que nadie ejecutaría;
- diagnósticos con la ubicación C++ y las candidatas de la sobrecarga;
- símbolos que faltan al enlazar, con el nombre C++;
- información de depuración CodeView del código C++ emitido;
- caché de compilación con la identidad de cada archivo que carga la sesión.

## Pruebas integrales (tanda 18)

| Biblioteca | Qué ejercita | Resultado |
|---|---|---|
| zlib 1.3.2 (C), 11 unidades | búferes y longitudes, cadena devuelta, struct con punteros a función, API de flujo, asignador del CRT | 137 igual que Clang, estática y DLL, O0–O3 |
| ICU4C 80 `icuuc` (C++), 203 unidades | compilación separada, destructores virtuales, memoria propia, constructores que lanzan | 1505555 igual que Clang, estática y DLL, O0–O3 |
| STL de MSVC (plantillas en encabezados) | `std::array`, `numeric_limits`, `integral_constant`, `<bit>`, `<numeric>` | 14236 igual que Clang, O0–O3 |
| Paquete instalado en un entorno limpio | NativeSharp, C, C++ con el CRT, DLL y biblioteca estática | 42 en todos los casos; tres negativos con su motivo |

## Regresión

La validación agrupada de la tanda 18 está en [TANDA-18-VALIDACION-AGRUPADA.md](TANDA-18-VALIDACION-AGRUPADA.md):

- estática: las tandas 18 a 10 pasan completas y la tanda 9 da 1094 de 1099 (la línea base);
- DLL: 61 de 61;
- CTest de NSC: 515 de 704 pasan. De las 189 fallas, 153 tienen una causa ajena a la interoperabilidad. Las 36
  unitarias sin línea base siguen en T19-027.

## Limitaciones

[PENDIENTES-INTEROP-C-CPP.md](PENDIENTES-INTEROP-C-CPP.md) reúne los pendientes que siguen abiertos. Cada uno
tiene su tarea en la sección 19 de TRACKING (T19-001 a T19-037). La [revisión del
2026-11-16](REVISION-CIERRES-INTEROP-C-CPP.md) reabrió las casillas de las tandas 9 a 18 cuyo alcance conserva
pendientes. Las limitaciones más visibles para quien use una biblioteca real:

- NativeSharp no atrapa excepciones C++: la biblioteca las atrapa o el programa termina.
- No se importan los constructores plantilla (`std::unique_ptr`, `std::pair`) ni los de `icu::UnicodeString`.
  Los retornos `const T&` de funciones y métodos ya se importan (tanda 19); faltan `T&&` y las referencias en
  operadores y en métodos estáticos.
- Las sobrecargas `int`/`long` de funciones de namespace ya se resuelven como en Clang, y los alias de
  namespace ya nombran tipos y enumeradores (tanda 19). Faltan las alternativas en los métodos de clases.
- Las macros de función no se expanden.
- Los punteros a función, records por valor y variádicas con `swiftcall`, `swiftasynccall` e
  `intel_ocl_bicc` se rechazan.
- Clang 23.1.0 no decora `preserve_all` ni `intel_ocl_bicc` en C++, y su propio control de una variádica
  C++ `preserve_none` falla (T19-032).
- Falta `new T` de un tipo C++ desde NativeSharp.
- No hay runtime dinámico `/MD` ni arranque de inicializadores sin el CRT.
- La instalación de NSC no incluye NSLink, `lld-link`, `llvm-lib` ni los encabezados de recursos de Clang.

## Criterio de terminación (T18-023)

T18-023 pide declarar terminado el objetivo solo cuando el inventario acordado y sus combinaciones estén
cubiertos con evidencia. No lo están: el inventario tiene entradas en estado P y R, y hay combinaciones sin
fixture. T18-023 queda abierto y el trabajo sigue en la sección 19 de TRACKING.
