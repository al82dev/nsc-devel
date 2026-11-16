# Revisión de los cierres de las tandas 9 a 18 (2026-11-16)

## Criterio

Una casilla queda cerrada solo si su capacidad funciona y tiene evidencia frente a Clang 23.1.0. Un rechazo con
diagnóstico, una investigación o una limitación documentada no cuentan como implementación. Por eso:

- una casilla cuyo alcance conserva una capacidad pendiente se reabre (`- [ ]`) y nombra la tarea de la
  sección 19 que la continúa;
- una tanda con casillas reabiertas no está cerrada;
- una capacidad que depende de una limitación de Clang queda abierta y bloqueada, con su reproducción.

Fuentes revisadas:

- lineamientos generales y del compilador;
- `AGENTS.md`;
- TRACKING, secciones 9 a 19;
- los registros de cierre `t09-cierre-t09-003.md` y `t10-cierre.md` a `t18-cierre.md`;
- el inventario, los pendientes, el informe final y la validación agrupada de la tanda 18.

Estado del árbol al revisar: rama `development-cpp-clang`, último commit `c70b08c85`, sin cambios propios. La
única entrada sin seguimiento es `selfhost/tests/profile/positive/compiler_skeleton/obj/`, que no se toca.

Alcance de la comprobación. Para cada casilla se contrastó su texto con el cierre de su tanda y con los
pendientes que ese cierre trasladó a otra tanda. Cada traslado se siguió hasta la tanda destino. Las casillas
sin pendientes en su cierre conservan la evidencia que citan; esta revisión no repitió sus fixtures.

## T09-003: convenciones y símbolos

T09-003 se cerró el 2026-11-14 con el criterio «lo que NSC acepta usa la convención de Clang y lo demás se
rechaza». Ese criterio no cubre el alcance: varias formas solo se rechazan. Se reabre.

### Funcional, con evidencia frente a Clang

- Los 15 atributos de convención de Clang 23.1 en Windows x64: NSC nunca emite una convención distinta de la de
  Clang ([matriz](registro-commits/t09-matriz-convenciones-win64.md)).
- `cdecl`, `stdcall`, `fastcall`, `thiscall`, `pascal` y `ms_abi`: los cinco primeros producen la convención
  predeterminada en este destino; los seis se consumen en C y C++.
- `vectorcall`, `regcall`, `sysv_abi`, `preserve_most`, `preserve_all` (C) y `preserve_none`: matriz de 7
  convenciones por 18 clases de firma C, directa y por puntero, con 252 celdas correctas
  ([matriz de firmas](registro-commits/t09-matriz-firmas-c.md)).
- Enums C y C++, uniones C (salvo la de solo `float` con `regcall`), agregados homogéneos con `regcall`, records
  C++ triviales y el replay precompilado de records C con calificadores, empaquetado, `char` y alineación:
  estática y DLL, O0–O3, directa y precompilada.
- `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` en C, y las dos Swift en C++, con firmas escalares y de
  punteros, en llamada directa.
- Tamaños de objeto ocultos y flujos de receptor C++ en las formas registradas (`t09-tamano-*.md`,
  `t09-receptor-*.md`, `t09-fabrica*.md`).

### Solo rechazado (dentro del alcance, pendiente de implementar)

| Forma | Diagnóstico actual | Tarea |
|---|---|---|
| Punteros a función `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` | `unsupported C callback calling convention` | T19-028 |
| Records por valor y variádicas con `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` | rechazo en el plan ABI | T19-028 |
| Unión C de solo `float` por valor con `regcall` | rechazo en el plan ABI | T19-029 |
| `__unaligned`, atributos distintos de la alineación, `#pragma pack` y alineación con bit-fields, en el replay sin encabezado | rechazo del replay | T19-030 |
| Tamaños de objeto ocultos (`pass_object_size`) en formas no registradas | rechazo | T19-031 |
| Flujos de receptor C++ en plantillas precompiladas no registrados | rechazo | T19-031 |

### Bloqueado por Clang 23.1.0

| Forma | Reproducción | Alcance exacto | Tarea |
|---|---|---|---|
| `preserve_all` e `intel_ocl_bicc` en C++ | Clang: `cannot mangle this calling convention yet` ([matriz](registro-commits/t09-matriz-convenciones-win64.md)) | solo C++; en C funcionan | T19-032 |
| Variádicas C++ `preserve_none` | el ejecutable de Clang falla con `0xC0000005`; `pruebas-tanda-09-cpp-variadica-preserve-none` ([observación](registro-commits/t09-preserve-none-variadica-observacion.md)) | funciones variádicas `preserve_none` de C++ con `va_start`; la `cdecl` equivalente pasa | T19-032 |

Ninguna se presenta como implementada. Para resolverlas hace falta otra versión de Clang o modificar Clang, y
ambas cosas necesitan autorización.

### Por verificar

