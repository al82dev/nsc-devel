# Consolidación — las 97 tareas SHR contra lo medido

**Fecha:** 2026-09-01 · **Entrada:** `NativeSharp_1.0_Faltantes_Concretos_Self_Hosting_Runtime_Stdlib_Language_2026-08-31.md`
**Contra:** el árbol tal como está tras la corrida verde de hoy (242/242 ctest, 71/71 checklist,
1201/1201 manifiesto), `78_huecos_del_lenguaje_completo.md`, `79_ids_79_80_81_83.md` y el CC2.
**Qué es:** una reconciliación. No implementa nada.

---

## 1. La corrección más importante, y va contra mí

**Ese documento introduce un objetivo que yo no tenía: el auto-hospedado.** Todo lo que he
priorizado en las últimas rondas — incluido el CC2 que escribí esta mañana — está ordenado por *qué
falla en silencio*. Ese criterio es correcto para un compilador que se usa; **no** es el criterio
para un compilador que va a compilarse a sí mismo.

Concretamente: recomendé **`volatile` primero** porque es lo único donde un programa correcto se
comporta mal sin síntoma. El documento SHR lo pone en **P2** y dice, con razón, *«no bloquea
self-host single-thread»* y *«no implementar keyword sin contrato»*. **Bajo el objetivo de
auto-hospedado, esa prioridad es la buena y la mía no.** `nsc-next` es un compilador de un hilo;
`volatile` no le hace falta para existir.

Retiro esa recomendación. Lo que sigue está ordenado por **qué impide que `nsc-next` arranque**.

---

## 2. La medición que fija el camino crítico

Nadie tiene que adivinar qué necesita `nsc-next`: **el compilador actual ya lo dice**. Conté qué
cabeceras de la biblioteca estándar de C++ usa este árbol, por frecuencia:

| Cabecera C++ | Usos | ¿Existe en NativeSharp? | SHR |
|---|---:|---|---|
| `<string>` | 43 | **sí** — `String.ns` | — |
| `<vector>` | 38 | **sí** — `List<T>` | — |
| `<string_view>` | **28** | **NO** | **SHR-010** |
| `<optional>` | **26** | **NO** | **SHR-017** |
| `<span>` | **25** | **NO** | **SHR-011/012** |
| `<utility>` | 18 | parcial — `move` sí, `pair` no | — |
| `<filesystem>` | **15** | **NO** (sólo texto y rutas básicas) | **SHR-031** |
| `<algorithm>` | **11** | **NO** | **SHR-052** |
| `<memory>` | 10 | **sí** — `owned` / `raii` cubren `unique_ptr` | — |
| `<unordered_map>` + `<unordered_set>` | 15 | **sí** — `Dictionary` / `HashSet` | — |
| `<array>` | 6 | **sí** — arreglos fijos `T[n]` | — |
| `<sstream>` | 5 | **NO** | **SHR-019** |
| `<functional>` | 3 | **sí** — delegados | — |
| `<variant>` | 1 | no, y con un solo uso no importa | — |

**Esto reordena la tabla SHR mejor que cualquier juicio.** Las cinco ausencias que de verdad pesan
son `string_view`, `optional`, `span`, `filesystem` y `algorithm` — en ese orden, medido por uso
real, no por intuición.

**Y una observación honesta en la otra dirección:** `Result<T,E>` (SHR-018, marcado P1/P2 «muy
útil») **no tiene ningún equivalente en uso en este compilador**. Es una aspiración de diseño, no
una necesidad medida. Puede seguir siendo buena idea; no es camino crítico.

**Un bloqueo duro que la tabla marca bien y conviene subrayar:** `src/Linker/LldLinker.cpp` invoca
`lld-link.exe` con `CreateProcessW` y espera con `WaitForSingleObject`. **Sin `Process.Run`
(SHR-032), `nsc-next` no puede enlazar.** Es P1 y es real.

---

## 3. Lo que ya está cerrado — 9 de las 97

Cinco de ellas se cerraron hoy y están verdes en tu máquina.

