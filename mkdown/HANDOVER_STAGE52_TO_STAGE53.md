# Handover — Stage 52 → Stage 53 (cierre formal definitivo, 2026-08-28)

Reemplaza la versión anterior de este documento, escrita antes de que Wine 9.0 estuviera
disponible y antes de las rondas de foreach (52.x, 52.x.2) y del cierre del bug de RAII/Wine.
Esta es la versión que efectivamente cierra Stage 52.

## Estado al cierre

```text
nsc 0.1.0-dev52 (NativeSharp 0.1, stage 52)
Full CTest: 210/210 PASS (100%)
```

Sin fallas conocidas. Verificado por ejecución real bajo Wine 9.0 (no solo estructuralmente):

```text
Stage52Collections.exe: 149, sin crash, determinista (3/3 corridas)
Stage52xForeach.exe:    91, sin crash, determinista
```

## Qué cambió respecto de Stage 51 (acumulado, todas las rondas)

1. **Cinco fixes reales de compilador en la ronda original** (8, 9, 10a, 10b, más el fix de
   resolución de generics cross-project — los fixes 1–7 ya estaban cerrados antes): `move()`
   como target de asignación a elemento de arreglo; `default(T)` implementado end-to-end como
   feature real de lenguaje (lexer→parser→AST→semántica→HIR→MIR→LLVM); resolución de overload
   para un parámetro `owned`/`borrowed` de tipo valor movido como argumento de llamada; lowering
   LLVM de `MoveValue` corregido para no asumir siempre puntero.
2. **Un sexto fix real, encontrado DESPUÉS del cierre original por ejecución real bajo Wine**:
   doble-free por asignación plana (en vez de `move()`) al guardar un parámetro `owned` en el
   storage de un contenedor — 14 sitios corregidos en toda la librería (`List`, `Queue`, `Stack`,
   `Dictionary`, `SortedList`, `PriorityQueue`, `LinkedListNode`). Este bug era invisible a toda
   la verificación estructural (compila/verifica LLVM/emite COFF) — solo apareció ejecutando el
   binario de verdad. Ver `52_nativesharp_generic_collections.md` sección 10.2.
3. **`foreach` real sobre los diez contenedores** (rondas 52.x y 52.x.2, posteriores al cierre
   original), con un séptimo fix real encontrado en el proceso: `analyzeForeach` no cerraba la
   cleanup region de una fuente `raii`. Ver sección 10.3 del mismo documento.
4. **`NativeSharp.Core.Exception` real**, empaquetado por primera vez.
5. **`NativeSharp.Core.Collections.Generic` completo**: `List<T>`, `Queue<T>`, `Stack<T>`,
   `LinkedList<T>`/`LinkedListNode<T>`, `Dictionary<TKey,TValue>`, `HashSet<T>`,
   `SortedList<TKey,TValue>`, `SortedDictionary<TKey,TValue>`, `SortedSet<T>`,
   `PriorityQueue<TElement,TPriority>`, `KeyValuePair<TKey,TValue>`, `Comparer<T>`/
   `EqualityComparer<T>`, delegates de soporte — con `foreach` real sobre las diez.
6. **Limitación real, deliberadamente NO arreglada**: Stage 40's struct-value ABI de Windows x64
   solo soporta agregados de 1/2/4/8 bytes (local, argumento, o retorno — sí funciona como
   elemento de arreglo). Rodeado con arreglo de salida (`TryGetValue`) y delegate de dos
   parámetros (`ForEach`) en vez de tocar el ABI.
7. **`var`** — inferencia de tipo local, bloque separado pedido por Alberto.
8. **23 ejemplos académicos** (paquete separado, basado en K&R + otro material), que a su vez
   destaparon y cerraron dos hallazgos más: la convención real de `Console.WriteLine` (no es un
   stub), y el bug de RAII/Wine (ver punto siguiente).
9. **Bug de RAII + try/catch, investigado y CERRADO como bug de Wine (no de `nsc`)** — reusar un
   objeto `raii` después de un `catch` real crasheaba bajo Wine 9.0. Confirmado contra Windows
   real el 2026-08-28: el binario de `nsc` corre perfecto ahí. Ver
   `claude/13_bug_raii_trycatch_confirmado_bug_de_wine.md` en el proyecto para el análisis
   completo (incluye un repro mínimo en C puro, sin NativeSharp, que reproduce la misma anomalía
   con clang real). Dos mejoras de codegen genuinas y no relacionadas quedaron aplicadas de
   paso: deduplicación de cleanup regions compartidas, y atributo LLVM `uwtable` en las funciones
   generadas (antes ausente por completo).