- `vectorcall` con agregados vectoriales homogéneos por valor: la matriz de firmas no lo nombra. Tarea T19-036.

## Tandas 10 a 18: casillas reabiertas

| Casilla | Lo que sigue sin funcionar | Tarea |
|---|---|---|
| T10-002 | alias y `using` de variables y tipos; `using` global; anidados privados o protegidos; enumerador no scoped nombrado por el record | T19-005 |
| T10-003 | parámetros `T&&` distintos del movimiento | T19-007 |
| T10-004 | global de tipo enum sin que el programa nombre el enum; `thread_local` sin `constinit`, sin evidencia en la tanda 16 para campos estáticos | T19-005, T19-036 |
| T10-005, T10-013 | `T&` y `const T&` en métodos estáticos | T19-007 |
| T10-006 | campos estáticos `thread_local` | T19-036 |
| T10-007 | anidados privados o protegidos; acceso protegido desde una clase NativeSharp derivada | T19-005, T19-035 |
| T10-010 | predeterminados de enum, de record o no constantes | T19-006 |
| T10-012 | funciones `inline` declaradas `dllimport` | T19-008 |
| T10-016 | operandos `T&`; retornos por referencia en operadores; temporal con `const T&` | T19-007 |
| T11-002 | records `class` en el catálogo precompilado: trasladado a T11-002 sin evidencia propia | T19-036 |
| T11-004 | campos de una unión C++ con miembros record | T19-004 |
| T11-005, T11-006 | `T&&` en métodos y constructores; métodos virtuales o heredados no públicos | T19-007, T19-012 |
| T11-007 | destructores que pueden lanzar en locales | T19-009 |
| T11-015, T11-016 | copia implícita no trivial; movimiento con constructor de copia | T19-009 |
| T11-018 | `new T` de un tipo C++ desde NativeSharp | T19-011 |
| T12-001, T12-003, T12-009 | campos, conversión y construcción con base virtual; referencia a la base | T19-014 |
| T12-004, T12-006 | operadores miembro no const, heredados, de asignación, llamada, subíndice y flecha; métodos heredados y virtuales en plantilla precompilada | T19-013, T19-016 |
| T12-010, T12-011 | `typeid` y `dynamic_cast` escritos en NativeSharp | T19-014 |
| T13-001, T13-020 | especialización en la API pública de un módulo precompilado; objetos de vida no trivial y métodos heredados en plantilla precompilada | T19-016 |
| T13-004, T13-005, T13-007 | plantillas miembro; ordenación parcial; deducción con struct NativeSharp; structs NativeSharp con campo C++, arreglo fijo o enum; structs genéricos | T19-015 |
| T13-006 | uso de plantilla con un nombre de `using` | T19-005 |
| T13-011 | excepciones a través de una especialización | T19-017, T19-036 |
| T13-021, T13-022 | plantillas de variable; constantes record, arreglo o puntero | T19-015 |
| T14-002, T14-003 | macros de función; macros de cadena, puntero, record o tipo | T19-003 |
| T14-006 | bit-fields de tipo enum o de más de 64 bits | T19-004 |
| T14-007 | portador de punteros a miembro | T19-014 |
| T14-009, T14-010 | bibliotecas solo con `.pch`; interfaces `.pcm` | T19-021 |
| T15-002 | runtime dinámico `/MD` | T19-019 |
| T15-003, T15-013 | atrapar o convertir una excepción C++; excepción NativeSharp en un callback | T19-017 |
| T15-008 | lambda escrita directamente como argumento: se trasladó a T17-003, que no la cubrió | T19-033 |
| T15-010 | callbacks con estado: la lambda con capturas solo se rechaza | T19-018 |
| T15-014 | constructor plantilla (`std::unique_ptr`) | T19-010 |
| T15-015 | `std::allocator<int>::deallocate` desde NativeSharp | T19-018 |
| T15-016 | movibles no copiables devueltos por valor; `move` de un objeto C++ | T19-009 |
| T15-017 | corutina devuelta por valor | T19-018 |
| T16-002, T16-009 | runtime dinámico `/MD` | T19-019 |
| T16-003, T16-008 | `.CRT$XCU` sin el CRT; llamada a NativeSharp desde un constructor global | T19-019 |
| T16-006 | diagnóstico de TLS sin el CRT: se trasladó a T17-006, que trata otra cosa; directorio TLS con NSLink | T19-034 |
| T16-007 | hilo creado por NativeSharp con TLS C++ | T19-020 |
| T16-012, T16-016 | `LoadLibrary`/`FreeLibrary` de una DLL C++; LTO y bitcode con NSLink | T19-020 |
| T17-001, T17-005, T17-007, T17-008 | ubicación con línea y subrayado; miembros que faltan al enlazar; depuración interactiva; `-fdebug-prefix-map` | T19-023 |
| T17-003 | alternativas en métodos de clases C++ | T19-006 |
| T17-010, T17-011 | inventario de `names` y `members`; caché en `nsc build` | T19-024 |
| T18-006 | combinaciones sin fixture | T19-025 |
| T18-009, T18-011, T18-013 | `gz*` de zlib; el resto de ICU; especializaciones de la STL que siguen rechazadas | T19-025, T19-009, T19-010, T19-015 |
| T18-014 | 189 pruebas de NSC fallan o no se ejecutan; 36 sin línea base | T19-027, T19-037 |