| SHR | Qué pedía | Estado | Evidencia |
|---|---|---|---|
| **SHR-001** | layout canónico de `enum` | ✅ **cerrado hoy** (ID 78) | `valueLayoutForTypeKey` decía 8 y `layoutNamedType` 4; ahora una sola regla. Fixture `c16_enum_layout` enfrenta `sizeof`, offset de campo, paso de puntero y paso de arreglo **entre sí** |
| **SHR-006** | prefijo `++`/`--` | ✅ **cerrado hoy** (ID 79) | `c13_increment`, once tipos numéricos + punteros, `-O0` y `-O2` |
| **SHR-007** | postfijo `++`/`--` | ✅ **cerrado hoy** (ID 79) | mismo nodo, un bit de diferencia; la fixture comprueba que valen cosas distintas |
| **SHR-008** | asignaciones compuestas | ✅ **cerrado hoy** (ID 81), *con una excepción* | `c14`, los diez operadores. Su criterio «lvalue una sola vez» se mide **dos veces**: contador en ejecución y conteo de llamadas en el IR. **Falta lo que su criterio también pide: propiedades e indexadores** → ID 82, abierto |
| **SHR-009** | operador `->` | ✅ **cerrado hoy** (IDs 80/83) | `c15_arrow_member`: campos, método de instancia y propiedad; `p->x` comparado contra `(*p).x` |
| **SHR-002** | promociones en el evaluador de constantes | ⚠️ **mitad cerrada** | Los **shifts ya estaban bien** desde CC1-103: el shift toma el tipo del operando izquierdo y valida el contador contra su ancho. La parte de `byte/char/short/ushort → int` son mis IDs **120 (cerrado hoy)**, **121** y **122** (abiertos) |
| **SHR-003** | limpieza al salir de un `catch` | ⚠️ **mitad cerrada** | EH-001 cerrado en CC1 y medido **en el IR** (contando bloques `eh.catch.leave.N`), porque el TLS no es observable desde el lenguaje. Falta la fixture profunda de su criterio (`catch → switch → continue`) → ID **124** |
| **SHR-046** | `PriorityQueue<T>` | ✅ **ya existía** | `lib/NativeSharp.Core.Collections.Generic/PriorityQueue.ns`, con su enumerador |
| **SHR-053** | infraestructura de hashing | ✅ **en su mayor parte** | `Hashing.ns`: `Int32/UInt32/Int64/UInt64/Boolean/Char/Byte/Short/Str` y `HashCombine`. Falta `Hash.Bytes` y el gancho para tipos propios |

---

## 4. Premisas que conviene corregir antes de planificar sobre ellas

Esto no es quisquillosería: en CC1, **seis de ochenta tareas resultaron no implementables como
estaban escritas**, y tres de ellas descansaban en la misma premisa falsa. Comprobar antes sale
barato.

1. **SHR-002 pide arreglar los shifts.** Ya están arreglados, con su razón escrita en el código
   (CC1-103 / C-004). Planificar esa parte otra vez es trabajo que ya está hecho.
2. **SHR-046 pide `PriorityQueue<T>`.** Existe.
3. **SHR-053 pide hashing.** Existe casi entero.
4. **SHR-005 («owned destination writes») está más avanzado de lo que sugiere.** El Bloque 1 ya
   cerró la liberación del valor desplazado al sobrescribir un campo `owned` — y lo hizo tras medir
   que `h.slot = null`, `h.slot = new Res(2)` y `h.slot = llamadaQueDevuelveOwned()` **filtraban**.
   Lo que queda de SHR-005 hay que **medirlo antes de escribirlo como tarea**, no asumirlo entero.
5. **SHR-075 dice «no implementar keyword sin contrato».** Estoy de acuerdo, y contradice el diseño
   que propuse esta mañana en el CC2 §6, que empezaba por la palabra. El orden correcto es
   SHR-073/074 (modelo de memoria y semántica de atómicos) **primero**, `volatile` después — y
   entonces `AtomicInt32.Value`, que hoy documenta su propio rodeo en `Atomics.ns:89`, se reescribe
   con el contrato ya escrito.

---

## 5. El error de orden que sí veo en la tabla

**SHR-093 «Bootstrap Profile 1.0» está en el paso 8 de 10. Tiene que ser el paso 1.**

Es P0/P1 y su propósito es exactamente evitar una dependencia circular. Pero decidirlo al final
invierte su función: **hasta que no esté congelado el subconjunto en el que se escribirá
`nsc-next`, no se puede saber cuáles de los ~40 items de stdlib hacen falta de verdad.** Con el
perfil congelado primero, la tabla se poda sola — la medición de §2 ya sugiere que cinco items
valen más que los otros treinta y cinco juntos.

Hacerlo al revés significa construir biblioteca a ciegas y descubrir los bloqueos tarde. Es la
misma forma del defecto que CC1 documentó: **planificar sobre una descripción en vez de sobre una
medición.**

Corolario que hace el perfil más barato de lo que parece: el lenguaje **ya** tiene lo que hace
falta para escribir `Optional<T>`, `Span<T>` y `StringView` — genéricos por monomorfización,
structs por valor, punteros, indexadores. **No hacen falta restricciones genéricas para eso**
(SHR-068/069 siguen siendo P2 de verdad). Donde sí se notan es en `algorithm` con comparadores —
y ahí hay salida por delegados, que sí existen.

---

## 6. Tabla de correspondencia SHR ↔ ID de este árbol

Sólo los que tienen correspondencia. El resto de las 97 no tiene ID propio todavía porque no había
por dónde medirlo.

