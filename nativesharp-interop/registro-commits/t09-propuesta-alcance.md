# Tanda 9 (llamadas, enlace y datos C): propuesta de alcance

Fecha: 2026-11-09. Instrucción del usuario: revisar el tracking y presentar el alcance, las dependencias y los
resultados de la tanda 8 antes de empezar. **No se implementó nada de la tanda 9.**

## Punto de partida: lo que la tanda 8 ya ejercita

La sección 09 de `TRACKING.md` se escribió antes de la tanda 8. Muchas de sus casillas ya tienen cobertura de
extremo a extremo con pruebas de la tanda 8, aunque no se cerraron como ítems T09.

| Ítem T09 | Cobertura existente (hecho comprobado en las suites de la tanda 8) | Lo que falta |
|---|---|---|
| T09-001/002: auditar HIR, MIR y emisión de llamadas externas | ninguna auditoría escrita; el camino se usa en todas las suites C | la auditoría: dónde se decide el símbolo, la convención y la clasificación ABI, y qué parte viene de Clang y qué parte reimplementa NSC |
| T09-003: símbolo y convención de Clang | `cdecl` y `__vectorcall` (D22), dirección de función C (D18), alias TLS (D01–D05) | comprobar que el nombre y la convención salen siempre de Clang (por ejemplo `__stdcall`, `__fastcall`, `asm("…")` y `dllimport`), no solo en los casos probados |
| T09-004/005/006: escalares y punteros | `funciones-c`, `punteros-c`, `enteros-c`, `flotante-c`, `void-c`, `bool-abi-c`; O0–O3 con control Clang | cerrar la casilla con una matriz única por tipo |
| T09-007/008: agregados por valor | D09 (struct por valor), variádicas con records de 3, 8 y 24 bytes, HFA y HVA (D22); `nsc.item342.aggregate-abi-matrix` pasa (construido en el cierre técnico) | matriz sistemática contra Clang: tamaños de 1 a 32 bytes y no potencias de 2, mezclas `float`/`int`, `pack` y `alignas`, parámetro y retorno (incluido el `sret` de Win64), por puntero a función y desde plantilla precompilada |
| T09-009/010: globales | bloque T08-004 (escalares calificados, arreglos, atómicas, TLS, `const`) | cerrar la casilla citando la evidencia y añadir la escritura rechazada por `const` en la matriz |
| T09-011/012: variádicas | `validar-variadicas*`, promociones de `byte`, `short` y `float` (tanda 8) | revisar la cobertura de `long double` y de los records por valor en posiciones variádicas altas |
| T09-013/014: biblioteca estática y DLL | cerradas antes de la tanda 8 | — |
| T09-015: errores de enlace | sonda de esta revisión: falta `missing()` y NSLink informa «NSL1020: unresolved external symbol: 'missing'… referenced by: program-0.obj» | el mensaje no nombra ni el encabezado o la `CppLibraryReference` de origen ni la línea NativeSharp; hay que diagnosticarlo en NSC antes de enlazar, o enriquecer el mensaje **sin modificar NSLink** |
| T09-016: contraste con un consumidor Clang | controles Clang en la mayoría de las suites | un arnés común que compile el mismo caso como consumidor C y como consumidor NativeSharp y compare valores y memoria |

## Alcance propuesto

1. **Auditoría (T09-001/002/003).** Recorrer el camino de la llamada C desde `CallAnalyzer` hasta la emisión
   LLVM: `HirCallLowering`, MIR y `LLVM/Lowering`. Hay que documentar quién decide en cada paso el símbolo, la
   convención, la clasificación de cada parámetro (directo, indirecto, `sret`, HFA y HVA) y los atributos
   (`zeroext`, `signext`, `inreg`).

   Inferencia, no verificada: la clasificación de las llamadas externas la calcula NSC replicando las reglas de
   Win64, porque Clang solo se usa para generar el IR de las funciones C `inline` y de las de duración de vida
   (`ClangSession.cpp`, `CreateLLVMCodeGen`).
2. **Decisión de diseño que saldría de la auditoría:**
   - **A:** obtener la clasificación de Clang con `clang/CodeGen/CodeGenABITypes.h` (`arrangeFreeFunctionType`),
     sin modificar Clang;
   - **B:** conservar la clasificación propia de NSC y fijarla con la matriz diferencial.

   Se presentaría con evidencia antes de cambiar nada.
3. **Matriz ABI diferencial (T09-004 a T09-008, T09-016).** Un generador de casos (escalares, punteros y records)
   que compile cada firma como función C con Clang y la llame desde NativeSharp y desde C. Compara valores
   devueltos, memoria escrita e IR de la frontera, en O0–O3, en directo, precompilado sin encabezado y DLL.
   Parte de `item342` (matriz ABI de agregados, ya pasa) y de `item340` (punteros a función COM, pasa).
4. **Globales y variádicas (T09-009 a T09-012).** Cerrar las casillas con la evidencia de la tanda 8 y añadir solo
   los huecos de la tabla.
5. **Errores de enlace (T09-015).** Un diagnóstico de NSC que relacione un símbolo C no resuelto con su
   declaración, su `CppLibraryReference` y el uso NativeSharp. Necesita saber qué exporta la biblioteca (`.lib` o
   DLL) antes de enlazar, sin modificar NSLink.

## Dependencias y pendientes que entran desde la tanda 8

- **Pendiente funcional sin resolver (T08-020, nombres sólo por metadata).** Un consumidor sin encabezado no
  puede nombrar un `typedef` ni un `enum` C que el productor no usa en su superficie (`NAlias`, `NMood` en
  `pruebas-tanda-08-nombres-metadata`). NSMeta solo publica los tipos C que la superficie del productor
  referencia. `validar.cmd` termina con 1. Las cuatro sondas por posición (declaración, `sizeof`, cast y valor)
  ya pasan. Es una decisión de contenido de NSMeta y necesita autorización:
  - publicar el catálogo C completo del encabezado;
  - o exigir el encabezado al consumidor y mantener el rechazo.
- **D21 / T08-GATE-05:** la agrupación del historial espera la decisión del usuario
  ([propuesta](t08-d21-propuesta-agrupacion.md)).
- **Fixture NSMeta 1.46 ausente:** `nsc.stage48_2.library-metadata`, 2 de 444. Se deja intacto por decisión
  del usuario.
- **`item341` (excepción que escapa por un thunk COM):** el fixture choca con el `throw` chequeado del congelamiento
  3.0. La frontera de excepciones C es una decisión de lenguaje previa a la tanda 9; si no se toma, la tanda 9
  la deja fuera de su alcance.
- **Configuración:** ctest de `tanda1-ON` mide ahora `A:\nsc-interop-build\tanda1-ON\bin\nsc.exe`
  (`NSC_TEST_COMPILER`). La tanda 9 debe mantenerlo así.
- **Restricciones vigentes:**
  - no modificar NSLink ni LLVM/Clang (Clang solo se consulta por su API);
  - un único contrato NSMeta, sin versiones nuevas;
  - temporales en `A:`, nada en `C:`.

## Resultados de la tanda 8 (resumen)

Ver [validación del cierre técnico](t08-validacion-cierre-tecnico-20261109.md) y `TRACKING-TANDA-08-DETALLADO.md`:

- T08-002 a T08-020 y las decisiones T08-D01 a T08-D24 están resueltas en el seguimiento detallado;
- quedan abiertos el pendiente de nombres solo por metadata, D21/GATE-05 y el fixture 1.46.
