# T08-D07: `_Atomic` sobre un record C, sin bloqueo y con la biblioteca atómica

Fecha: 2026-11-09. Casillas: T08-D07 y T08-004-04d1 ([límite anterior](t08-globales-atomicas.md#límite-registrado-t08-004-04d1)).

## Problema

C11 admite `_Atomic` sobre un struct o una union. NativeSharp no importaba esas globales: daba «C/C++ global type is
not yet mapped». La decisión del usuario pide los dos casos: los tamaños que la plataforma resuelve directamente y
los que requieren la biblioteca atómica.

## Política de Clang (x86_64-pc-windows-msvc)

Observada con la sonda `A:\nsc-interop-build\claude\sondas\atomic1\probe.c`:

- Un record atómico de 16 bytes o menos se amplía a la siguiente potencia de dos y se alinea a ese ancho. Uno
  mayor conserva su tamaño y su alineación. Ejemplos: 3 → 4, 12 → 16, 24 → 24.
- Anchos 1, 2, 4 y 8: `load atomic iN seq_cst` y `store atomic iN seq_cst`. Con `volatile`, también son `volatile`.
- Cualquier otro ancho, incluido 16: llamadas genéricas `__atomic_load(size, obj, buf, 5)` y
  `__atomic_store(size, obj, buf, 5)`, sobre un búfer del ancho atómico. Al escribir, el relleno va a cero.
- `clang_rt.builtins` no contiene `__atomic_*`. La biblioteca atómica la aporta quien enlaza, igual que para el
  código C.

Para que C y NativeSharp coincidan sobre el mismo objeto, NativeSharp tiene que usar el mismo protocolo. En los
anchos no libres de bloqueo, eso significa llamar a la misma implementación de la biblioteca.

## Cambio

- `CppSemanticInventory.cpp`: nuevo caso `atomicRecord`. Cubre `_Atomic` sobre un struct o union C, también
  `_Thread_local` y `volatile`, y lo admite cuando:
  - el ancho atómico es múltiplo de 8 bits y no es menor que el record;
  - si el ancho es de 16 bytes o menos, la alineación es al menos ese ancho.

  Se proyecta con el tipo del record y su `typeKey`, y la comprobación de consistencia exige un record (`type:` sin
  `*`).
- `ExpressionResult.h` e `IdentifierAnalyzer.cpp`: `cppAtomicRecordStorage` marca la expresión que denota la global.
- `MemberAccessAnalyzer.cpp`: como en C, un miembro de un record atómico no se accede en su sitio. Leer o escribir
  `g.a`, o tomar `&g.a`, da «cannot access member 'a' of C atomic record storage; copy the whole value into a local
  first». `&g` conserva el rechazo de las atómicas: «cannot take the address of C atomic storage…».
- `HirNameExpressionLowering.cpp`: el nodo lleva el ancho atómico (`cppAtomicAlignmentBytes`), tomado del tamaño de
  almacenamiento.
- `OopLlvmArraysAndStores.cpp`: nueva función `lowerCAtomicRecord`, que replica la política de Clang:
  - búfer local del ancho atómico, alineado a 16;
  - anchos 1, 2, 4 y 8: `load/store atomic iN seq_cst` con alineación igual al ancho (`volatile` si el acceso lo es);
  - los demás: `__atomic_load`/`__atomic_store` (`nounwind`) con el tamaño, la dirección, el búfer y el orden 5;
  - al escribir, el búfer se pone a cero antes de copiar el valor.

  La usan la escritura de la global (`OopLlvmArraysAndStores.cpp`) y su lectura (`OopLlvmExpressionLeaves.cpp`).

El mismo camino cubre varias formas de la global, porque la dirección la sigue calculando `staticFieldAddress`:

- la externa;
- la `static` del encabezado, que es una copia propia del proyecto;
- la `_Thread_local`;
- la `volatile`;
- la importada de una DLL.

## Pruebas

**Nueva:** `pruebas-tanda-08-globales-c/validar-atomicas-record.py`. Aprobada en O0–O3, con rechazos en O0.

La biblioteca atómica de la prueba está en C (`atomiclib.c`, dentro de `capi.lib`), basada en un bloqueo global, y
cuenta sus llamadas.

- **Suite «basica»:** records externos de 3, 8, 12, 16 y 24 bytes, y una union de 4 bytes.
  - NativeSharp lee cada record entero, modifica una copia local y la escribe;
  - C lo comprueba con lecturas atómicas, escribe valores nuevos, y NativeSharp los vuelve a leer;
  - NativeSharp comprueba con el contador que solo los records de 12, 16 y 24 bytes llaman a la biblioteca: tres
    lecturas y tres escrituras.
- **Suite «variantes»:**
  - `_Thread_local _Atomic` de 8 y 12 bytes;
  - `volatile _Atomic` de 8 y 16 bytes;
  - `static _Atomic` de 8 y 12 bytes definidas en el encabezado, compartidas con accesores C `static inline`
    (`s_sum`, `s_reset`).
- **Suite «dll»:** records de 8 y 12 bytes, exportados como datos por una DLL C. La misma DLL exporta la biblioteca
  atómica (`__atomic_load`, `__atomic_store` y el contador).
- **IR:**
  - `load/store atomic i64` para 8 bytes, `i32` para el record de 3 bytes, y `volatile` en la global `volatile`;
  - `__atomic_load`/`__atomic_store` con tamaño 16 (para 12 y 16 bytes) y 24;
  - la global TLS es `thread_local` y las de la DLL son `dllimport`;
  - ningún acceso no atómico a las globales.
- Cada suite tiene un control Clang y se ejecuta de forma directa. «basica» y «variantes» se ejecutan además desde una
  plantilla precompilada consumida sin el encabezado.
- **Rechazos** en las tres suites:
  - leer o escribir un miembro, también de la union, de la TLS, de la `volatile`, de la `static` y de la DLL;
  - `&` del record, de la TLS, de la `volatile` y de la DLL;
  - `&` de un miembro.

**Actualizada:** `validar-atomicas.py`. El caso `record-atomico` (`return a_pair.a;`) fijaba «C/C++ global type is not
yet mapped»; ahora espera «cannot access member 'a' of C atomic record storage». Lo autoriza el punto 6 del usuario.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d07.cmd`, resultado 0:

- la suite nueva;
- `validar-atomicas`, `validar-tls`, `validar-enum-record`, `validar-arreglos` y `validar-direccion-const`;
- `record-c/validar.cmd`;
- `pruebas-proyecto/ejecutar-cpp.cmd`;
- `nsc_cpp_session_tests`;
- el conjunto de D02.