| SHR | ID aquí | Estado |
|---|---|---|
| SHR-001 | 78 | ✅ cerrado |
| SHR-002 | 120 ✅ · 121 · 122 | mitad |
| SHR-003 | EH-001 ✅ · 124 | mitad |
| SHR-004 | 123 | abierto |
| SHR-006 / 007 | 79 | ✅ cerrado |
| SHR-008 | 81 ✅ · 82 | mitad |
| SHR-009 | 80 · 83 | ✅ cerrado |
| SHR-062 | 121 · 122 · deuda de `sizeof` constante | abierto |
| SHR-064 | 94 | abierto |
| SHR-065 | 89 | abierto |
| SHR-068 | 90 | abierto |
| SHR-069 | 91 | abierto |
| SHR-070 | 84 | abierto |
| SHR-071 | 85 | abierto |
| SHR-072 | 86 | abierto |
| SHR-075 | 87 | abierto — **y con el orden corregido: después de 073/074** |
| SHR-079 | 125 | abierto (decisión, no código) |
| SHR-088…092 | A-001 · A-002 · P-001 | diferidos desde CC1, con razón escrita |
| — | 77 (conteo de comprobaciones en los logs) | abierto, y protege a todos los demás |

---

## 7. El camino crítico, reordenado por lo medido

No es el orden del documento SHR ni el del CC2. Es el que sale de §2 y §5.

**Paso 0 — congelar el Bootstrap Profile (SHR-093).**
Antes que cualquier código. Con el inventario completo del lenguaje ya hecho
(`78_huecos_del_lenguaje_completo.md`) esto es una tarde de decisiones, no de implementación, y poda
la tabla.
*Terminado cuando:* existe una lista escrita de qué formas del lenguaje puede usar `nsc-next`, y
ninguna de ellas depende de algo que sólo `nsc-next` compilaría.

**Paso 1 — las cinco de la medición, en orden de uso.**
`StringView` (SHR-010) · `Optional<T>` (SHR-017) · `Span<T>`/`ReadOnlySpan<T>` (SHR-011/012) ·
filesystem compiler-grade (SHR-031) · `algorithm` (SHR-052).
Con **SHR-013 (escape checking) pegado a las tres primeras**, no después: una vista que sobrevive a
su dueño es un uso-después-de-liberar, y `borrowed` ya existe como maquinaria para expresarlo.

**Paso 2 — los dos bloqueos duros de proceso y binario.**
`Process.Run` (SHR-032, sin él no se enlaza) · `File.ReadAllBytes`/`WriteAllBytes` (SHR-028/029) ·
`ByteBuffer`/`BinaryReader`/`BinaryWriter` (SHR-025/026/027).

**Paso 3 — la corrección que queda del núcleo.**
IDs **122** y luego **121** (la aritmética de constantes, de una vez — no se puede escribir la regla
de promoción entera mientras la estructura que la alimenta tenga dos significados), **123**
(`stackalloc`), **124** (fixture de EH) y **77** (conteo en los logs).

**Paso 4 — arquitectura, y sólo lo que desbloquea.**
`SHR-090` (congelar el subconjunto de la API C de LLVM: hoy son **242 símbolos declarados a mano**
en `LlvmCApi.h`) y `SHR-089` (externalizar `WindowsRuntimeBuilder`). Sin estos dos, `nsc-next` no
puede hablar con LLVM ni dejar de sintetizar el runtime.

**Paso 5 — el resto de la stdlib**, ya podado por el perfil del paso 0.

**Paso 6 — concurrencia, con el contrato primero.** SHR-073/074 y luego `volatile` (SHR-075 / ID 87).

---

## 8. Lo que yo no haría

* **No empezar por `volatile`.** Retiro mi propia recomendación de esta mañana, por §1.
* **No escribir NSASM ni LIR todavía** (SHR-091). Un ensamblador propio antes de tener la frontera
  medida construye una segunda dependencia igual de enredada que la primera. La forma de avanzar es
  la que ya usé hoy sin querer: extraer **una función por regla del lenguaje** que tome valores ya
  bajados — `applyBinaryOperation` es el ejemplo. Cuando haya suficientes, la frontera se ve sola.
* **No planificar SHR-005 sin medirlo antes.** Ver §4.
* **No tocar `Result<T,E>`** en el camino crítico: no hay uso medido.
* **Y lo que el documento SHR ya dice y conviene repetir:** LINQ, `async`/`await`, records, pattern
  matching, `decimal`, tuplas, inicializadores de objeto, argumentos con nombre, `params`, arreglos
  multidimensionales, métodos de extensión, reflection y GC **no están en el camino**. Coincide
  exactamente con el corte de `78_huecos_del_lenguaje_completo.md` §1, hecho por separado.

---

## 9. Lo que hace falta decidir, y no es mío

Tres cosas de este documento son decisiones de producto, no de ingeniería, y las dejo dichas sin
tomarlas:

1. **¿El objetivo inmediato es el auto-hospedado, o un compilador utilizable?** Si es lo segundo, mi
   orden del CC2 vuelve a valer y `volatile` sube. Si es lo primero, manda §7. **No pueden mandar
   los dos.**
2. **UTF-8 (SHR-079).** La recomendación de `78`/CC2 sigue siendo: `char` es una **unidad de
   código** de un byte, `string` es UTF-8, y se documenta. La alternativa — `char` de 16 bits como
   C# — rompe la ABI de todo lo que ya cruza a C.
3. **Cuánta stdlib antes del primer bootstrap.** El paso 0 la contesta, y hasta que exista, cualquier
   número es inventado.