## Contradicciones corregidas

- **T18-007 y T18-021** decían que todos los pendientes de las tandas 8 a 18 tenían tarea. Faltaban:
  - los límites de T09-003;
  - la lambda directa como argumento (T15-008);
  - el diagnóstico de TLS sin el CRT (T16-006);
  - la clase NativeSharp derivada (T10-007, T12).

  Esta revisión les da tarea (T19-028 a T19-036). Las dos casillas quedan cerradas con esa corrección anotada.
- **Heredar un tipo C++ desde NativeSharp.** El cierre de la tanda 12 lo declaraba fuera del alcance, pero
  ningún ítem lo excluía. Es la forma de implementar una interfaz C++ que llama la biblioteca, un callback en
  el sentido de la tanda 15, y no el camino inverso. Pasa a T19-035.
- **`/MD`.** El inventario lo ponía fuera de la línea base y los pendientes lo tenían en T19-019. Queda
  pendiente en T19-019.
- **T18-014 decía «no atribuible» para 153 fallas.** Que no sean atribuibles a la interoperabilidad no les da
  una línea base que pase:
  - 83 no se ejecutan porque `ninja -k 0` no enlaza por tres símbolos sin definir;
  - 27 necesitan `NativeSharp.Core.IO`;
  - 25 son del selfhost;
  - 12 fallan por la salida concisa;
  - 4 fallan igual con el compilador anterior;
  - 2 dependen del entorno.

  Pasan a T19-037.
- **T19-026** proponía «reclasificar». Los dos validadores `preserve-none` reproducen el bloqueo de T19-032.
  `cpp-tamano-objeto/validar-indice-expresion` falla con cinco procesos y debe corregirse.
- **El informe final** decía que no se importaban los retornos `const T&`, que `int`/`long` era ambiguo y que
  los alias de namespace no servían como prefijo. La tanda 19 lo cambió; el informe se actualiza.

## Tareas nuevas

- T19-028. Convenciones `swiftcall`, `swiftasynccall` e `intel_ocl_bicc`: tipos de función NativeSharp para
  sus punteros, records por valor y variádicas.
- T19-029. Unión C de solo `float` por valor con `regcall`.
- T19-030. Replay precompilado sin encabezado de `__unaligned`, atributos distintos de la alineación,
  `#pragma pack` y alineación con bit-fields.
- T19-031. Tamaños de objeto ocultos en todas las formas; flujos de receptor C++ en plantillas precompiladas.
- T19-032. Bloqueos de Clang 23.1.0: `preserve_all` e `intel_ocl_bicc` en C++, variádicas C++
  `preserve_none`. Conservar la reproducción; resolver solo con otra versión de Clang o con autorización.
- T19-033. Lambda escrita directamente como argumento de tipo puntero a función.
- T19-034. Diagnóstico propio de TLS sin el CRT; directorio TLS con NSLink (requiere autorización para NSLink).
- T19-035. Clases NativeSharp derivadas de clases C++: overrides llamados por la biblioteca y acceso protegido.
- T19-036. Fixtures de los traslados sin evidencia:
  - `class` en el catálogo precompilado;
  - globales y campos estáticos C++ en una plantilla precompilada;
  - campos estáticos `thread_local`;
  - excepciones a través de un adaptador y de una especialización;
  - records anidados en plantillas;
  - `vectorcall` con agregados vectoriales.
- T19-037. Línea base de las 189 pruebas de NSC que fallan o no se ejecutan.

## Orden de trabajo

1. T19-037 y T19-027: línea base de regresión.
2. T19-036: traslados sin evidencia.
3. T19-005, T19-006, T19-007, T19-008: nombres, sobrecargas y referencias.
4. T19-002, T19-001, T19-003, T19-004: C.
5. T19-029, T19-028, T19-030, T19-031: convenciones y replay.
6. T19-009, T19-010, T19-011, T19-012, T19-013, T19-014, T19-035: objetos y clases.
7. T19-015, T19-016: plantillas.
8. T19-033, T19-017, T19-018: callbacks, excepciones e invocables.
9. T19-019, T19-020, T19-034, T19-021, T19-022: arranque, enlace y distribución.
10. T19-023, T19-024: diagnóstico y caché.
11. T19-025, T19-026: combinaciones y pruebas integrales.
12. T19-032 sigue abierto mientras dependa de Clang.

El objetivo de las 18 tandas (T18-023) no está terminado: quedan las casillas reabiertas de arriba.