## Qué NO se tocó / queda explícitamente fuera de Stage 52

- **Constructores nunca reciben `owned T`** — decisión deliberada, no un bug.
- **`ref`/`out` reales** — limitación de lenguaje general y preexistente.
- **Struct-value ABI > 8 bytes** — ver punto 6 arriba. Arreglarlo de raíz significa implementar
  paso/retorno indirecto de structs (convención `sret`), un trabajo de ABI completo aparte.
- **`SortedDictionary<TKey,TValue>`/`SortedSet<T>` son arreglo ordenado, no árbol balanceado** —
  decisión deliberada y documentada. O(n) inserción/borrado en vez de O(log n).
- **Interfaz `IEnumerable<T>` formal** — `foreach` funciona vía desugaring estructural
  (`GetEnumerator`/`MoveNext`/`Current`), sin necesitar la interfaz explícita.

## Ejecución real — dónde quedó parada, y qué falta

- **Wine 9.0**: toda la superficie de Stage 52 (colecciones + foreach) verificada por ejecución
  real, determinista, sin crashes conocidos.
- **Windows real**: ya se demostró que es viable prepararle a Alberto `.exe` para correr
  directamente sin compilar nada (usado con éxito para el bug de RAII de arriba). **Falta
  todavía** correr específicamente `Stage52Collections.exe`/`Stage52xForeach.exe` (o repros
  puntuales del bug de doble-free ya corregido) en Windows real — se entrega, junto con este
  cierre, un paquete de verificación aparte (`.exe` + instrucciones) para que Alberto lo corra
  cuando quiera. No es bloqueante para declarar Stage 52 cerrado (Wine 9.0 ya da una señal fuerte
  y determinista, y el precedente del bug de RAII ya mostró que Wine puede divergir de Windows
  real en casos de SEH específicos — vale la pena la confirmación, pero no es un blocker de
  diseño ni de arquitectura como sí lo hubiera sido el bug de doble-free si no se hubiera
  encontrado).

## Recomendación para Stage 53

Roadmap dice **Time / Filesystem / Environment**. Puntos a tener en cuenta desde este cierre:

1. Cualquier API nueva que necesite devolver "encontrado + valor" (p. ej. un `TryParse`-style)
   debe usar el mismo idioma de arreglo-de-un-elemento que `TryGetValue` estableció aquí — no
   hay `out` real, y un struct `{bool, T}` topa con el límite de ABI en cuanto T supera 4-8
   bytes.
2. Si Stage 53 necesita pasar/devolver structs más grandes (p. ej. un `DateTime`/`TimeSpan` de
   más de 8 bytes, o un `FileStat` con varios campos) por valor de verdad, vale la pena evaluar
   ahí mismo si conviene por fin implementar paso/retorno indirecto de structs (`sret`) en el ABI
   de Stage 40.
3. **Cualquier feature con cleanup RAII + control de flujo no trivial (loops, foreach, excepciones)
   debe probarse con EJECUCIÓN REAL bajo Wine desde el primer momento**, no solo verificación
   estructural — dos de los tres bugs reales de esta etapa (doble-free `owned`/`move()`, cleanup
   region no cerrada en `foreach`) eran invisibles a `--verify-project-oop-llvm`/
   `--emit-project-oop-obj` y solo aparecieron corriendo el binario.
4. **Si aparece un crash bajo Wine con forma de SEH/try-catch/raii, confirmar contra Windows real
   antes de asumir que es un bug de `nsc`** — Wine 9.0 tiene al menos un bug real y reproducible
   (fuera de nuestro control) en su emulación de SEH cuando hay un `__except` anidado dentro de
   un `__finally`/cleanup que atrapa de verdad (ver el bug de RAII arriba).

## Cómo verificar este paquete de forma independiente

```bash
# 1. Verificar el manifest de integridad del arbol FULL
cd NativeSharp_Stage52_Current_Code
python3 verify_manifest.py
# esperado: "MANIFEST: N/N PASS"

# 2. Build limpio + suite completa
cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
# esperado: 210/210 PASS, sin fallas conocidas

# 3. Identidad del compilador
./build/nsc --version
# esperado: nsc 0.1.0-dev52 (NativeSharp 0.1, stage 52)

# 4. Ejemplo de Stage 52 hasta ejecucion real bajo Wine
wine build/bin_windows/Stage52Collections.exe ; echo $?
# esperado: 149
wine build/bin_windows/Stage52xForeach.exe ; echo $?
# esperado: 91

# 5. Verificacion en Windows real (paquete de .exe entregado aparte con este cierre)
# correr los .exe directamente en Windows, sin compilar nada -- ver instrucciones incluidas
```
